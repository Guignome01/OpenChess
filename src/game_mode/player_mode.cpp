#include "player_mode.h"
#include "chess_game.h"
#include <Arduino.h>

PlayerMode::PlayerMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* cg) : GameMode(bd, wm, cg) {}

void PlayerMode::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  if (!tryResumeGame())
    chess_->startNewGame(GameModeId::CHESS_MOVES);
  waitForBoardSetup(chess_->getBoard());
}

void PlayerMode::update() {
  boardDriver_->readSensors();

  if (processResign()) return;

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(chess_->currentTurn(), fromRow, fromCol, toRow, toCol))
    applyMove(fromRow, fromCol, toRow, toCol);

  boardDriver_->updateSensorPrev();
}
