#include "lichess_mode.h"
#include "chess_game.h"
#include "utils.h"
#include "led_colors.h"
#include "lichess_api.h"
#include "system_utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

// Map Lichess status string to GameResult enum.
static GameResult lichessStatusToResult(const String& status) {
  if (status == "mate") return GameResult::CHECKMATE;
  if (status == "resign") return GameResult::RESIGNATION;
  if (status == "stalemate") return GameResult::STALEMATE;
  if (status == "draw") return GameResult::DRAW_AGREEMENT;
  if (status == "timeout" || status == "outoftime") return GameResult::TIMEOUT;
  if (status == "aborted") return GameResult::ABORTED;
  return GameResult::ABORTED; // Unknown status — treat as aborted
}

// Map Lichess winner string to color char.
static char lichessWinnerToColor(const String& winner) {
  if (winner == "white") return 'w';
  if (winner == "black") return 'b';
  return 'd'; // draw or empty
}

LichessMode::LichessMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, LichessConfig cfg)
    : BotMode(bd, wm, gc, 'w'),
      lichessConfig_(cfg),
      currentGameId_(""),
      lastKnownMoves_(""),
      lastSentMove_(""),
      lastPollTime_(0) {}

void LichessMode::begin() {
  Serial.println("=== Starting Lichess Mode ===");

  if (!wifiManager_->isWiFiConnected()) {
    Serial.println("Not connected to WiFi. Lichess mode unavailable.");
    boardDriver_->flashBoardAnimation(LedColors::Red);
    controller_->endGame(GameResult::ABORTED, ' ');
    return;
  }

  if (lichessConfig_.apiToken.length() == 0) {
    Serial.println("No Lichess API token configured!");
    Serial.println("Please set your Lichess API token via the web interface.");
    boardDriver_->flashBoardAnimation(LedColors::Red);
    controller_->endGame(GameResult::ABORTED, ' ');
    return;
  }

  String username;
  LichessAPI::setToken(lichessConfig_.apiToken);
  if (!LichessAPI::verifyToken(username)) {
    Serial.println("Invalid Lichess API token!");
    boardDriver_->flashBoardAnimation(LedColors::Red);
    controller_->endGame(GameResult::ABORTED, ' ');
    return;
  }

  Serial.println("Logged in as: " + username);
  Serial.println("Waiting for a Lichess game to start...");
  Serial.println("Start a game on lichess.org or accept a challenge!");
  Serial.println("====================================");

  waitForLichessGame();
}

void LichessMode::waitForLichessGame() {
  Serial.println("Searching for active Lichess games...");
  std::atomic<bool>* waitAnim = boardDriver_->startWaitingAnimation();
  LichessEvent event;
  event.type = LichessEventType::UNKNOWN;
  while (!controller_->isGameOver()) {
    if (!LichessAPI::pollForGameEvent(event) || event.type != LichessEventType::GAME_START) {
      delay(2000);
      continue;
    }
    break;
  }
  boardDriver_->stopAndWaitForAnimation(waitAnim);
  currentGameId_ = event.gameId;
  playerColor_ = event.myColor;

  Serial.println("=== Game Found! ===");
  Serial.println("Game ID: " + currentGameId_);
  Serial.printf("Playing as: %s\n", playerColor_ == 'w' ? "White" : "Black");

  // Get full game state
  LichessGameState state;
  state.myColor = playerColor_;
  state.gameId = currentGameId_;
  state.fen = event.fen; // Use FEN from initial event as fallback

  if (LichessAPI::pollGameStream(currentGameId_, state)) {
    Serial.println("Got full game state from stream");
  } else {
    // Fallback: Use data from the initial event
    Serial.println("Warning: Could not get full game state, using initial event data");
    state.gameStarted = true;
    state.gameEnded = false;
    state.lastMove = "";
    // Determine turn from FEN (6th field) or assume White starts
    if (event.fen.length() > 0) {
      int spaceCount = 0;
      for (size_t i = 0; i < event.fen.length(); i++) {
        if (event.fen[i] == ' ') spaceCount++;
        if (spaceCount == 1) {
          state.isMyTurn = (event.fen[i + 1] == 'w' && playerColor_ == 'w') || (event.fen[i + 1] == 'b' && playerColor_ == 'b');
          break;
        }
      }
    } else {
      state.isMyTurn = (playerColor_ == 'w'); // White moves first
    }
  }

  // Sync the board with the current game state
  syncBoardWithLichess(state);

  // Wait for board setup with the current position
  waitForBoardSetup(controller_->getBoard());

  Serial.println("Board synchronized! Game starting...");
}

