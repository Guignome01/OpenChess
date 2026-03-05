#include "chess_game.h"

#include <cstring>

#include "chess_notation.h"
#include "chess_rules.h"

ChessGame::ChessGame(IGameStorage* storage, IGameObserver* observer, ILogger* logger)
    : history_(storage, logger), observer_(observer), batchDepth_(0), batchDirty_(false) {}

static const char* STANDARD_START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessGame::newGame() {
  board_.newGame();
  history_.clear();
  startFen_ = STANDARD_START_FEN;
  fireCallback();
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
  if (board_.isGameOver()) return;  // Guard against double-call

  history_.save(result, winnerColor);  // no-op if not recording
  board_.endGame(result, winnerColor);
  fireCallback();
  notifyObserver();
}

void ChessGame::discardRecording() {
  history_.discard();
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

MoveResult ChessGame::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (board_.isGameOver()) return invalidMoveResult();

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
  char captured = ' ';
  if (result.isEnPassant) {
    captured = ChessUtils::isWhitePiece(piece) ? 'p' : 'P';
  } else if (result.isCapture) {
    captured = targetPiece;
  }

  MoveEntry entry;
  entry.fromRow = fromRow;
  entry.fromCol = fromCol;
  entry.toRow = toRow;
  entry.toCol = toCol;
  entry.piece = piece;
  entry.captured = captured;
  entry.promotion = result.isPromotion ? result.promotedTo : ' ';
  entry.isCapture = result.isCapture;
  entry.isEnPassant = result.isEnPassant;
  entry.epCapturedRow = result.epCapturedRow;
  entry.isCastling = result.isCastling;
  entry.isPromotion = result.isPromotion;
  entry.isCheck = result.isCheck;
  entry.prevState = prevState;
  history_.addMove(entry);

  // Auto-save recording on game end
  if (result.gameResult != GameResult::IN_PROGRESS)
    history_.save(result.gameResult, result.winnerColor);

  fireCallback();
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
  history_.snapshotPosition(fen);  // no-op if not recording

  fireCallback();
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
  fireCallback();
  notifyObserver();
  return true;
}

bool ChessGame::redoMove() {
  const MoveEntry* entry = history_.redoMove();
  if (!entry) return false;
  board_.applyMoveEntry(*entry);
  fireCallback();
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
    startFen_ = board_.getFen();
    // Walk back to find the real start FEN: if history has moves,
    // the board is at the end of replay; startFen_ should be the
    // position before any moves.  replayInto loads from the last
    // FEN snapshot, so approximate with what the board looked like
    // before replay.  For accuracy, undo all moves to find the start.
    // Actually, replayInto loads the FEN first then replays — the
    // startFen_ is the FEN that replayInto loaded.  We can get it
    // by undoing all moves temporarily.
    // Simpler: just record the FEN from the board state before moves,
    // but replayInto already consumed moves.  We can reconstruct by
    // undoing all moves in a temp copy.  However, the replay FEN is
    // already the initial FEN of the segment.  Let's get it properly:
    if (history_.moveCount() > 0) {
      // The start FEN is the position before the first move.
      // Reverse the first move to retrieve it.
      ChessBoard temp = board_;
      for (int i = history_.moveCount() - 1; i >= 0; --i)
        temp.reverseMove(history_.getMove(i));
      startFen_ = temp.getFen();
    }
    fireCallback();
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
  if (board_.positionState().halfmoveClock >= 100) return true;
  if (board_.isThreefoldRepetition()) return true;
  if (board_.isInsufficientMaterial()) return true;
  return false;
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
    if (stateCallback_) stateCallback_();
    if (observer_)
      observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
  }
}

// ---------------------------------------------------------------------------
// Internal: callback dispatch
// ---------------------------------------------------------------------------

void ChessGame::fireCallback() {
  if (batchDepth_ > 0) {
    batchDirty_ = true;
    return;
  }
  if (stateCallback_) stateCallback_();
}

void ChessGame::notifyObserver() {
  if (batchDepth_ > 0) {
    batchDirty_ = true;
    return;
  }
  if (observer_)
    observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
}
