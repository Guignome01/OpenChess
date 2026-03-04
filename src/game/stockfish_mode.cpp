#include "stockfish_mode.h"
#include "chess_game.h"
#include "led_colors.h"
#include "stockfish_api.h"
#include "utils.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

StockfishMode::StockfishMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, char playerClr, StockfishSettings cfg)
    : BotMode(bd, wm, gc, playerClr), settings_(cfg), currentEvaluation_(0.0f) {}

void StockfishMode::begin() {
  Serial.println("=== Starting Stockfish Mode ===");
  Serial.printf("Player plays: %s\n", playerColor_ == 'w' ? "White" : "Black");
  Serial.printf("Bot plays: %s\n", playerColor_ == 'w' ? "Black" : "White");
  Serial.printf("Bot Difficulty: Depth %d, Timeout %dms\n", settings_.depth, settings_.timeoutMs);
  Serial.println("====================================");
  if (wifiManager_->isWiFiConnected()) {
    if (!tryResumeGame())
      controller_->startNewGame(GameModeId::BOT, playerColor_, (uint8_t)settings_.depth);
    waitForBoardSetup(controller_->getBoard());
  } else {
    Serial.println("Failed to connect to WiFi. Stockfish mode unavailable.");
    boardDriver_->flashBoardAnimation(LedColors::Red);
    controller_->endGame(GameResult::ABORTED, ' ');
    return;
  }
}

String StockfishMode::makeStockfishRequest(const std::string& fen) {
  WiFiSSLClient client;
  client.setInsecure();
  String path = StockfishAPI::buildRequestURL(String(fen.c_str()), settings_.depth);
  Serial.println("Stockfish request: " STOCKFISH_API_URL + path);
  // Retry logic
  for (int attempt = 1; attempt <= settings_.maxRetries; attempt++) {
    if (attempt > 1)
      Serial.println("Attempt: " + String(attempt) + "/" + String(settings_.maxRetries));
    if (client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
      client.println("GET " + path + " HTTP/1.1");
      client.println("Host: " STOCKFISH_API_URL);
      client.println("Connection: close");
      client.println();
      // Wait for response
      unsigned long startTime = millis();
      String response = "";
      bool gotResponse = false;
      while (client.connected() && (millis() - startTime < (unsigned long)settings_.timeoutMs)) {
        if (client.available()) {
          response = client.readString();
          gotResponse = true;
          break;
        }
        delay(10);
      }
      client.stop();

      if (gotResponse && response.length() > 0)
        return response;
    }

    Serial.println("API request timeout or empty response");
    if (attempt < settings_.maxRetries) {
      Serial.println("Retrying...");
      delay(500);
    }
  }

  Serial.println("All API request attempts failed");
  return "";
}

bool StockfishMode::parseStockfishResponse(const String& response, String& bestMove, float& evaluation) {
  StockfishResponse stockfishResp;
  if (!StockfishAPI::parseResponse(response, stockfishResp)) {
    Serial.printf("Failed to parse Stockfish response: %s\n", stockfishResp.errorMessage.c_str());
    return false;
  }
  bestMove = stockfishResp.bestMove;
  if (stockfishResp.hasMate) {
    Serial.printf("Mate in %d moves\n", stockfishResp.mateInMoves);
    // Convert mate to a large evaluation (positive or negative based on direction)
    evaluation = stockfishResp.mateInMoves > 0 ? 100.0f : -100.0f;
  } else {
    // Regular evaluation (already in pawns from API)
    evaluation = stockfishResp.evaluation;
  }
  return true;
}

void StockfishMode::requestEngineMove() {
  Serial.println("=== STOCKFISH MOVE CALCULATION ===");
  startThinking();
  String bestMove;
  String response = makeStockfishRequest(controller_->getFen());
  stopThinking();
  if (parseStockfishResponse(response, bestMove, currentEvaluation_)) {
    Serial.println("=== STOCKFISH EVALUATION ===");
    Serial.printf("%s advantage: %.2f pawns\n", currentEvaluation_ > 0 ? "White" : "Black", currentEvaluation_);

    int fromRow, fromCol, toRow, toCol;
    char promotion;
    if (ChessGame::parseUCIMove(std::string(bestMove.c_str()), fromRow, fromCol, toRow, toCol, promotion)) {
      Serial.printf("Stockfish UCI move: %s = (%d,%d) -> (%d,%d)%s%c\n", bestMove.c_str(), fromRow, fromCol, toRow, toCol, promotion == ' ' ? "" : " Promotion to: ", promotion);
      Serial.println("============================");
      // Verify the move is from the correct color piece
      char piece = controller_->getSquare(fromRow, fromCol);
      char engineColor = ChessUtils::opponentColor(playerColor_);
      bool isEnginePiece = (engineColor == 'w') ? (piece >= 'A' && piece <= 'Z') : (piece >= 'a' && piece <= 'z');
      if (!isEnginePiece) {
        Serial.printf("ERROR: Engine tried to move a %s piece, but engine plays %s. Piece at source: %c\n", (piece >= 'A' && piece <= 'Z') ? "WHITE" : "BLACK", engineColor == 'w' ? "WHITE" : "BLACK", piece);
        return;
      }
      if (piece == ' ') {
        Serial.println("ERROR: Engine tried to move from an empty square!");
        return;
      }
      applyMove(fromRow, fromCol, toRow, toCol, promotion, true);
    } else {
      Serial.println("Failed to parse Stockfish UCI move: " + bestMove);
    }
  }
}
