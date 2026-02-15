#ifndef MOVE_HISTORY_H
#define MOVE_HISTORY_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

// Forward declaration
class ChessGame;

enum GameResult : uint8_t {
  RESULT_IN_PROGRESS = 0,
  RESULT_CHECKMATE = 1,
  RESULT_STALEMATE = 2,
  RESULT_DRAW_50 = 3,
  RESULT_DRAW_3FOLD = 4,
  RESULT_RESIGNATION = 5
};

enum GameModeCode : uint8_t {
  GAME_MODE_CHESS_MOVES = 1,
  GAME_MODE_BOT = 2
};

struct __attribute__((packed)) GameHeader {
  uint8_t version;        // Format version (currently 1)
  uint8_t mode;           // GameModeCode
  uint8_t result;         // GameResult
  uint8_t winnerColor;    // 'w', 'b', 'd' (draw), '?' (in-progress)
  uint8_t playerColor;    // For bot mode: human's color ('w'/'b'), '?' for ChessMoves
  uint8_t botDepth;       // For bot mode: Stockfish depth, 0 for ChessMoves
  uint16_t moveCount;     // Number of 2-byte entries (incl. FEN markers)
  uint16_t fenEntryCnt;   // Number of FEN table entries
  uint16_t lastFenOffset; // Byte offset of the last FEN entry within the FEN table
  uint32_t timestamp;     // Unix epoch (from NTP, 0 if unavailable)
};
static_assert(sizeof(GameHeader) == 16, "GameHeader must be 16 bytes");

class MoveHistory {
 public:
  MoveHistory();

  // Call after LittleFS is mounted to create the /games directory
  void begin();

  // Call once when a new game begins (writes header + starting FEN)
  void startGame(uint8_t mode, uint8_t playerColor = '?', uint8_t botDepth = 0);

  // Append a move (2-byte UCI encoding) to the live file
  void addMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Append a FEN marker to the live moves file and write the FEN string into the live FEN table file
  void addFen(const String& fen);

  // Finalize the live game: update header, merge FEN table, rename to a completed-game file, enforce storage limits
  void finishGame(uint8_t result, char winnerColor);

  void discardLiveGame();

  bool isRecording() const { return recording; }

  // Returns true if live.bin exists on flash
  bool hasLiveGame();

  // Read mode/config from the live header (for mode selection)
  bool getLiveGameInfo(uint8_t& mode, uint8_t& playerColor, uint8_t& botDepth);

  // Replay the live game into a ChessGame instance:
  //  1. Finds the last FEN marker and its FEN string
  //  2. Calls game->setBoardStateFromFEN() with that FEN
  //  3. Replays all UCI moves after the last FEN using game->applyMove()
  // Recording is suppressed automatically during replay
  bool replayIntoGame(ChessGame* game);

  // JSON array of game metadata (id, mode, result, timestamp …)
  String getGameListJSON();

  // Delete a single completed game by id
  bool deleteGame(int id);
  // LittleFS.exists() wrapper that suppresses noisy vfs_api log output
  static bool quietExists(const char* path);

  // Build the path string for a given game id
  static String gamePath(int id);

  // Delete oldest games until count ≤ MAX_GAMES and LittleFS usage ≤ MAX_USAGE_PERCENT
  void enforceStorageLimits();

  // Encode a move into 2 bytes: [from(6)][to(6)][promo(4)]
  static uint16_t encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion);

  // Decode 2 bytes back into row/col/promotion
  static void decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

 private:
  bool recording;
  GameHeader header;

  static constexpr const char* GAMES_DIR = "/games";
  static constexpr const char* LIVE_MOVES_PATH = "/games/live.bin";
  static constexpr const char* LIVE_FEN_PATH = "/games/live_fen.bin";
  static constexpr int MAX_GAMES = 50;
  static constexpr float MAX_USAGE_PERCENT = 0.80f;
  static constexpr uint8_t FORMAT_VERSION = 1;
  static constexpr uint16_t FEN_MARKER = 0xFFFF;

  // Map promotion character to 4-bit code and back
  static uint8_t promoCharToCode(char p);
  static char promoCodeToChar(uint8_t code);

  // Rewrite the header stored at offset 0 of live.bin
  void updateLiveHeader();

  // Find the lowest available game id (1-based)
  int nextGameId();

  // Collect sorted list of existing game ids
  std::vector<int> listGameIds();

  // Obtain a Unix timestamp (returns 0 if NTP has not synced)
  static uint32_t getTimestamp();
};

#endif // MOVE_HISTORY_H
