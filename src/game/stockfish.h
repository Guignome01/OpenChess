#ifndef STOCKFISH_H
#define STOCKFISH_H

#include "bot.h"
#include "stockfish_api.h"
#include "stockfish_settings.h"

// ESP32 WiFi includes
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define WiFiSSLClient WiFiClientSecure

class ChessStockfish : public ChessBot {
 private:
  StockfishSettings settings;
  float currentEvaluation;

  // Stockfish API
  String makeStockfishRequest(const std::string& fen);
  bool parseStockfishResponse(const String& response, String& bestMove, float& evaluation);

 protected:
  // ChessBot hooks
  void requestEngineMove() override;
  float getEngineEvaluation() override { return currentEvaluation; }

 public:
  ChessStockfish(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc, char playerColor, StockfishSettings settings);
  void begin() override;
};

#endif  // STOCKFISH_H
