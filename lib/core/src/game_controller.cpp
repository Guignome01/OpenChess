#include "game_controller.h"

GameController::GameController(GameRecorder* recorder, IGameObserver* observer)
    : recorder_(recorder), observer_(observer) {}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void GameController::newGame() {
  board_.newGame();
  notifyObserver();
}

void GameController::startNewGame(GameMode mode, uint8_t playerColor, uint8_t botDepth) {
  newGame();
  startRecording(mode, playerColor, botDepth);
}

void GameController::startRecording(GameMode mode, uint8_t playerColor, uint8_t botDepth) {
  if (recorder_) {
    recorder_->startRecording(mode, playerColor, botDepth);
    recorder_->recordFen(board_.getFen());
  }
}

void GameController::endGame(GameResult result, char winnerColor) {
  if (board_.isGameOver()) return;  // Guard against double-call

  if (recorder_ && recorder_->isRecording())
    recorder_->finishRecording(result, winnerColor);
  board_.endGame(result, winnerColor);
  notifyObserver();
}

void GameController::discardRecording() {
  if (recorder_) recorder_->discardRecording();
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

MoveResult GameController::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  MoveResult result = board_.makeMove(fromRow, fromCol, toRow, toCol, promotion);
  if (!result.valid) return result;

  // Record the move
  if (recorder_ && recorder_->isRecording())
    recorder_->recordMove(fromRow, fromCol, toRow, toCol,
                          result.isPromotion ? result.promotedTo : promotion);

  // Auto-finish recording on game end
  if (result.gameResult != GameResult::IN_PROGRESS && recorder_ && recorder_->isRecording())
    recorder_->finishRecording(result.gameResult, result.winnerColor);

  notifyObserver();
  return result;
}

bool GameController::loadFEN(const std::string& fen) {
  if (!board_.loadFEN(fen))
    return false;

  if (recorder_ && recorder_->isRecording())
    recorder_->recordFen(fen);

  notifyObserver();
  return true;
}

// ---------------------------------------------------------------------------
// Replay
// ---------------------------------------------------------------------------

bool GameController::resumeGame() {
  if (!recorder_) return false;
  bool ok = recorder_->replayInto(board_);
  if (ok) notifyObserver();
  return ok;
}

// ---------------------------------------------------------------------------
// Resume queries
// ---------------------------------------------------------------------------

bool GameController::hasActiveGame() {
  if (!recorder_) return false;
  return recorder_->hasActiveGame();
}

bool GameController::getActiveGameInfo(GameMode& mode, uint8_t& playerColor, uint8_t& botDepth) {
  if (!recorder_) return false;
  return recorder_->getActiveGameInfo(mode, playerColor, botDepth);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void GameController::notifyObserver() {
  if (observer_)
    observer_->onBoardStateChanged(board_.getFen(), board_.getEvaluation());
}
