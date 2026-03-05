#include "bot_mode.h"
#include "chess_game.h"
#include "led_colors.h"
#include "system_utils.h"
#include "chess_utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

BotMode::BotMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, char playerClr)
    : GameMode(bd, wm, gc), playerColor_(playerClr), thinkingAnimation_(nullptr), wasThinkingBeforeResign_(false) {}

float BotMode::getEngineEvaluation() { return controller_->getEvaluation(); }

bool BotMode::isNavigationAllowed() const {
  return controller_->isGameOver() || controller_->currentTurn() == playerColor_;
}

// --- Template Method: game loop skeleton ---
// Player turn: tryPlayerMove → applyMove → onPlayerMoveApplied hook.
// Engine turn: requestEngineMove (subclass drives the move).

void BotMode::update() {
  if (controller_->isGameOver()) return;

  boardDriver_->readSensors();

  if (processResign()) return;

  bool isPlayerTurn = (controller_->currentTurn() == playerColor_);

  if (isPlayerTurn) {
    int fromRow, fromCol, toRow, toCol;
    if (tryPlayerMove(playerColor_, fromRow, fromCol, toRow, toCol)) {
      MoveResult result = applyMove(fromRow, fromCol, toRow, toCol);
      onPlayerMoveApplied(result, fromRow, fromCol, toRow, toCol);
    }
  } else {
    requestEngineMove();
  }

  boardDriver_->updateSensorPrev();
}

// --- Resign hooks (thinking-animation management) ---

void BotMode::onBeforeResignConfirm() {
  wasThinkingBeforeResign_ = (thinkingAnimation_ != nullptr);
  if (wasThinkingBeforeResign_) stopThinking();
}

void BotMode::onResignCancelled() {
  if (wasThinkingBeforeResign_ && controller_->currentTurn() != playerColor_ && !controller_->isGameOver())
    startThinking();
}

// --- Thinking animation helpers ---

void BotMode::startThinking() {
  boardDriver_->waitForAnimationQueueDrain();
  thinkingAnimation_ = boardDriver_->startThinkingAnimation();
}

void BotMode::stopThinking() {
  if (thinkingAnimation_) {
    boardDriver_->stopAndWaitForAnimation(thinkingAnimation_);
    thinkingAnimation_ = nullptr;
  }
}

// --- Remote move guidance (LED + sensor blocking) ---
// Shared by all engine subclasses. Guides the local player to physically
// execute a move made by the remote engine on the board.

void BotMode::waitForRemoteMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant, int enPassantCapturedPawnRow) {
  BoardDriver::LedGuard guard(boardDriver_);
  boardDriver_->clearAllLEDs(false);
  // Show source square (where to pick up from)
  boardDriver_->setSquareLED(fromRow, fromCol, LedColors::Cyan);
  // Show destination square (where to place)
  if (isCapture)
    boardDriver_->setSquareLED(toRow, toCol, LedColors::Red);
  else
    boardDriver_->setSquareLED(toRow, toCol, LedColors::White);
  if (isEnPassant)
    boardDriver_->setSquareLED(enPassantCapturedPawnRow, toCol, LedColors::Purple);
  boardDriver_->showLEDs();

  bool piecePickedUp = false;
  bool capturedPieceRemoved = false;
  bool moveCompleted = false;

  Serial.println("Waiting for you to complete the remote move...");

  while (!moveCompleted) {
    boardDriver_->readSensors();

    // For capture moves, ensure captured piece is removed first
    // For en passant, check the actual captured pawn square (not the destination)
    if (isCapture && !capturedPieceRemoved) {
      int captureCheckRow = isEnPassant ? enPassantCapturedPawnRow : toRow;
      if (!boardDriver_->getSensorState(captureCheckRow, toCol)) {
        capturedPieceRemoved = true;
        if (isEnPassant)
          Serial.println("En passant captured pawn removed, now complete the move...");
        else
          Serial.println("Captured piece removed, now complete the move...");
      }
    }

    // Check if piece was picked up from source
    if (!piecePickedUp && !boardDriver_->getSensorState(fromRow, fromCol)) {
      piecePickedUp = true;
      Serial.println("Piece picked up, now place it on the destination...");
    }

    // Check if piece was placed on destination
    // For captures: wait until captured piece is removed AND piece is placed
    // For normal moves: just wait for piece to be placed
    if (piecePickedUp && boardDriver_->getSensorState(toRow, toCol))
      if (!isCapture || (isCapture && capturedPieceRemoved)) {
        moveCompleted = true;
        Serial.println("Move completed on physical board!");
      }

    delay(SENSOR_READ_DELAY_MS);
    boardDriver_->updateSensorPrev();
  }

  boardDriver_->clearAllLEDs();
}  // LedGuard released
