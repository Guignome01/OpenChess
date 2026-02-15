#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include "board_driver.h"
#include "chess_engine.h"
#include "chess_utils.h"
#include "led_colors.h"
#include <Arduino.h>

// Forward declarations to avoid circular dependencies
class WiFiManagerESP32;
class MoveHistory;

// Base class for chess game modes (shared state and common functionality)
class ChessGame {
  friend class MoveHistory; // MoveHistory needs access to applyMove/advanceTurn for replay
 protected:
  BoardDriver* boardDriver;
  ChessEngine* chessEngine;
  WiFiManagerESP32* wifiManager;
  MoveHistory* moveHistory; // nullptr for Lichess mode (moves already recorded on Lichess cloud)

  char board[8][8];
  char currentTurn; // 'w' or 'b'
  bool gameOver;
  bool replaying; // True while replaying moves during resume (suppresses LEDs and physical move waits)

  // Standard initial chess board setup
  static const char INITIAL_BOARD[8][8];

  // Constructor
  ChessGame(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, MoveHistory* mh);

  // Common initialization and game flow methods
  void initializeBoard();
  void waitForBoardSetup(const char targetBoard[8][8]);
  void applyMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ', bool isRemoteMove = false);
  bool tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol);
  void updateGameStatus();

  // Chess rule helpers
  void updateCastlingRightsAfterMove(int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece);
  void applyCastling(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece, bool waitForKingCompletion = false);
  void confirmSquareCompletion(int row, int col);

  // Virtual hooks for remote move handling (overridden in subclasses)
  virtual void waitForRemoteMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant = false, int enPassantCapturedPawnRow = -1) {}

 public:
  virtual ~ChessGame() {}

  virtual void begin() = 0;
  virtual void update() = 0;

  void setBoardStateFromFEN(const String& fen);
  bool isGameOver() const { return gameOver; }

  // Advance turn and record position (extracted from updateGameStatus for replay use)
  void advanceTurn();
};

#endif // CHESS_GAME_H
