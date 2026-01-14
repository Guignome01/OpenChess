#ifndef CHESS_BOT_H
#define CHESS_BOT_H

#include "arduino_secrets.h"
#include "board_driver.h"
#include "chess_engine.h"
#include "chess_utils.h"
#include "stockfish_api.h"
#include "stockfish_settings.h"

// Forward declaration to avoid circular dependency
class WiFiManagerESP32;

// Platform-specific WiFi includes
#if defined(ESP32) || defined(ESP8266)
// ESP32/ESP8266 use built-in WiFi libraries
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define WiFiSSLClient WiFiClientSecure
#else
// Other boards - WiFi not supported
#warning "WiFi not supported on this board - Chess Bot will not work"
#endif

class ChessBot {
 private:
  BoardDriver* _boardDriver;
  ChessEngine* _chessEngine;
  WiFiManagerESP32* _wifiManager;

  char board[8][8];
  const char INITIAL_BOARD[8][8] = {
      {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, // row 0 = rank 8 (black pieces, top row)
      {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // row 1 = rank 7 (black pawns)
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 2 = rank 6
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 3 = rank 5
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 4 = rank 4
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 5 = rank 3
      {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, // row 6 = rank 2 (white pawns)
      {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  // row 7 = rank 1 (White pieces, bottom row)
  };

  BotConfig botConfig;

  bool isWhiteTurn;
  bool gameStarted;
  bool gameOver;
  bool botThinking;
  bool wifiConnected;
  float currentEvaluation; // Stockfish evaluation (in pawns, positive = white advantage)

  // Castling rights bitmask (KQkq = 0b1111)
  uint8_t castlingRights;

  // WiFi and API
  String makeStockfishRequest(String fen);
  bool parseStockfishResponse(String response, String& bestMove, float& evaluation);

  // Move handling
  void executeBotMove(int fromRow, int fromCol, int toRow, int toCol);

  // Game flow
  void initializeBoard();
  void waitForBoardSetup();
  void processPlayerMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
  void makeBotMove();
  void showBotThinking();
  void showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol, bool isCapture);
  void waitForBotMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture);
  void waitForBotCastlingCompletion(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol);

 public:
  ChessBot(BoardDriver* boardDriver, ChessEngine* chessEngine, WiFiManagerESP32* _wifiManager, BotConfig botConfig);
  void begin();
  void update();

  // Get current board state for WiFi display
  void getBoardState(char boardState[8][8]);

  // Set board state for editing/corrections
  void setBoardState(char newBoardState[8][8]);

  // Get current evaluation
  float getEvaluation() { return currentEvaluation; }
};

#endif // CHESS_BOT_H
