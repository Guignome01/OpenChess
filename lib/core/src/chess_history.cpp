#include "chess_history.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include "chess_board.h"

// --- Compact 2-byte move encoding ---

static uint8_t promoCharToCode(char p) {
  switch (tolower(p)) {
    case 'q': return 1;
    case 'r': return 2;
    case 'b': return 3;
    case 'n': return 4;
    default:  return 0;
  }
}

static char promoCodeToChar(uint8_t code) {
  switch (code) {
    case 1: return 'q';
    case 2: return 'r';
    case 3: return 'b';
    case 4: return 'n';
    default: return ' ';
  }
}

uint16_t ChessHistory::encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  uint8_t from = (uint8_t)(fromRow * 8 + fromCol);
  uint8_t to = (uint8_t)(toRow * 8 + toCol);
  uint8_t promo = promoCharToCode(promotion);
  return (uint16_t)((from << 10) | (to << 4) | promo);
}

void ChessHistory::decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
  uint8_t from = (encoded >> 10) & 0x3F;
  uint8_t to = (encoded >> 4) & 0x3F;
  uint8_t promo = encoded & 0x0F;
  fromRow = from / 8;
  fromCol = from % 8;
  toRow = to / 8;
  toCol = to % 8;
  promotion = promoCodeToChar(promo);
}

// ---------------------------------------------------------------------------

ChessHistory::ChessHistory(IGameStorage* storage, ILogger* logger)
    : moveCount_(0),
      currentIndex_(-1),
      storage_(storage),
      logger_(logger),
      recordingActive_(false),
      headerInitialized_(false),
      movesSinceFlush_(0) {
  memset(&header_, 0, sizeof(header_));
}

void ChessHistory::clear() {
  moveCount_ = 0;
  currentIndex_ = -1;
}

// ---------------------------------------------------------------------------
// Move log with undo/redo
// ---------------------------------------------------------------------------

void ChessHistory::addMove(const MoveEntry& entry) {
  if (currentIndex_ + 1 >= MAX_MOVES) return;

  // Branch point: if cursor is not at the end, wipe future moves
  if (currentIndex_ < moveCount_ - 1) {
    int newCount = currentIndex_ + 1;
    // Truncate recording to match
    if (recordingActive_ && storage_) {
      storage_->truncateMoveData(newCount * 2);
      header_.moveCount = static_cast<uint16_t>(newCount);
      storage_->updateHeader(header_);
      movesSinceFlush_ = 0;
    }
    moveCount_ = newCount;
  }

  moves_[moveCount_] = entry;
  moveCount_++;
  currentIndex_ = moveCount_ - 1;

  // Persist to storage if recording is active
  if (recordingActive_ && storage_)
    persistMove(entry);
}

const MoveEntry* ChessHistory::undoMove() {
  if (currentIndex_ < 0) return nullptr;
  return &moves_[currentIndex_--];
}

const MoveEntry* ChessHistory::redoMove() {
  if (currentIndex_ >= moveCount_ - 1) return nullptr;
  return &moves_[++currentIndex_];
}

const MoveEntry& ChessHistory::getMove(int index) const {
  return moves_[index];
}

const MoveEntry& ChessHistory::lastMove() const {
  return moves_[currentIndex_];
}

// ---------------------------------------------------------------------------
// Recording (persistent storage)
// ---------------------------------------------------------------------------

void ChessHistory::setHeader(const GameHeader& header) {
  if (!storage_) return;

  // If already recording, discard previous game
  if (recordingActive_)
    storage_->discardGame();

  header_ = header;
  storage_->beginGame(header_);
  recordingActive_ = true;
  headerInitialized_ = true;
  movesSinceFlush_ = 0;

  if (logger_) logger_->info("ChessHistory: new recording started");
}

void ChessHistory::snapshotPosition(const std::string& fen) {
  if (!storage_ || !recordingActive_) return;

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

void ChessHistory::save(GameResult result, char winnerColor) {
  if (!storage_ || !recordingActive_) return;

  recordingActive_ = false;
  headerInitialized_ = false;
  header_.result = result;
  header_.winnerColor = winnerColor;

  storage_->finalizeGame(header_);
  storage_->enforceStorageLimits();

  if (logger_)
    logger_->infof("ChessHistory: recording saved (result=%d, moves=%d)", static_cast<int>(result), header_.moveCount);
}

void ChessHistory::discard() {
  recordingActive_ = false;
  headerInitialized_ = false;
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

  // Clear and repopulate in-memory move log during replay
  clear();

  int replayed = 0;
  for (int i = lastFenIdx + 1; i < static_cast<int>(entryCount); i++) {
    if (moves[i] == FEN_MARKER) continue;  // skip intermediate FEN markers
    int fromRow, fromCol, toRow, toCol;
    char promotion;
    decodeMove(moves[i], fromRow, fromCol, toRow, toCol, promotion);

    // Capture pre-move state for the MoveEntry
    char piece = board.getSquare(fromRow, fromCol);
    char targetPiece = board.getSquare(toRow, toCol);
    PositionState prevState = board.positionState();

    MoveResult moveResult = board.makeMove(fromRow, fromCol, toRow, toCol, promotion);
    if (!moveResult.valid) {
      if (logger_) logger_->errorf("ChessHistory: invalid move at entry %d during replay", i);
      return false;
    }

    // Build MoveEntry and add to in-memory log (but don't persist — it's already on disk)
    char captured = ' ';
    if (moveResult.isEnPassant)
      captured = (piece >= 'A' && piece <= 'Z') ? 'p' : 'P';
    else if (moveResult.isCapture)
      captured = targetPiece;

    MoveEntry entry;
    entry.fromRow = fromRow;
    entry.fromCol = fromCol;
    entry.toRow = toRow;
    entry.toCol = toCol;
    entry.piece = piece;
    entry.captured = captured;
    entry.promotion = moveResult.isPromotion ? moveResult.promotedTo : ' ';
    entry.isCapture = moveResult.isCapture;
    entry.isEnPassant = moveResult.isEnPassant;
    entry.epCapturedRow = moveResult.epCapturedRow;
    entry.isCastling = moveResult.isCastling;
    entry.isPromotion = moveResult.isPromotion;
    entry.isCheck = moveResult.isCheck;
    entry.prevState = prevState;

    // Add to in-memory log only (bypass persistMove by direct insertion)
    if (moveCount_ < MAX_MOVES) {
      moves_[moveCount_++] = entry;
      currentIndex_ = moveCount_ - 1;
    }

    replayed++;
  }

  // Restore header for continued recording
  header_ = hdr;
  recordingActive_ = true;
  headerInitialized_ = true;

  if (logger_)
    logger_->infof("ChessHistory: replayed %d moves from last FEN marker, game resumed", replayed);

  return true;
}

// ---------------------------------------------------------------------------
// Internal: persist a single move to storage
// ---------------------------------------------------------------------------

void ChessHistory::persistMove(const MoveEntry& entry) {
  char promo = entry.isPromotion ? entry.promotion : ' ';
  uint16_t encoded = encodeMove(entry.fromRow, entry.fromCol,
                                entry.toRow, entry.toCol, promo);
  storage_->appendMoveData(reinterpret_cast<const uint8_t*>(&encoded), 2);
  header_.moveCount++;
  movesSinceFlush_++;

  // Flush header every full turn (2 half-moves) to reduce flash wear
  if (movesSinceFlush_ >= 2) {
    storage_->updateHeader(header_);
    movesSinceFlush_ = 0;
  }
}
