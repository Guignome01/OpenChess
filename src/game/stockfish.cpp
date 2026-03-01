#include "stockfish.h"
#include "codec.h"
#include "led_colors.h"
#include "move_history.h"
#include "stockfish_api.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

ChessStockfish::ChessStockfish(BoardDriver* bd, WiFiManagerESP32* wm, MoveHistory* mh, char playerClr, StockfishSettings cfg)
    : ChessBot(bd, wm, mh, playerClr), settings(cfg), currentEvaluation(0.0f) {}

void ChessStockfish::begin() {
  Serial.println("=== Starting Stockfish Mode ===");
  Serial.printf("Player plays: %s\n", playerColor == 'w' ? "White" : "Black");
  Serial.printf("Bot plays: %s\n", playerColor == 'w' ? "Black" : "White");
  Serial.printf("Bot Difficulty: Depth %d, Timeout %dms\n", settings.depth, settings.timeoutMs);
  Serial.println("====================================");
  if (wifiManager->isWiFiConnected()) {
    initializeBoard();
    if (!tryResumeGame()) {
      moveHistory->startGame(GAME_MODE_BOT, playerColor, (uint8_t)settings.depth);
      moveHistory->addFen(gm_.getFen());
    }
    waitForBoardSetup(gm_.getBoard());
  } else {
    Serial.println("Failed to connect to WiFi. Stockfish mode unavailable.");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gm_.endGame(RESULT_ABORTED, ' ');
    return;
  }
}

String ChessStockfish::makeStockfishRequest(const std::string& fen) {
  WiFiSSLClient client;
  client.setInsecure();
  String path = StockfishAPI::buildRequestURL(String(fen.c_str()), settings.depth);
  Serial.println("Stockfish request: " STOCKFISH_API_URL + path);
  // Retry logic
  for (int attempt = 1; attempt <= settings.maxRetries; attempt++) {
    if (attempt > 1)
      Serial.println("Attempt: " + String(attempt) + "/" + String(settings.maxRetries));
    if (client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
      client.println("GET " + path + " HTTP/1.1");
      client.println("Host: " STOCKFISH_API_URL);
      client.println("Connection: close");
      client.println();
      // Wait for response
      unsigned long startTime = millis();
      String response = "";
      bool gotResponse = false;
      while (client.connected() && (millis() - startTime < (unsigned long)settings.timeoutMs)) {
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
    if (attempt < settings.maxRetries) {
      Serial.println("Retrying...");
      delay(500);
    }
  }

  Serial.println("All API request attempts failed");
  return "";
}

bool ChessStockfish::parseStockfishResponse(const String& response, String& bestMove, float& evaluation) {
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

void ChessStockfish::requestEngineMove() {
  Serial.println("=== STOCKFISH MOVE CALCULATION ===");
  startThinking();
  String bestMove;
  String response = makeStockfishRequest(gm_.getFen());
  stopThinking();
  if (parseStockfishResponse(response, bestMove, currentEvaluation)) {
    Serial.println("=== STOCKFISH EVALUATION ===");
    Serial.printf("%s advantage: %.2f pawns\n", currentEvaluation > 0 ? "White" : "Black", currentEvaluation);

    int fromRow, fromCol, toRow, toCol;
    char promotion;
    if (ChessCodec::parseUCIMove(std::string(bestMove.c_str()), fromRow, fromCol, toRow, toCol, promotion)) {
      Serial.printf("Stockfish UCI move: %s = (%d,%d) -> (%d,%d)%s%c\n", bestMove.c_str(), fromRow, fromCol, toRow, toCol, promotion == ' ' ? "" : " Promotion to: ", promotion);
      Serial.println("============================");
      // Verify the move is from the correct color piece
      char piece = gm_.getSquare(fromRow, fromCol);
      char engineColor = (playerColor == 'w') ? 'b' : 'w';
      bool isEnginePiece = (engineColor == 'w') ? (piece >= 'A' && piece <= 'Z') : (piece >= 'a' && piece <= 'z');
      if (!isEnginePiece) {
        Serial.printf("ERROR: Engine tried to move a %s piece, but engine plays %s. Piece at source: %c\n", (piece >= 'A' && piece <= 'Z') ? "WHITE" : "BLACK", engineColor == 'w' ? "WHITE" : "BLACK", piece);
        return;
      }
      if (piece == ' ') {
        Serial.println("ERROR: Engine tried to move from an empty square!");
        return;
      }
      applyMove(fromRow, fromCol, toRow, toCol, (bestMove.length() >= 5) ? bestMove[4] : ' ', true);
    } else {
      Serial.println("Failed to parse Stockfish UCI move: " + bestMove);
    }
  }
}
