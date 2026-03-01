#include "lichess.h"
#include "codec.h"
#include "utils.h"
#include "led_colors.h"
#include "lichess_api.h"
#include "system_utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

// Map Lichess status string to GameResult enum.
static GameResult lichessStatusToResult(const String& status) {
  if (status == "mate") return RESULT_CHECKMATE;
  if (status == "resign") return RESULT_RESIGNATION;
  if (status == "stalemate") return RESULT_STALEMATE;
  if (status == "draw") return RESULT_DRAW_AGREEMENT;
  if (status == "timeout" || status == "outoftime") return RESULT_TIMEOUT;
  if (status == "aborted") return RESULT_ABORTED;
  return RESULT_ABORTED; // Unknown status — treat as aborted
}

// Map Lichess winner string to color char.
static char lichessWinnerToColor(const String& winner) {
  if (winner == "white") return 'w';
  if (winner == "black") return 'b';
  return 'd'; // draw or empty
}

ChessLichess::ChessLichess(BoardDriver* bd, WiFiManagerESP32* wm, LichessConfig cfg)
    : ChessBot(bd, wm, nullptr, 'w'),
      lichessConfig(cfg),
      currentGameId(""),
      lastKnownMoves(""),
      lastSentMove(""),
      lastPollTime(0) {}

void ChessLichess::begin() {
  Serial.println("=== Starting Lichess Mode ===");

  if (!wifiManager->isWiFiConnected()) {
    Serial.println("Not connected to WiFi. Lichess mode unavailable.");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gm_.endGame(RESULT_ABORTED, ' ');
    return;
  }

  if (lichessConfig.apiToken.length() == 0) {
    Serial.println("No Lichess API token configured!");
    Serial.println("Please set your Lichess API token via the web interface.");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gm_.endGame(RESULT_ABORTED, ' ');
    return;
  }

  String username;
  LichessAPI::setToken(lichessConfig.apiToken);
  if (!LichessAPI::verifyToken(username)) {
    Serial.println("Invalid Lichess API token!");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gm_.endGame(RESULT_ABORTED, ' ');
    return;
  }

  Serial.println("Logged in as: " + username);
  Serial.println("Waiting for a Lichess game to start...");
  Serial.println("Start a game on lichess.org or accept a challenge!");
  Serial.println("====================================");

  waitForLichessGame();
}

void ChessLichess::waitForLichessGame() {
  Serial.println("Searching for active Lichess games...");
  std::atomic<bool>* waitAnim = boardDriver->startWaitingAnimation();
  LichessEvent event;
  event.type = LichessEventType::UNKNOWN;
  while (!gm_.isGameOver()) {
    if (!LichessAPI::pollForGameEvent(event) || event.type != LichessEventType::GAME_START) {
      delay(2000);
      continue;
    }
    break;
  }
  boardDriver->stopAndWaitForAnimation(waitAnim);
  currentGameId = event.gameId;
  playerColor = event.myColor;

  Serial.println("=== Game Found! ===");
  Serial.println("Game ID: " + currentGameId);
  Serial.printf("Playing as: %s\n", playerColor == 'w' ? "White" : "Black");

  // Get full game state
  LichessGameState state;
  state.myColor = playerColor;
  state.gameId = currentGameId;
  state.fen = event.fen; // Use FEN from initial event as fallback

  if (LichessAPI::pollGameStream(currentGameId, state)) {
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
          state.isMyTurn = (event.fen[i + 1] == 'w' && playerColor == 'w') || (event.fen[i + 1] == 'b' && playerColor == 'b');
          break;
        }
      }
    } else {
      state.isMyTurn = (playerColor == 'w'); // White moves first
    }
  }

  // Sync the board with the current game state
  syncBoardWithLichess(state);

  // Wait for board setup with the current position
  waitForBoardSetup(gm_.getBoard());

  Serial.println("Board synchronized! Game starting...");
  wifiManager->updateBoardState(gm_.getFen(), gm_.getEvaluation());
}

void ChessLichess::syncBoardWithLichess(const LichessGameState& state) {
  initializeBoard();

  playerColor = state.myColor;
  currentGameId = state.gameId;

  // If FEN is provided, use it directly — turn is derived from the FEN's active-color field
  if (state.fen.length() > 0 && state.fen != "startpos")
    setBoardStateFromFEN(std::string(state.fen.c_str()));
  else
    Serial.println("No FEN provided, assuming starting position");

  lastKnownMoves = "";

  Serial.printf("My color: %s, Is my turn: %s\n", playerColor == 'w' ? "White" : "Black", state.isMyTurn ? "Yes" : "No");
}

// --- ChessBot hooks ---

