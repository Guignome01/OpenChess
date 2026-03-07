#include "stockfish_provider.h"
#include "engine/stockfish/stockfish_api.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

StockfishProvider::StockfishProvider(const StockfishSettings& settings, char playerColor)
    : settings_(settings), playerColor_(playerColor) {}

bool StockfishProvider::initialize(EngineInitResult& result) {
  Serial.printf("StockfishProvider: depth=%d, timeout=%dms, retries=%d\n",
                settings_.depth, settings_.timeoutMs, settings_.maxRetries);
  result.playerColor = playerColor_;
  result.fen = "";  // Starting position
  result.gameModeId = GameModeId::BOT;
  result.depth = static_cast<uint8_t>(settings_.depth);
  result.canResume = true;
  return true;
}

void StockfishProvider::requestMove(const std::string& fen) {
  auto* ctx = new TaskContext();
  ctx->fen = fen;
  ctx->depth = settings_.depth;
  ctx->timeoutMs = settings_.timeoutMs;
  ctx->maxRetries = settings_.maxRetries;
  spawnTask(ctx, "sfTask", taskFunction);
}

bool StockfishProvider::checkResult(EngineResult& result) {
  if (!peekResult(result)) return false;
  if (result.type == EngineResult::MOVE)
    currentEvaluation_ = result.evaluation;
  finishTask();
  return true;
}

float StockfishProvider::getEvaluation() {
  return currentEvaluation_;
}

// ---------------------------------------------------------------
// FreeRTOS task — runs the blocking HTTP request off the game loop.
// ---------------------------------------------------------------

void StockfishProvider::taskFunction(void* param) {
  auto* ctx = static_cast<TaskContext*>(param);

  WiFiClientSecure client;
  client.setInsecure();

  String path = StockfishAPI::buildRequestURL(String(ctx->fen.c_str()), ctx->depth);
  Serial.println("Stockfish request: " STOCKFISH_API_URL + path);

  String response;
  bool gotResponse = false;

  for (int attempt = 1; attempt <= ctx->maxRetries; attempt++) {
    if (ctx->cancel.load()) break;

    if (attempt > 1) {
      Serial.printf("Stockfish: attempt %d/%d\n", attempt, ctx->maxRetries);
      delay(500);
    }

    if (!client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
      Serial.println("Stockfish: connection failed");
      continue;
    }

    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: " STOCKFISH_API_URL);
    client.println("Connection: close");
    client.println();

    unsigned long startTime = millis();
    while (client.connected() && (millis() - startTime < (unsigned long)ctx->timeoutMs)) {
      if (ctx->cancel.load()) break;
      if (client.available()) {
        response = client.readString();
        gotResponse = true;
        break;
      }
      delay(10);
    }
    client.stop();

    if (gotResponse) break;
    Serial.println("Stockfish: timeout or empty response");
  }

  if (ctx->cancel.load()) {
    ctx->ready.store(true);
    vTaskDelete(nullptr);
    return;
  }

  if (!gotResponse || response.length() == 0) {
    Serial.println("Stockfish: all attempts failed");
    ctx->ready.store(true);
    vTaskDelete(nullptr);
    return;
  }

  // Parse the response
  StockfishResponse sfResp;
  if (StockfishAPI::parseResponse(response, sfResp)) {
    ctx->result.type = EngineResult::MOVE;
    ctx->result.move = std::string(sfResp.bestMove.c_str());

    if (sfResp.hasMate)
      ctx->result.evaluation = sfResp.mateInMoves > 0 ? 100.0f : -100.0f;
    else
      ctx->result.evaluation = sfResp.evaluation;

    Serial.printf("Stockfish: bestMove=%s, eval=%.2f\n",
                  sfResp.bestMove.c_str(), ctx->result.evaluation);
  } else {
    Serial.printf("Stockfish: parse failed: %s\n", sfResp.errorMessage.c_str());
  }

  ctx->ready.store(true);
  vTaskDelete(nullptr);
}
