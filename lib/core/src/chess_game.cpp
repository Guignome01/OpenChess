#include "chess_game.h"

#include <cstring>

#include "chess_codec.h"
#include "chess_rules.h"

ChessGame::ChessGame(IGameStorage* storage, IGameObserver* observer, ILogger* logger)
    : history_(storage, logger), observer_(observer), batchDepth_(0), batchDirty_(false) {}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessGame::newGame() {
  board_.newGame();
  history_.clear();
  fireCallback();
  notifyObserver();
}

void ChessGame::startNewGame(GameModeId mode, uint8_t playerColor, uint8_t botDepth) {
  newGame();
  startRecording(mode, playerColor, botDepth);
}

void ChessGame::startRecording(GameModeId mode, uint8_t playerColor, uint8_t botDepth) {
  history_.startRecording(mode, playerColor, botDepth);
  if (history_.isRecording())
    history_.recordFen(board_.getFen());
}

void ChessGame::endGame(GameResult result, char winnerColor) {
  if (board_.isGameOver()) return;  // Guard against double-call

  if (history_.isRecording())
    history_.finishRecording(result, winnerColor);
  board_.endGame(result, winnerColor);
  fireCallback();
  notifyObserver();
}

void ChessGame::discardRecording() {
  history_.discardRecording();
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

  // Record move in history
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

  // Record with persistent storage
  if (history_.isRecording())
    history_.recordMove(fromRow, fromCol, toRow, toCol,
                        result.isPromotion ? result.promotedTo : promotion);

  // Auto-finish recording on game end
  if (result.gameResult != GameResult::IN_PROGRESS && history_.isRecording())
    history_.finishRecording(result.gameResult, result.winnerColor);

  fireCallback();
  notifyObserver();
  return result;
}

MoveResult ChessGame::makeMove(const std::string& uci) {
  int fromRow, fromCol, toRow, toCol;
  char promotion = ' ';
  if (!ChessCodec::parseUCIMove(uci, fromRow, fromCol, toRow, toCol, promotion))
    return invalidMoveResult();
  return makeMove(fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::loadFEN(const std::string& fen) {
  if (!board_.loadFEN(fen))
    return false;

  history_.clear();

  if (history_.isRecording())
    history_.recordFen(fen);

  fireCallback();
  notifyObserver();
  return true;
}

// ---------------------------------------------------------------------------
// UCI helpers
// ---------------------------------------------------------------------------

std::string ChessGame::toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  return ChessCodec::toUCIMove(fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::parseUCIMove(const std::string& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
  return ChessCodec::parseUCIMove(move, fromRow, fromCol, toRow, toCol, promotion);
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

bool ChessGame::resumeGame() {
  beginBatch();
  bool ok = history_.replayInto(board_);
  if (ok) {
    // Clear in-memory move log — only the storage has the full history.
    // Board already tracks position history from replay moves.
    history_.clear();
  }
  endBatch();
  if (ok) notifyObserver();
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
  if (observer_)
    observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
}
