#include "chess_game.h"

#include <cstring>

#include "chess_notation.h"
#include "chess_rules.h"

ChessGame::ChessGame(IGameStorage* storage, IGameObserver* observer, ILogger* logger)
    : history_(storage, logger), observer_(observer), batchDepth_(0), batchDirty_(false),
      gameOver_(false), gameResult_(GameResult::IN_PROGRESS), winnerColor_(' ') {}

static const char* STANDARD_START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessGame::newGame() {
  board_.newGame();
  history_.clear();
  startFen_ = STANDARD_START_FEN;
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';
  notifyObserver();
}

void ChessGame::startNewGame(GameModeId mode, uint8_t playerColor, uint8_t botDepth) {
  newGame();

  // Build header and start recording
  GameHeader header;
  memset(&header, 0, sizeof(header));
  header.version = FORMAT_VERSION;
  header.mode = mode;
  header.result = GameResult::IN_PROGRESS;
  header.winnerColor = '?';
  header.playerColor = playerColor;
  header.botDepth = botDepth;
  history_.setHeader(header);  // creates live file, no-op if no storage
  history_.snapshotPosition(board_.getFen());  // record initial FEN
}

void ChessGame::endGame(GameResult result, char winnerColor) {
  if (gameOver_) return;  // Guard against double-call

  gameOver_ = true;
  gameResult_ = result;
  winnerColor_ = winnerColor;
  history_.save(result, winnerColor);  // no-op if not recording
  notifyObserver();
}

void ChessGame::discardRecording() {
  history_.discard();
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

MoveResult ChessGame::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (gameOver_) return invalidMoveResult();

  // Save pre-move state for history
  char piece = board_.getSquare(fromRow, fromCol);
  char targetPiece = board_.getSquare(toRow, toCol);
  PositionState prevState = board_.positionState();

  // Delegate move validation, application, and all end-condition detection to board
  // (checkmate, stalemate, 50-move, insufficient material, threefold repetition)
  MoveResult result = board_.makeMove(fromRow, fromCol, toRow, toCol, promotion);
  if (!result.valid) return result;

  // Build MoveEntry and record in history (addMove handles both in-memory log
  // and persistent recording automatically)
  MoveEntry entry = buildMoveEntry(fromRow, fromCol, toRow, toCol, piece, targetPiece, result, prevState);
  history_.addMove(entry);

  // Auto-end game on checkmate/stalemate/draw
  if (result.gameResult != GameResult::IN_PROGRESS)
    endGame(result.gameResult, result.winnerColor);  // calls notifyObserver()
  else
    notifyObserver();

  return result;
}