void LichessMode::syncBoardWithLichess(const LichessGameState& state) {
  controller_->newGame();

  playerColor_ = state.myColor;
  currentGameId_ = state.gameId;

  // If FEN is provided, use it directly — turn is derived from the FEN's active-color field
  if (state.fen.length() > 0 && state.fen != "startpos")
    setBoardStateFromFEN(std::string(state.fen.c_str()));
  else
    Serial.println("No FEN provided, assuming starting position");

  lastKnownMoves_ = "";

  Serial.printf("My color: %s, Is my turn: %s\n", playerColor_ == 'w' ? "White" : "Black", state.isMyTurn ? "Yes" : "No");
}

// --- BotMode hooks ---

void LichessMode::requestEngineMove() {
  // Start thinking animation if not already running
  if (!thinkingAnimation_)
    startThinking();

  // Polling throttle
  if (millis() - lastPollTime_ < POLL_INTERVAL_MS)
    return;
  lastPollTime_ = millis();

  // Poll Lichess for updates
  LichessGameState state;
  state.myColor = playerColor_;
  state.gameId = currentGameId_;
  if (!LichessAPI::pollGameStream(currentGameId_, state))
    return;

  // Process new moves first — applyMove() → ChessBoard detects
  // checkmate/stalemate/draw from the move itself, with correct animation.
  if (state.lastMove.length() > 0 && state.lastMove != lastKnownMoves_) {
    lastKnownMoves_ = state.lastMove;
    // Skip if this is the move we just sent (avoid processing our own move)
    if (state.lastMove == lastSentMove_) {
      Serial.println("Skipping own move echo: " + state.lastMove);
    } else {
      Serial.println("Lichess move received: " + state.lastMove);
      stopThinking();
      applyUCIMove(std::string(state.lastMove.c_str()));
    }
  }

  // External game-end from Lichess (resign, timeout, abort) — only if
  // ChessBoard didn't already detect it through move processing above.
  if (state.gameEnded && !controller_->isGameOver()) {
    Serial.println("Game ended externally! Status: " + state.status);
    if (state.winner.length() > 0)
      Serial.println("Winner: " + state.winner);
    stopThinking();
    GameResult result = lichessStatusToResult(state.status);
    char winner = lichessWinnerToColor(state.winner);
    boardDriver_->fireworkAnimation(winner == 'd' ? LedColors::Cyan : SystemUtils::colorLed(winner));
    controller_->endGame(result, winner);
  }
}

void LichessMode::onPlayerMoveApplied(const MoveResult& result, int fromRow, int fromCol, int toRow, int toCol) {
  sendMoveToLichess(fromRow, fromCol, toRow, toCol, result.promotedTo);
}

// --- Lichess communication ---

void LichessMode::sendMoveToLichess(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  String uciMove = String(ChessGame::toUCIMove(fromRow, fromCol, toRow, toCol, promotion).c_str());
  Serial.println("Sending move to Lichess: " + uciMove);

  // Track this move so we don't process it as a remote move when it echoes back
  lastSentMove_ = uciMove;

  // Retry up to 3 times if sending fails
  const int maxRetries = 3;
  int attempt = 0;
  bool sent = false;
  while (attempt < maxRetries && !sent) {
    if (LichessAPI::makeMove(currentGameId_, uciMove)) {
      sent = true;
      break;
    } else {
      Serial.printf("ERROR: Failed to send move to Lichess! Attempt %d/%d\n", attempt + 1, maxRetries);
      delay(500);
      attempt++;
    }
  }
  if (!sent) {
    controller_->endGame(GameResult::ABORTED, ' ');
    Serial.println("ERROR: All attempts to send move to Lichess failed, ending game!");
    boardDriver_->flashBoardAnimation(LedColors::Red);
    lastSentMove_ = "";
  }
}

void LichessMode::onResignConfirmed(char resignColor) {
  Serial.println("Sending resign to Lichess...");
  LichessAPI::resignGame(currentGameId_);
}
