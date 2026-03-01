#include "bot.h"
#include "led_colors.h"
#include "move_history.h"
#include "system_utils.h"
#include "utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessBot::ChessBot(BoardDriver* bd, WiFiManagerESP32* wm, MoveHistory* mh, char playerClr)
    : ChessGame(bd, wm, mh), playerColor(playerClr), thinkingAnimation(nullptr) {}

// --- Template Method: game loop skeleton ---
// Player turn: tryPlayerMove → applyMove → onPlayerMoveApplied hook.
// Engine turn: requestEngineMove (subclass drives the move).

void ChessBot::update() {
  if (gm_.isGameOver()) return;

  boardDriver->readSensors();

  if (processResign()) return;

  bool isPlayerTurn = (gm_.currentTurn() == playerColor);

  if (isPlayerTurn) {
    int fromRow, fromCol, toRow, toCol;
    if (tryPlayerMove(playerColor, fromRow, fromCol, toRow, toCol)) {
      MoveResult result = applyMove(fromRow, fromCol, toRow, toCol);
      wifiManager->updateBoardState(gm_.getFen(), getEngineEvaluation());
      onPlayerMoveApplied(result, fromRow, fromCol, toRow, toCol);
    }
  } else {
    requestEngineMove();
    if (!gm_.isGameOver())
      wifiManager->updateBoardState(gm_.getFen(), getEngineEvaluation());
  }

  boardDriver->updateSensorPrev();
}

// --- Resign with thinking-animation management ---

bool ChessBot::handleResign(char resignColor) {
  bool wasThinking = (thinkingAnimation != nullptr);
  if (wasThinking) stopThinking();

  bool flipped = (playerColor == 'b');

  Serial.printf("Engine resign confirmation for %s...\n", ChessUtils::colorName(resignColor));

  if (!boardConfirm(boardDriver, flipped)) {
    Serial.println("Resign cancelled");
    if (wasThinking && gm_.currentTurn() != playerColor && !gm_.isGameOver())
      startThinking();
    return false;
  }

  char winnerColor = (resignColor == 'w') ? 'b' : 'w';
  Serial.printf("RESIGNATION! %s resigns. %s wins!\n", ChessUtils::colorName(resignColor), ChessUtils::colorName(winnerColor));

  boardDriver->fireworkAnimation(SystemUtils::colorLed(winnerColor));
  if (moveHistory) moveHistory->finishGame(RESULT_RESIGNATION, winnerColor);
  gm_.endGame(RESULT_RESIGNATION, winnerColor);
  return true;
}

// --- Thinking animation helpers ---

void ChessBot::startThinking() {
  boardDriver->waitForAnimationQueueDrain();
  thinkingAnimation = boardDriver->startThinkingAnimation();
}

void ChessBot::stopThinking() {
  if (thinkingAnimation) {
    boardDriver->stopAndWaitForAnimation(thinkingAnimation);
    thinkingAnimation = nullptr;
  }
}

// --- Remote move guidance (LED + sensor blocking) ---
// Shared by all engine subclasses. Guides the local player to physically
// execute a move made by the remote engine on the board.

void ChessBot::waitForRemoteMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant, int enPassantCapturedPawnRow) {
  BoardDriver::LedGuard guard(boardDriver);
  boardDriver->clearAllLEDs(false);
  // Show source square (where to pick up from)
  boardDriver->setSquareLED(fromRow, fromCol, LedColors::Cyan);
  // Show destination square (where to place)
  if (isCapture)
    boardDriver->setSquareLED(toRow, toCol, LedColors::Red);
  else
    boardDriver->setSquareLED(toRow, toCol, LedColors::White);
  if (isEnPassant)
    boardDriver->setSquareLED(enPassantCapturedPawnRow, toCol, LedColors::Purple);
  boardDriver->showLEDs();

  bool piecePickedUp = false;
  bool capturedPieceRemoved = false;
  bool moveCompleted = false;

  Serial.println("Waiting for you to complete the remote move...");

  while (!moveCompleted) {
    boardDriver->readSensors();

    // For capture moves, ensure captured piece is removed first
    // For en passant, check the actual captured pawn square (not the destination)
    if (isCapture && !capturedPieceRemoved) {
      int captureCheckRow = isEnPassant ? enPassantCapturedPawnRow : toRow;
      if (!boardDriver->getSensorState(captureCheckRow, toCol)) {
        capturedPieceRemoved = true;
        if (isEnPassant)
          Serial.println("En passant captured pawn removed, now complete the move...");
        else
          Serial.println("Captured piece removed, now complete the move...");
      }
    }

    // Check if piece was picked up from source
    if (!piecePickedUp && !boardDriver->getSensorState(fromRow, fromCol)) {
      piecePickedUp = true;
      Serial.println("Piece picked up, now place it on the destination...");
    }

    // Check if piece was placed on destination
    // For captures: wait until captured piece is removed AND piece is placed
    // For normal moves: just wait for piece to be placed
    if (piecePickedUp && boardDriver->getSensorState(toRow, toCol))
      if (!isCapture || (isCapture && capturedPieceRemoved)) {
        moveCompleted = true;
        Serial.println("Move completed on physical board!");
      }

    delay(SENSOR_READ_DELAY_MS);
    boardDriver->updateSensorPrev();
  }

  boardDriver->clearAllLEDs();
}  // LedGuard released
