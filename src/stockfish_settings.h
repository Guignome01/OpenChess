#ifndef STOCKFISH_SETTINGS_H
#define STOCKFISH_SETTINGS_H

// Stockfish Engine Settings
struct StockfishSettings {
  int depth;      // Search depth (5-15, higher = stronger but slower)
  int timeoutMs;  // API timeout in milliseconds
  int maxRetries; // Max API call retries on failure

  StockfishSettings(int depth = 5, int timeoutMs = 60000, int maxRetries = 3) : depth(depth), timeoutMs(timeoutMs), maxRetries(maxRetries) {}

  // Difficulty presets (8 levels, depth 3–17)
  static StockfishSettings beginner()     { return {3,  10000}; }
  static StockfishSettings easy()         { return {5,  15000}; }
  static StockfishSettings intermediate() { return {7,  20000}; }
  static StockfishSettings medium()       { return {9,  25000}; }
  static StockfishSettings advanced()     { return {11, 35000}; }
  static StockfishSettings hard()         { return {13, 45000}; }
  static StockfishSettings expert()       { return {15, 55000}; }
  static StockfishSettings master()       { return {17, 65000}; }

  /// Get preset by 1-based difficulty level (1–8). Defaults to medium.
  static StockfishSettings fromLevel(int level) {
    switch (level) {
      case 1: return beginner();
      case 2: return easy();
      case 3: return intermediate();
      case 4: return medium();
      case 5: return advanced();
      case 6: return hard();
      case 7: return expert();
      case 8: return master();
      default: return medium();
    }
  }
};

// Bot configuration structure
struct BotConfig {
  StockfishSettings stockfishSettings;
  bool playerIsWhite;
};

#endif // STOCKFISH_SETTINGS_H