#include "lichess_provider.h"
#include "engine/lichess/lichess_api.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Map Lichess status string to GameResult enum.
static GameResult lichessStatusToResult(const String& status) {
  if (status == "mate") return GameResult::CHECKMATE;
  if (status == "resign") return GameResult::RESIGNATION;
  if (status == "stalemate") return GameResult::STALEMATE;
  if (status == "draw") return GameResult::DRAW_AGREEMENT;
  if (status == "timeout" || status == "outoftime") return GameResult::TIMEOUT;
  if (status == "aborted") return GameResult::ABORTED;
  return GameResult::ABORTED;
}

// Map Lichess winner string to color char.
static char lichessWinnerToColor(const String& winner) {
  if (winner == "white") return 'w';
  if (winner == "black") return 'b';
  return 'd';
}

LichessProvider::LichessProvider(const LichessConfig& config)
    : config_(config) {}

// ---------------------------------------------------------------
// initialize() — blocking game discovery (BotMode wraps with animation)
// ---------------------------------------------------------------

bool LichessProvider::initialize(EngineInitResult& result) {
  if (config_.apiToken.length() == 0) {
    Serial.println("LichessProvider: no API token configured");
    return false;
  }

  LichessAPI::setToken(config_.apiToken);

  String username;
  if (!LichessAPI::verifyToken(username)) {
    Serial.println("LichessProvider: invalid API token");
    return false;
  }
  Serial.println("Lichess: logged in as " + username);
  Serial.println("Waiting for a Lichess game...");

  // Poll for a game start event (timeout after 3 minutes)
  LichessEvent event;
  event.type = LichessEventType::UNKNOWN;
  unsigned long initDeadline = millis() + 180000;
  while (event.type != LichessEventType::GAME_START) {
    if (LichessAPI::pollForGameEvent(event) && event.type == LichessEventType::GAME_START)
      break;
    if (millis() > initDeadline) {
      Serial.println("LichessProvider: timed out waiting for game");
      return false;
    }
    delay(2000);
  }

  currentGameId_ = event.gameId;
  playerColor_ = event.myColor;

  Serial.println("Game found! ID: " + currentGameId_);
  Serial.printf("Playing as: %s\n", playerColor_ == 'w' ? "White" : "Black");

  // Get full game state
  LichessGameState state;
  state.myColor = playerColor_;
  state.gameId = currentGameId_;
  state.fen = event.fen;

  if (!LichessAPI::pollGameStream(currentGameId_, state)) {
    Serial.println("Warning: could not get full game state, using initial event data");
    state.gameStarted = true;
    state.gameEnded = false;
    state.lastMove = "";
  }

  // Populate init result
  result.playerColor = playerColor_;
  result.gameModeId = GameModeId::LICHESS;
  result.depth = 0;
  result.canResume = false;

  if (state.fen.length() > 0 && state.fen != "startpos")
    result.fen = std::string(state.fen.c_str());
  else
    result.fen = "";

  lastKnownMoveCount_ = state.moveCount;
  return true;
}

// ---------------------------------------------------------------
// Async opponent-move polling via FreeRTOS task
// ---------------------------------------------------------------

void LichessProvider::requestMove(const std::string& /*fen*/) {
  auto* ctx = new TaskContext();
  ctx->gameId = currentGameId_;
  ctx->playerColor = playerColor_;
  ctx->lastKnownMoveCount = lastKnownMoveCount_;
  ctx->lastSentMove = lastSentMove_;
  spawnTask(ctx, "liTask", taskFunction);
}

bool LichessProvider::checkResult(EngineResult& result) {
  if (!peekResult(result)) return false;
  auto* ctx = static_cast<TaskContext*>(activeTask_);
  lastKnownMoveCount_ = ctx->lastKnownMoveCount;
  finishTask();
  return true;
}

// ---------------------------------------------------------------
// Player move → send to Lichess server (synchronous, small request)
// ---------------------------------------------------------------

