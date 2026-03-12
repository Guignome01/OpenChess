#include "player_mode.h"
#include "chess_game.h"
#include <Arduino.h>

PlayerMode::PlayerMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* cg, ILogger* logger) : GameMode(bd, wm, cg, logger) {}

void PlayerMode::begin() {
  logger_.info("=== Starting Chess Moves Mode ===");
  if (!tryResumeGame())
    chess_->startNewGame(GameModeId::CHESS_MOVES);
  waitForBoardSetup();
}

void PlayerMode::update() {
  boardDriver_->readSensors();

  if (processResign()) return;

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(chess_->currentTurn(), fromRow, fromCol, toRow, toCol))
    applyMove(fromRow, fromCol, toRow, toCol);

  boardDriver_->updateSensorPrev();
}
