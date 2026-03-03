#ifndef GAME_LICHESS_H
#define GAME_LICHESS_H

#include "bot.h"
#include "lichess_api.h"

// Lichess game configuration
struct LichessConfig {
  String apiToken;
};

class ChessLichess : public ChessBot {
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
  // --- ChessBot hooks ---
  void requestEngineMove() override;
  void onPlayerMoveApplied(const MoveResult& result, int fromRow, int fromCol, int toRow, int toCol) override;

  // --- Resign hook (adds Lichess API call) ---
  void onResignConfirmed(char resignColor) override;

 public:
  ChessLichess(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc, LichessConfig cfg);
  void begin() override;
};

#endif // GAME_LICHESS_H
