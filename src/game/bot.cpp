#include "bot.h"
#include "game_controller.h"
#include "led_colors.h"
#include "system_utils.h"
#include "utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessBot::ChessBot(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc, char playerClr)
    : ChessGame(bd, wm, gc), playerColor(playerClr), thinkingAnimation(nullptr), wasThinkingBeforeResign_(false) {}

float ChessBot::getEngineEvaluation() { return controller_->getEvaluation(); }

// --- Template Method: game loop skeleton ---
// Player turn: tryPlayerMove → applyMove → onPlayerMoveApplied hook.
// Engine turn: requestEngineMove (subclass drives the move).

void ChessBot::update() {
  if (controller_->isGameOver()) return;

  boardDriver->readSensors();

  if (processResign()) return;

  bool isPlayerTurn = (controller_->currentTurn() == playerColor);

  if (isPlayerTurn) {
    int fromRow, fromCol, toRow, toCol;
    if (tryPlayerMove(playerColor, fromRow, fromCol, toRow, toCol)) {
      MoveResult result = applyMove(fromRow, fromCol, toRow, toCol);
      onPlayerMoveApplied(result, fromRow, fromCol, toRow, toCol);
    }
  } else {
    requestEngineMove();
  }

  boardDriver->updateSensorPrev();
}

// --- Resign hooks (thinking-animation management) ---

void ChessBot::onBeforeResignConfirm() {
  wasThinkingBeforeResign_ = (thinkingAnimation != nullptr);
  if (wasThinkingBeforeResign_) stopThinking();
}

void ChessBot::onResignCancelled() {
  if (wasThinkingBeforeResign_ && controller_->currentTurn() != playerColor && !controller_->isGameOver())
    startThinking();
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
