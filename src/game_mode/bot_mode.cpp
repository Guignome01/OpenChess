#include "bot_mode.h"
#include "chess_game.h"
#include "led_colors.h"
#include "system_utils.h"
#include "chess_utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

BotMode::BotMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, EngineProvider* provider)
    : GameMode(bd, wm, gc), provider_(provider) {}

BotMode::~BotMode() {
  delete provider_;
}

// ---------------------------------------------------------------
// begin() — common initialization skeleton for all engines
// ---------------------------------------------------------------

void BotMode::begin() {
  Serial.println("=== Starting Bot Mode ===");

  if (!wifiManager_->isWiFiConnected()) {
    abortWithError("No WiFi connection");
    return;
  }

  // Provider may block (e.g., Lichess game discovery). Show waiting animation.
  std::atomic<bool>* waitAnim = boardDriver_->startWaitingAnimation();

  EngineInitResult initResult;
  bool ok = provider_->initialize(initResult);

  boardDriver_->stopAndWaitForAnimation(waitAnim);

  if (!ok) {
    abortWithError("Engine initialization failed");
    return;
  }

  playerColor_ = initResult.playerColor;
  Serial.printf("Player: %s, Engine: %s\n",
                playerColor_ == 'w' ? "White" : "Black",
                playerColor_ == 'w' ? "Black" : "White");

  if (initResult.canResume && tryResumeGame()) {
    // Resumed from flash — skip new game
  } else {
    controller_->startNewGame(initResult.gameModeId, playerColor_, initResult.depth);
    if (!initResult.fen.empty())
      setBoardStateFromFEN(initResult.fen);
  }

  waitForBoardSetup(controller_->getBoard());

  // If it's the engine's turn after setup, start requesting immediately
  if (controller_->currentTurn() != playerColor_) {
    startThinking();
    provider_->requestMove(controller_->getFen());
    botState_ = BotState::ENGINE_THINKING;
  }

  Serial.println("=== Game Ready ===");
}

// ---------------------------------------------------------------
// update() — non-blocking state machine
// ---------------------------------------------------------------

bool BotMode::isNavigationAllowed() const {
  return controller_->isGameOver() || botState_ == BotState::PLAYER_TURN;
}

void BotMode::update() {
  if (controller_->isGameOver()) return;

  boardDriver_->readSensors();

  if (processResign()) return;

  if (botState_ == BotState::PLAYER_TURN) {
    int fromRow, fromCol, toRow, toCol;
    if (tryPlayerMove(playerColor_, fromRow, fromCol, toRow, toCol)) {
      MoveResult result = applyMove(fromRow, fromCol, toRow, toCol);

      // Notify provider (Lichess sends the move to the server)
      std::string coord = ChessGame::toCoordinate(fromRow, fromCol, toRow, toCol, result.promotedTo);
      if (!provider_->onPlayerMoveApplied(coord)) {
        abortWithError("Failed to send move to server");
        return;
      }

      // If the game didn't end and it's now the engine's turn, start engine
      if (!controller_->isGameOver() && controller_->currentTurn() != playerColor_) {
        startThinking();
        provider_->requestMove(controller_->getFen());
        botState_ = BotState::ENGINE_THINKING;
      }
    }
  } else if (botState_ == BotState::ENGINE_THINKING) {
    EngineResult result;
    if (provider_->checkResult(result)) {
      stopThinking();
      switch (result.type) {
        case EngineResult::MOVE:
          applyEngineMove(result.move);
          break;
        case EngineResult::GAME_ENDED:
          handleRemoteGameEnd(result);
          break;
        case EngineResult::NONE:
          Serial.println("BotMode: engine returned no result, retrying...");
          startThinking();
          provider_->requestMove(controller_->getFen());
          return;  // Stay in ENGINE_THINKING
      }
      botState_ = BotState::PLAYER_TURN;
    }
  }

  boardDriver_->updateSensorPrev();
}

// ---------------------------------------------------------------
// Engine move application
// ---------------------------------------------------------------

void BotMode::applyEngineMove(const std::string& move) {
  int fromRow, fromCol, toRow, toCol;
  char promotion;
  if (!ChessGame::parseCoordinate(move, fromRow, fromCol, toRow, toCol, promotion)) {
    Serial.printf("BotMode: failed to parse engine move: %s\n", move.c_str());
    return;
  }

  // Verify the move is from the correct color piece
  char piece = controller_->getSquare(fromRow, fromCol);
  char engineColor = ChessUtils::opponentColor(playerColor_);
  bool isEnginePiece = (engineColor == 'w') ? (piece >= 'A' && piece <= 'Z') : (piece >= 'a' && piece <= 'z');

  if (piece == ' ' || !isEnginePiece) {
    Serial.printf("BotMode: engine move from invalid square (%d,%d) piece='%c'\n",
                  fromRow, fromCol, piece);
    return;
  }

  Serial.printf("Engine move: %s (%d,%d)->(%d,%d)\n", move.c_str(), fromRow, fromCol, toRow, toCol);
  applyMove(fromRow, fromCol, toRow, toCol, promotion, true);
}

void BotMode::handleRemoteGameEnd(const EngineResult& result) {
  Serial.printf("Remote game ended: result=%d, winner=%c\n",
                static_cast<int>(result.gameResult), result.winnerColor);
  LedRGB color = (result.winnerColor == 'd')
                     ? LedColors::Cyan
                     : SystemUtils::colorLed(result.winnerColor);
  boardDriver_->fireworkAnimation(color);
  controller_->endGame(result.gameResult, result.winnerColor);
}

void BotMode::abortWithError(const char* message) {
  Serial.printf("BotMode ABORT: %s\n", message);
  boardDriver_->flashBoardAnimation(LedColors::Red);
  controller_->endGame(GameResult::ABORTED, ' ');
}

// ---------------------------------------------------------------
// Resign hooks
// ---------------------------------------------------------------

void BotMode::onBeforeResignConfirm() {
  wasThinkingBeforeResign_ = (thinkingAnimation_ != nullptr);
  if (wasThinkingBeforeResign_) {
    provider_->cancelRequest();
    stopThinking();
  }
}

void BotMode::onResignCancelled() {
  if (wasThinkingBeforeResign_ && controller_->currentTurn() != playerColor_ && !controller_->isGameOver()) {
    startThinking();
    provider_->requestMove(controller_->getFen());
    botState_ = BotState::ENGINE_THINKING;
  }
}

void BotMode::onResignConfirmed(char resignColor) {
  provider_->onResignConfirmed();
}

// ---------------------------------------------------------------
// Thinking animation helpers
// ---------------------------------------------------------------

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

// ---------------------------------------------------------------
// Remote move guidance (LED + sensor blocking)
// Guides the player to physically execute the engine's move.
// ---------------------------------------------------------------

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
