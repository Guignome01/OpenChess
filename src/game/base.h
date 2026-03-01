#ifndef BASE_H
#define BASE_H

#include "board_driver.h"
#include "board_menu.h"
#include "board.h"
#include "led_colors.h"
#include <Arduino.h>

// Forward declarations to avoid circular dependencies
class WiFiManagerESP32;
class MoveHistory;

// Base class for chess game modes (shared state and common functionality)
class ChessGame {
 protected:
  BoardDriver* boardDriver;
  WiFiManagerESP32* wifiManager;
  MoveHistory* moveHistory; // nullptr for Lichess mode (moves already recorded on Lichess cloud)

  ChessBoard gm_;

  bool replaying; // True while replaying moves during resume (suppresses LEDs and physical move waits)

  // --- Resign ---
  static constexpr unsigned long RESIGN_HOLD_MS = 3000;       // Duration king must stay off its square to initiate resign
  static constexpr unsigned long RESIGN_LIFT_WINDOW_MS = 1000; // Max time per quick lift during gesture
  bool resignPending = false;    // Set by web resign endpoint

  // Constructor
  ChessGame(BoardDriver* bd, WiFiManagerESP32* wm, MoveHistory* mh);

  // Common initialization and game flow methods
  void initializeBoard();
  void waitForBoardSetup(const char targetBoard[8][8]);
  MoveResult applyMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ', bool isRemoteMove = false);
  bool tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol);

  /// Try to resume a live game from MoveHistory. Returns true if resumed.
  /// If no live game exists, returns false (caller should start a new game).
  bool tryResumeGame();

  // --- Resign ---
  /// Unified resign entry point. Call at the start of update() after readSensors().
  /// Returns true if the game loop should return early.
  bool processResign();
  /// Show standard invalid-move feedback (red blink) on a square.
  void showIllegalMoveFeedback(int row, int col);
  /// Handle resign confirmation and game-end sequence.
  /// Virtual so ChessLichess can add API call and animation management.
  virtual bool handleResign(char resignColor);

 private:
  /// Blocking loop for the 2 quick lifts after the initial king return.
  /// Called inline from tryPlayerMove(). Returns true if resign was confirmed.
  bool continueResignGesture(int row, int col, char color);
  /// Show scaled orange resign indicator on a square (level 0–3 maps to RESIGN_BRIGHTNESS_LEVELS).
  /// If clearFirst is true, clears all LEDs before setting the indicator.
  void showResignProgress(int row, int col, int level, bool clearFirst = false);
  /// Turn off the resign indicator LED on a square.
  void clearResignFeedback(int row, int col);

  // Hardware-only castling interactions (board already updated by ChessBoard)
  void applyCastlingHardware(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece, bool waitForKingCompletion = false);
  void confirmSquareCompletion(int row, int col);

  // Virtual hooks for remote move handling (overridden in subclasses)
  virtual void waitForRemoteMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant = false, int enPassantCapturedPawnRow = -1) {}

 public:
  virtual ~ChessGame() {}

  virtual void begin() = 0;
  virtual void update() = 0;

  void setBoardStateFromFEN(const std::string& fen);
  bool isGameOver() const { return gm_.isGameOver(); }
  void setResignPending(bool pending) { resignPending = pending; }

  // --- Replay support (used by MoveHistory) ---
  void beginReplay();
  void endReplay();
  MoveResult replayMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');
};

#endif // BASE_H
