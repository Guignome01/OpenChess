#include "chess_moves.h"
#include "chess_utils.h"
#include "led_colors.h"
#include "move_history.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessMoves::ChessMoves(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, MoveHistory* mh) : ChessGame(bd, ce, wm, mh) {}

void ChessMoves::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  initializeBoard();
  if (moveHistory->hasLiveGame()) {
    Serial.println("Resuming live game...");
    replaying = true;
    moveHistory->replayIntoGame(this);
    replaying = false;
    wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
  } else {
    moveHistory->startGame(GAME_MODE_CHESS_MOVES);
    moveHistory->addFen(ChessUtils::boardToFEN(board, currentTurn, chessEngine));
  }
  waitForBoardSetup(board);
}

void ChessMoves::update() {
  boardDriver->readSensors();

  int fromRow, fromCol, toRow, toCol;
  if (tryPlayerMove(currentTurn, fromRow, fromCol, toRow, toCol)) {
    applyMove(fromRow, fromCol, toRow, toCol);
    updateGameStatus();
    wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
  }

  boardDriver->updateSensorPrev();
}
