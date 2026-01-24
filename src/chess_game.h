#ifndef CHESS_COMMON_H
#define CHESS_COMMON_H

#include "board_driver.h"
#include "chess_engine.h"
#include "chess_utils.h"
#include "led_colors.h"
#include <Arduino.h>

// Forward declaration to avoid circular dependency
class WiFiManagerESP32;

// Base class for chess game modes (shared state and common functionality)
class ChessGame {
 protected:
  BoardDriver* boardDriver;
  ChessEngine* chessEngine;
  WiFiManagerESP32* wifiManager;

  char board[8][8];
  char currentTurn; // 'w' or 'b'
  bool gameOver;

  // Standard initial chess board setup
  static const char INITIAL_BOARD[8][8];

  // Constructor
  ChessGame(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm);

  // Common initialization and game flow methods
  void initializeBoard();
  void waitForBoardSetup();
  void processPlayerMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
  bool tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol, char& movedPiece);
  void updateGameStatus();

  // Chess rule helpers
  void recomputeCastlingRightsFromBoard();
  void updateCastlingRightsAfterMove(int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece);
  void applyCastling(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece);
  bool applyPawnPromotionIfNeeded(int toRow, int toCol, char movedPiece, char& promotedPieceOut);
  bool findKingPosition(char colorToMove, int& kingRow, int& kingCol);
  void confirmSquareCompletion(int row, int col);

  // En passant helpers
  char applyEnPassant(int toRow, int toCol, char piece);
  void updateEnPassantTarget(int fromRow, int fromCol, int toRow, char piece);

 public:
  virtual ~ChessGame() {}

  virtual void begin() = 0;
  virtual void update() = 0;

  void setBoardState(char newBoardState[8][8]);
  void setBoardStateFromFEN(const String& fen);
  bool isGameOver() const { return gameOver; }
};

#endif // CHESS_COMMON_H
