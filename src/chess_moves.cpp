#include "chess_moves.h"
#include "chess_utils.h"
#include "led_colors.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessMoves::ChessMoves(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm)
    : ChessGame(bd, ce, wm) {}

void ChessMoves::begin() {
  Serial.println("=== Starting Chess Moves Mode ===");
  initializeBoard();
  waitForBoardSetup();
}

void ChessMoves::update() {
  boardDriver->readSensors();

  int fromRow, fromCol, toRow, toCol;
  char piece;
  if (tryPlayerMove(currentTurn, fromRow, fromCol, toRow, toCol, piece)) {
    processPlayerMove(fromRow, fromCol, toRow, toCol, piece);
    updateGameStatus();
    wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
  }

  boardDriver->updateSensorPrev();
}