bool LichessProvider::onPlayerMoveApplied(const std::string& moveCoord) {
  String coord = String(moveCoord.c_str());
  Serial.println("Sending move to Lichess: " + coord);
  lastSentMove_ = coord;

  const int maxRetries = 3;
  for (int attempt = 0; attempt < maxRetries; attempt++) {
    if (LichessAPI::makeMove(currentGameId_, coord)) return true;
    Serial.printf("LichessProvider: send failed, attempt %d/%d\n", attempt + 1, maxRetries);
    delay(500);
  }

  Serial.println("LichessProvider: all send attempts failed");
  lastSentMove_ = "";
  return false;
}

void LichessProvider::onResignConfirmed() {
  Serial.println("Sending resign to Lichess...");
  LichessAPI::resignGame(currentGameId_);
}

// ---------------------------------------------------------------
// FreeRTOS task — persistent NDJSON stream for opponent moves.
// Opens one TLS connection and reads events as they arrive.
// Reconnects with exponential backoff on connection loss.
// ---------------------------------------------------------------

void LichessProvider::taskFunction(void* param) {
  auto* ctx = static_cast<TaskContext*>(param);

  static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
  static constexpr unsigned long INITIAL_BACKOFF_MS = 1000;
  static constexpr unsigned long MAX_BACKOFF_MS = 8000;
  // Timeout for reading stream events: generous enough to accommodate
  // Lichess heartbeat interval (~15-20s) with margin.
  static constexpr unsigned long STREAM_READ_TIMEOUT_MS = 30000;

  int reconnectAttempts = 0;
  unsigned long backoffMs = INITIAL_BACKOFF_MS;

  while (!ctx->cancel.load()) {
    WiFiClientSecure client;

    if (!LichessAPI::connectGameStream(client, ctx->gameId)) {
      reconnectAttempts++;
      if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
        Serial.println("Lichess stream: all reconnect attempts exhausted, aborting");
        ctx->result.type = EngineResult::GAME_ENDED;
        ctx->result.gameResult = GameResult::ABORTED;
        ctx->result.winnerColor = ' ';
        ctx->ready.store(true);
        vTaskDelete(nullptr);
        return;
      }
      Serial.printf("Lichess stream: reconnect attempt %d/%d in %lums\n",
                     reconnectAttempts, MAX_RECONNECT_ATTEMPTS, backoffMs);
      delay(backoffMs);
      backoffMs = min(backoffMs * 2, MAX_BACKOFF_MS);
      continue;
    }

    // Connected — reset backoff
    reconnectAttempts = 0;
    backoffMs = INITIAL_BACKOFF_MS;

    // Read events from the persistent stream
    while (!ctx->cancel.load()) {
      LichessGameState state;
      state.myColor = ctx->playerColor;
      state.gameId = ctx->gameId;

      if (!LichessAPI::readStreamEvent(client, state, ctx->cancel, STREAM_READ_TIMEOUT_MS)) {
        // Connection dropped or timed out — break to reconnect loop
        Serial.println("Lichess stream: connection lost, will reconnect...");
        client.stop();
        break;
      }

      // New move detected?
      if (state.moveCount > ctx->lastKnownMoveCount) {
        // Skip echo of our own move
        if (state.lastMove == ctx->lastSentMove) {
          ctx->lastKnownMoveCount = state.moveCount;
          continue;
        }

        Serial.println("Lichess move received: " + state.lastMove);
        ctx->lastKnownMoveCount = state.moveCount;
        ctx->result.type = EngineResult::MOVE;
        ctx->result.move = std::string(state.lastMove.c_str());
        client.stop();
        ctx->ready.store(true);
        vTaskDelete(nullptr);
        return;
      }

      // External game-end?
      if (state.gameEnded) {
        Serial.println("Lichess game ended: " + state.status);
        ctx->result.type = EngineResult::GAME_ENDED;
        ctx->result.gameResult = lichessStatusToResult(state.status);
        ctx->result.winnerColor = lichessWinnerToColor(state.winner);
        client.stop();
        ctx->ready.store(true);
        vTaskDelete(nullptr);
        return;
      }
    }
  }

  // Cancelled
  ctx->ready.store(true);
  vTaskDelete(nullptr);
}