void ChessLichess::requestEngineMove() {
  // Start thinking animation if not already running
  if (!thinkingAnimation)
    startThinking();

  // Polling throttle
  if (millis() - lastPollTime < POLL_INTERVAL_MS)
    return;
  lastPollTime = millis();

  // Poll Lichess for updates
  LichessGameState state;
  state.myColor = playerColor;
  state.gameId = currentGameId;
  if (!LichessAPI::pollGameStream(currentGameId, state))
    return;

  // Process new moves first — applyMove() → ChessBoard detects
  // checkmate/stalemate/draw from the move itself, with correct animation.
  if (state.lastMove.length() > 0 && state.lastMove != lastKnownMoves) {
    lastKnownMoves = state.lastMove;
    // Skip if this is the move we just sent (avoid processing our own move)
    if (state.lastMove == lastSentMove) {
      Serial.println("Skipping own move echo: " + state.lastMove);
    } else {
      Serial.println("Lichess move received: " + state.lastMove);
      int fromRow, fromCol, toRow, toCol;
      char promotion = ' ';
      if (ChessCodec::parseUCIMove(std::string(state.lastMove.c_str()), fromRow, fromCol, toRow, toCol, promotion)) {
        stopThinking();
        Serial.printf("Lichess UCI move: %s = (%d,%d) -> (%d,%d)%s%c\n", state.lastMove.c_str(), fromRow, fromCol, toRow, toCol, promotion == ' ' ? "" : " Promotion to: ", promotion);
        applyMove(fromRow, fromCol, toRow, toCol, promotion, true);
      } else {
        Serial.println("Failed to parse Lichess UCI move: " + state.lastMove);
      }
    }
  }

  // External game-end from Lichess (resign, timeout, abort) — only if
  // ChessBoard didn't already detect it through move processing above.
  if (state.gameEnded && !gm_.isGameOver()) {
    Serial.println("Game ended externally! Status: " + state.status);
    if (state.winner.length() > 0)
      Serial.println("Winner: " + state.winner);
    stopThinking();
    GameResult result = lichessStatusToResult(state.status);
    char winner = lichessWinnerToColor(state.winner);
    boardDriver->fireworkAnimation(winner == 'd' ? LedColors::Cyan : SystemUtils::colorLed(winner));
    gm_.endGame(result, winner);
  }
}

void ChessLichess::onPlayerMoveApplied(const MoveResult& result, int fromRow, int fromCol, int toRow, int toCol) {
  sendMoveToLichess(fromRow, fromCol, toRow, toCol, result.promotedTo);
}

// --- Lichess communication ---

void ChessLichess::sendMoveToLichess(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  String uciMove = String(ChessCodec::toUCIMove(fromRow, fromCol, toRow, toCol, promotion).c_str());
  Serial.println("Sending move to Lichess: " + uciMove);

  // Track this move so we don't process it as a remote move when it echoes back
  lastSentMove = uciMove;

  // Retry up to 3 times if sending fails
  const int maxRetries = 3;
  int attempt = 0;
  bool sent = false;
  while (attempt < maxRetries && !sent) {
    if (LichessAPI::makeMove(currentGameId, uciMove)) {
      sent = true;
      break;
    } else {
      Serial.printf("ERROR: Failed to send move to Lichess! Attempt %d/%d\n", attempt + 1, maxRetries);
      delay(500);
      attempt++;
    }
  }
  if (!sent) {
    gm_.endGame(RESULT_ABORTED, ' ');
    Serial.println("ERROR: All attempts to send move to Lichess failed, ending game!");
    boardDriver->flashBoardAnimation(LedColors::Red);
    lastSentMove = "";
  }
}

bool ChessLichess::handleResign(char resignColor) {
  bool wasThinking = (thinkingAnimation != nullptr);
  if (wasThinking) stopThinking();

  bool flipped = (playerColor == 'b');

  Serial.printf("Lichess resign confirmation for %s...\n", ChessUtils::colorName(resignColor));

  if (!boardConfirm(boardDriver, flipped)) {
    Serial.println("Resign cancelled");
    // Restart thinking animation if it was running before
    if (wasThinking && gm_.currentTurn() != playerColor && !gm_.isGameOver())
      startThinking();
    return false;
  }

  // Send resign to Lichess API
  Serial.println("Sending resign to Lichess...");
  LichessAPI::resignGame(currentGameId);

  char winnerColor = (resignColor == 'w') ? 'b' : 'w';
  Serial.printf("RESIGNATION! %s resigns on Lichess. %s wins!\n", ChessUtils::colorName(resignColor), ChessUtils::colorName(winnerColor));

  boardDriver->fireworkAnimation(SystemUtils::colorLed(winnerColor));
  // Lichess mode has no local moveHistory (nullptr)
  gm_.endGame(RESULT_RESIGNATION, winnerColor);
  return true;
}
