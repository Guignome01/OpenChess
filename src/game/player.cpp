#include "player.h"
#include "game_controller.h"
#include "led_colors.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessPlayer::ChessPlayer(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc) : ChessGame(bd, wm, gc) {}

void ChessPlayer::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  if (!tryResumeGame())
    controller_->startNewGame(GameMode::CHESS_MOVES);
  waitForBoardSetup(controller_->getBoard());
}

void ChessPlayer::update() {
  boardDriver_->readSensors();

  if (processResign()) return;

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(controller_->currentTurn(), fromRow, fromCol, toRow, toCol))
    applyMove(fromRow, fromCol, toRow, toCol);

  boardDriver_->updateSensorPrev();
}
