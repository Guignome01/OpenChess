#ifndef GAME_STOCKFISH_MODE_H
#define GAME_STOCKFISH_MODE_H

#include "bot_mode.h"
#include "stockfish_api.h"
#include "stockfish_settings.h"

// ESP32 WiFi includes
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define WiFiSSLClient WiFiClientSecure

class StockfishMode : public BotMode {
 private:
  StockfishSettings settings_;
  float currentEvaluation_;

  // Stockfish API
  String makeStockfishRequest(const std::string& fen);
  bool parseStockfishResponse(const String& response, String& bestMove, float& evaluation);

 protected:
  // BotMode hooks
  void requestEngineMove() override;
  float getEngineEvaluation() override { return currentEvaluation_; }

 public:
  StockfishMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, char playerColor, StockfishSettings settings);
  void begin() override;
};

#endif  // GAME_STOCKFISH_MODE_H
