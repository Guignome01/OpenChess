#ifndef LICHESS_API_H
#define LICHESS_API_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

// Lichess API Configuration
#define LICHESS_API_HOST "lichess.org"
#define LICHESS_API_PORT 443

// Lichess game state
struct LichessGameState {
  String gameId;
  String fen;
  String lastMove; // UCI format (e.g., "e2e4")
  int moveCount = 0; // Total half-moves in the game
  bool isMyTurn;
  char myColor; // 'w' or 'b'
  bool gameStarted;
  bool gameEnded;
  String winner; // "white", "black", "draw", or empty if ongoing
  String status; // "started", "mate", "resign", "stalemate", etc.
};

// Lichess game event types
enum class LichessEventType {
  GAME_START,
  GAME_FINISH,
  CHALLENGE,
  CHALLENGE_CANCELED,
  CHALLENGE_DECLINED,
  UNKNOWN
};

// Lichess event structure
struct LichessEvent {
  LichessEventType type;
  String gameId;
  String fen;
  char myColor; // 'w' or 'b'
};

class LichessAPI {
 public:
  // Set the API token (Personal Access Token)
  static void setToken(const String& token);
  static String getToken();
  static bool hasToken();

  // Account verification
  static bool verifyToken(String& username);

  // Stream events to find new games
  // Returns true if a new game event was found
  static bool pollForGameEvent(LichessEvent& event);

  // Stream game state (for ongoing game updates)
  // Returns true if there's new state data
  static bool pollGameStream(const String& gameId, LichessGameState& state);

  // Make a move in the current game
  // move: UCI format (e.g., "e2e4", "e7e8q" for promotion)
  static bool makeMove(const String& gameId, const String& move);

  // Resign the game
  static bool resignGame(const String& gameId);

  // --- Persistent stream helpers ---

  // Open a persistent TLS connection to the game stream endpoint.
  // Skips HTTP headers. Returns true if the connection is ready to read NDJSON lines.
  static bool connectGameStream(WiFiClientSecure& client, const String& gameId);

  // Read the next NDJSON event from an open stream connection.
  // Skips heartbeat (empty) lines and chunked-encoding size lines.
  // Returns true if a JSON line was read into `state`; false on timeout or disconnect.
  // `cancel` is polled to allow early exit.
  static bool readStreamEvent(WiFiClientSecure& client, LichessGameState& state,
                              const std::atomic<bool>& cancel, unsigned long timeoutMs = 30000);

 private:
  static String apiToken;
  static String makeHttpRequest(const String& method, const String& path, const String& body = "");
  static bool parseGameFullEvent(const String& json, LichessGameState& state);
  static bool parseGameStateEvent(const String& json, LichessGameState& state);
};

#endif // LICHESS_API_H
