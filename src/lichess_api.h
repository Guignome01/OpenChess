#ifndef LICHESS_API_H
#define LICHESS_API_H

#include <Arduino.h>

// Lichess API Configuration
#define LICHESS_API_HOST "lichess.org"
#define LICHESS_API_PORT 443

// Lichess game state
struct LichessGameState {
  String gameId;
  String fen;
  String lastMove; // UCI format (e.g., "e2e4")
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

  // Get current game state
  static bool getGameState(const String& gameId, LichessGameState& state);

  // Stream game state (for ongoing game updates)
  // Returns true if there's new state data
  static bool pollGameStream(const String& gameId, LichessGameState& state);

  // Make a move in the current game
  // move: UCI format (e.g., "e2e4", "e7e8q" for promotion)
  static bool makeMove(const String& gameId, const String& move);

  // Resign the game
  static bool resignGame(const String& gameId);

  // Convert array coordinates to UCI format
  static String toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

 private:
  static String apiToken;
  static String makeHttpRequest(const String& method, const String& path, const String& body = "");
  static bool parseGameFullEvent(const String& json, LichessGameState& state);
  static bool parseGameStateEvent(const String& json, LichessGameState& state);
  static bool parseStreamEvent(const String& json, LichessEvent& event);
};

#endif // LICHESS_API_H
