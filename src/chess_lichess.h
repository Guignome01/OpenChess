#ifndef CHESS_LICHESS_H
#define CHESS_LICHESS_H

#include "chess_bot.h"
#include "lichess_api.h"

// Lichess game configuration
struct LichessConfig {
  String apiToken;
};

class ChessLichess : public ChessBot {
 private:
  LichessConfig lichessConfig;
  String currentGameId;
  char myColor; // 'w' or 'b' - the color we play as

  // Last known state from Lichess
  String lastKnownMoves;
  // Track last move we sent to avoid processing it as remote move
  String lastSentMove;

  // Polling state
  unsigned long lastPollTime;
  static const unsigned long POLL_INTERVAL_MS = 500;

  // Game flow
  void waitForLichessGame();
  void syncBoardWithLichess(const LichessGameState& state);
  void processLichessMove(const String& uciMove);
  void sendMoveToLichess(int fromRow, int fromCol, int toRow, int toCol, char promotedPiece = ' ');

 public:
  ChessLichess(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, LichessConfig cfg);
  void begin() override;
  void update() override;
};

#endif // CHESS_LICHESS_H
