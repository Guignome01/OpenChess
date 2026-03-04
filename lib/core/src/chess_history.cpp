#include "chess_history.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "chess_board.h"
#include "chess_codec.h"

ChessHistory::ChessHistory(IGameStorage* storage, ILogger* logger)
    : moveCount_(0),
      storage_(storage),
      logger_(logger),
      recording_(false),
      movesSinceFlush_(0) {
  memset(&header_, 0, sizeof(header_));
}

void ChessHistory::clear() {
  moveCount_ = 0;
}

// ---------------------------------------------------------------------------
// Move log
// ---------------------------------------------------------------------------

void ChessHistory::addMove(const MoveEntry& entry) {
  if (moveCount_ < MAX_MOVES) {
    moves_[moveCount_++] = entry;
  }
}

bool ChessHistory::popMove(MoveEntry& entry) {
  if (moveCount_ <= 0) return false;
  entry = moves_[--moveCount_];
  return true;
}

const MoveEntry& ChessHistory::getMove(int index) const {
  return moves_[index];
}

const MoveEntry& ChessHistory::lastMove() const {
  return moves_[moveCount_ - 1];
}

// ---------------------------------------------------------------------------
// Recording lifecycle (persistent storage)
// ---------------------------------------------------------------------------

void ChessHistory::startRecording(GameModeId mode, uint8_t playerColor, uint8_t botDepth) {
  if (!storage_) return;

  // Discard any leftover live game
  discardRecording();

  memset(&header_, 0, sizeof(header_));
  header_.version = FORMAT_VERSION;
  header_.mode = mode;
  header_.result = GameResult::IN_PROGRESS;
  header_.winnerColor = '?';
  header_.playerColor = playerColor;
  header_.botDepth = botDepth;
  // Timestamp is set by the storage layer (firmware has NTP access)

  storage_->beginGame(header_);
  recording_ = true;
  movesSinceFlush_ = 0;

  if (logger_) logger_->info("ChessHistory: new recording started");
}

void ChessHistory::recordMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (!storage_ || !recording_) return;

  uint16_t encoded = ChessCodec::encodeMove(fromRow, fromCol, toRow, toCol, promotion);
  storage_->appendMoveData(reinterpret_cast<const uint8_t*>(&encoded), 2);
  header_.moveCount++;
  movesSinceFlush_++;

  // Flush header every full turn (2 half-moves) to reduce flash wear
  if (movesSinceFlush_ >= 2) {
    storage_->updateHeader(header_);
    movesSinceFlush_ = 0;
  }
}

void ChessHistory::recordFen(const std::string& fen) {
  if (!storage_ || !recording_) return;

  // Write FEN_MARKER to the move stream
  uint16_t marker = FEN_MARKER;
  storage_->appendMoveData(reinterpret_cast<const uint8_t*>(&marker), 2);
  header_.moveCount++;

  // Write FEN to the FEN table — storage returns the byte offset
  header_.lastFenOffset = static_cast<uint16_t>(storage_->appendFenEntry(fen));
  header_.fenEntryCnt++;

  storage_->updateHeader(header_);
  movesSinceFlush_ = 0;
}

void ChessHistory::finishRecording(GameResult result, char winnerColor) {
  if (!storage_ || !recording_) return;

  recording_ = false;
  header_.result = result;
  header_.winnerColor = winnerColor;

  storage_->finalizeGame(header_);
  storage_->enforceStorageLimits();

  if (logger_)
    logger_->infof("ChessHistory: recording finished (result=%d, moves=%d)", static_cast<int>(result), header_.moveCount);
}

void ChessHistory::discardRecording() {
  recording_ = false;
  if (storage_) storage_->discardGame();
}

// ---------------------------------------------------------------------------
// State queries (persistent storage)
// ---------------------------------------------------------------------------

bool ChessHistory::hasActiveGame() {
  if (!storage_) return false;
  return storage_->hasActiveGame();
}

bool ChessHistory::getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth) {
  if (!storage_) return false;

  GameHeader hdr;
  if (!storage_->readHeader(hdr)) return false;
  if (hdr.version != FORMAT_VERSION) return false;

  mode = hdr.mode;
  playerColor = hdr.playerColor;
  botDepth = hdr.botDepth;
  return true;
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

bool ChessHistory::replayInto(ChessBoard& board) {
  if (!storage_) return false;

  // Read header from live file
  GameHeader hdr;
  if (!storage_->readHeader(hdr)) return false;
  if (hdr.version != FORMAT_VERSION) {
    if (logger_) logger_->error("ChessHistory: incompatible format version");
    return false;
  }
  if (hdr.fenEntryCnt == 0) {
    if (logger_) logger_->error("ChessHistory: no FEN in live game, cannot resume");
    return false;
  }

  // Read all 2-byte move entries
  std::vector<uint8_t> moveData;
  if (!storage_->readMoveData(moveData)) return false;

  size_t entryCount = moveData.size() / 2;

  // Read last FEN
  std::string lastFen;
  if (!storage_->readFenAt(hdr.lastFenOffset, lastFen) || lastFen.empty()) {
    if (logger_) logger_->error("ChessHistory: failed to read last FEN");
    return false;
  }

  // Find last FEN marker in move stream (scan backwards)
  const uint16_t* moves = reinterpret_cast<const uint16_t*>(moveData.data());
  int lastFenIdx = -1;
  for (int i = static_cast<int>(entryCount) - 1; i >= 0; i--) {
    if (moves[i] == FEN_MARKER) {
      lastFenIdx = i;
      break;
    }
  }

  if (lastFenIdx < 0) {
    if (logger_) logger_->error("ChessHistory: FEN marker not found in moves");
    return false;
  }

  if (logger_) logger_->infof("ChessHistory: resuming from FEN: %s", lastFen.c_str());

  // Load FEN into board, then replay subsequent moves
  if (!board.loadFEN(lastFen)) {
    if (logger_) logger_->error("ChessHistory: invalid FEN in recording");
    return false;
  }

  int replayed = 0;
  for (int i = lastFenIdx + 1; i < static_cast<int>(entryCount); i++) {
    if (moves[i] == FEN_MARKER) continue;  // skip intermediate FEN markers
    int fromRow, fromCol, toRow, toCol;
    char promotion;
    ChessCodec::decodeMove(moves[i], fromRow, fromCol, toRow, toCol, promotion);
    MoveResult moveResult = board.makeMove(fromRow, fromCol, toRow, toCol, promotion);
    if (!moveResult.valid) {
      if (logger_) logger_->errorf("ChessHistory: invalid move at entry %d during replay", i);
      return false;
    }
    replayed++;
  }

  // Restore header for continued recording
  header_ = hdr;
  recording_ = true;

  if (logger_)
    logger_->infof("ChessHistory: replayed %d moves from last FEN marker, game resumed", replayed);

  return true;
}
