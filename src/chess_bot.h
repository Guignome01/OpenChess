#ifndef CHESS_BOT_H
#define CHESS_BOT_H

#include "chess_game.h"
#include "chess_utils.h"
#include "stockfish_api.h"
#include "stockfish_settings.h"

// ESP32 WiFi includes
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define WiFiSSLClient WiFiClientSecure

class ChessBot : public ChessGame {
 private:
  BotConfig botConfig;

  bool wifiConnected;
  float currentEvaluation; // Stockfish evaluation (in pawns, positive = white advantage)

  // WiFi and API
  String makeStockfishRequest(String fen);
  bool parseStockfishResponse(String response, String& bestMove, float& evaluation);

  // Move handling
  void executeBotMove(int fromRow, int fromCol, int toRow, int toCol);

  // Game flow
  void makeBotMove();
  void showBotThinking();
  void showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant = false, int enPassantCapturedPawnRow = -1);
  void waitForBotMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant = false, int enPassantCapturedPawnRow = -1);
  void waitForBotCastlingCompletion(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol);

 public:
  ChessBot(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, BotConfig cfg);
  void begin() override;
  void update() override;

  // Get current evaluation
  float getEvaluation() { return currentEvaluation; }
};

#endif // CHESS_BOT_H