MoveResult ChessGame::makeMove(const std::string& move) {
  int fromRow, fromCol, toRow, toCol;
  char promotion = ' ';
  if (!ChessNotation::parseCoordinate(move, fromRow, fromCol, toRow, toCol, promotion))
    return invalidMoveResult();
  return makeMove(fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::loadFEN(const std::string& fen) {
  if (!board_.loadFEN(fen))
    return false;

  history_.clear();
  startFen_ = fen;
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';
  history_.snapshotPosition(fen);  // no-op if not recording

  notifyObserver();
  return true;
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

bool ChessGame::undoMove() {
  const MoveEntry* entry = history_.undoMove();
  if (!entry) return false;
  board_.reverseMove(*entry);
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';
  notifyObserver();
  return true;
}

bool ChessGame::redoMove() {
  const MoveEntry* entry = history_.redoMove();
  if (!entry) return false;
  MoveResult result = board_.applyMoveEntry(*entry);
  if (!result.valid) {
    // Shouldn't happen with a valid history, but undo the cursor advance
    history_.undoMove();
    return false;
  }
  if (result.gameResult != GameResult::IN_PROGRESS) {
    gameOver_ = true;
    gameResult_ = result.gameResult;
    winnerColor_ = result.winnerColor;
  }
  notifyObserver();
  return true;
}

// ---------------------------------------------------------------------------
// History queries
// ---------------------------------------------------------------------------

int ChessGame::getHistory(std::string out[], int maxMoves, MoveFormat format) const {
  int count = history_.moveCount();
  if (count > maxMoves) count = maxMoves;

  if (format == MoveFormat::COORDINATE) {
    // Fast path — no board replay needed
    for (int i = 0; i < count; ++i) {
      const MoveEntry& m = history_.getMove(i);
      char promo = m.isPromotion ? m.promotion : ' ';
      out[i] = ChessNotation::toCoordinate(m.fromRow, m.fromCol, m.toRow, m.toCol, promo);
    }
    return count;
  }

  // SAN / LAN — replay moves through a temporary board for context
  ChessBoard tempBoard;
  if (!startFen_.empty()) {
    tempBoard.loadFEN(startFen_);
  } else {
    tempBoard.newGame();
  }

  for (int i = 0; i < count; ++i) {
    const MoveEntry& m = history_.getMove(i);

    // Generate notation before applying the move
    if (format == MoveFormat::SAN) {
      out[i] = ChessNotation::toSAN(tempBoard.getBoard(), tempBoard.positionState(), m);
    } else {
      out[i] = ChessNotation::toLAN(m);
    }

    // Apply the move to advance the temp board
    tempBoard.applyMoveEntry(m);

    // Append check/checkmate suffix
    if (m.isCheck) {
      out[i] += tempBoard.isCheckmate() ? '#' : '+';
    }
  }

  return count;
}

// ---------------------------------------------------------------------------
// Notation helpers
// ---------------------------------------------------------------------------

std::string ChessGame::toCoordinate(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  return ChessNotation::toCoordinate(fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::parseCoordinate(const std::string& move, int& fromRow, int& fromCol,
                                int& toRow, int& toCol, char& promotion) {
  return ChessNotation::parseCoordinate(move, fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::parseMove(const std::string& move, int& fromRow, int& fromCol,
                          int& toRow, int& toCol, char& promotion) const {
  return ChessNotation::parseMove(board_.getBoard(), board_.positionState(),
                                  board_.currentTurn(),
                                  move, fromRow, fromCol, toRow, toCol, promotion);
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

bool ChessGame::resumeGame() {
  bool ok = history_.replayInto(board_);
  if (ok) {
    // Use the FEN that replayInto() loaded as the start position
    startFen_ = history_.replayFen();

    // Restore game-over state from the recording header
    GameResult hdrResult = history_.headerResult();
    if (hdrResult != GameResult::IN_PROGRESS) {
      gameOver_ = true;
      gameResult_ = hdrResult;
      winnerColor_ = history_.headerWinnerColor();
    }

    notifyObserver();
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Resume queries
// ---------------------------------------------------------------------------

bool ChessGame::hasActiveGame() {
  return history_.hasActiveGame();
}

bool ChessGame::getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth) {
  return history_.getActiveGameInfo(mode, playerColor, botDepth);
}

// ---------------------------------------------------------------------------
// Draw queries
// ---------------------------------------------------------------------------

bool ChessGame::isDraw() const {
  return board_.isDraw();
}

bool ChessGame::isThreefoldRepetition() const {
  return board_.isThreefoldRepetition();
}

// ---------------------------------------------------------------------------
// Batching
// ---------------------------------------------------------------------------

void ChessGame::beginBatch() {
  ++batchDepth_;
}

void ChessGame::endBatch() {
  if (batchDepth_ > 0) --batchDepth_;
  if (batchDepth_ == 0 && batchDirty_) {
    batchDirty_ = false;
    if (observer_)
      observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
  }
}

// ---------------------------------------------------------------------------
// Internal: observer dispatch
// ---------------------------------------------------------------------------

void ChessGame::notifyObserver() {
  if (batchDepth_ > 0) {
    batchDirty_ = true;
    return;
  }
  if (observer_)
    observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
}
