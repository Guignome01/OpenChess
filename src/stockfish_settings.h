#ifndef STOCKFISH_SETTINGS_H
#define STOCKFISH_SETTINGS_H

// Stockfish Engine Settings
struct StockfishSettings {
  int depth;      // Search depth (5-15, higher = stronger but slower)
  int timeoutMs;  // API timeout in milliseconds
  int maxRetries; // Max API call retries on failure

  StockfishSettings(int depth = 5, int timeoutMs = 60000, int maxRetries = 3) : depth(depth), timeoutMs(timeoutMs), maxRetries(maxRetries) {}

  // Difficulty presets
  static StockfishSettings easy() { return {5, 15000}; }
  static StockfishSettings medium() { return {8, 25000}; }
  static StockfishSettings hard() { return {11, 45000}; }
  static StockfishSettings expert() { return {15, 60000}; }
};

// Bot configuration structure
struct BotConfig {
  StockfishSettings stockfishSettings;
  bool playerIsWhite;
};

#endif // STOCKFISH_SETTINGS_H