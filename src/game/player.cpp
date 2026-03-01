#include "player.h"
#include "led_colors.h"
#include "move_history.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessPlayer::ChessPlayer(BoardDriver* bd, WiFiManagerESP32* wm, MoveHistory* mh) : ChessGame(bd, wm, mh) {}

void ChessPlayer::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  initializeBoard();
  if (!tryResumeGame()) {
    moveHistory->startGame(GAME_MODE_CHESS_MOVES);
    moveHistory->addFen(gm_.getFen());
  }
  waitForBoardSetup(gm_.getBoard());
}

void ChessPlayer::update() {
  boardDriver->readSensors();

  if (processResign()) return;

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(gm_.currentTurn(), fromRow, fromCol, toRow, toCol)) {
    applyMove(fromRow, fromCol, toRow, toCol);
    wifiManager->updateBoardState(gm_.getFen(), gm_.getEvaluation());
  }

  boardDriver->updateSensorPrev();
}
