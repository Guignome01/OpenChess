#include "player_mode.h"
#include "chess_game.h"
#include <Arduino.h>

PlayerMode::PlayerMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc) : GameMode(bd, wm, gc) {}

void PlayerMode::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  if (!tryResumeGame())
    controller_->startNewGame(GameModeId::CHESS_MOVES);
  waitForBoardSetup(controller_->getBoard());
}

void PlayerMode::update() {
  boardDriver_->readSensors();

  if (processResign()) return;

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(controller_->currentTurn(), fromRow, fromCol, toRow, toCol))
    applyMove(fromRow, fromCol, toRow, toCol);

  boardDriver_->updateSensorPrev();
}
