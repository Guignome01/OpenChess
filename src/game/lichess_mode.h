#ifndef GAME_LICHESS_MODE_H
#define GAME_LICHESS_MODE_H

#include "bot_mode.h"
#include "lichess_api.h"

// Lichess game configuration
struct LichessConfig {
  String apiToken;
};

class LichessMode : public BotMode {
 private:
  LichessConfig lichessConfig_;
  String currentGameId_;

  // Last known state from Lichess
  String lastKnownMoves_;
  // Track last move we sent to avoid processing it as remote move
  String lastSentMove_;

  // Polling state
  unsigned long lastPollTime_;
  static const unsigned long POLL_INTERVAL_MS = 500;

  // Game flow
  void waitForLichessGame();
  void syncBoardWithLichess(const LichessGameState& state);
  void sendMoveToLichess(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

 protected:
  // --- BotMode hooks ---
  void requestEngineMove() override;
  void onPlayerMoveApplied(const MoveResult& result, int fromRow, int fromCol, int toRow, int toCol) override;

  // --- Resign hook (adds Lichess API call) ---
  void onResignConfirmed(char resignColor) override;

 public:
  LichessMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc, LichessConfig cfg);
  void begin() override;
};

#endif // GAME_LICHESS_MODE_H
