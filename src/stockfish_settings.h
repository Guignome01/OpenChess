#ifndef STOCKFISH_SETTINGS_H
#define STOCKFISH_SETTINGS_H

// Stockfish Engine Settings
struct StockfishSettings {
  int depth = 5;         // Search depth (5-15, higher = stronger but slower)
  int timeoutMs = 15000; // API timeout in milliseconds (15 seconds)
  int maxRetries = 3;    // Max API call retries on failure

  // Difficulty presets
  static StockfishSettings easy() {
    StockfishSettings s;
    s.depth = 5;
    s.timeoutMs = 15000;
    return s;
  }

  static StockfishSettings medium() {
    StockfishSettings s;
    s.depth = 8;
    s.timeoutMs = 25000;
    return s;
  }

  static StockfishSettings hard() {
    StockfishSettings s;
    s.depth = 11;
    s.timeoutMs = 45000;
    return s;
  }

  static StockfishSettings expert() {
    StockfishSettings s;
    s.depth = 15;
    s.timeoutMs = 60000;
    return s;
  }
};

// Bot configuration structure
struct BotConfig {
  StockfishSettings stockfishSettings;
  bool playerIsWhite;
};

#endif