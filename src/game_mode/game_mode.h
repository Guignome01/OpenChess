#ifndef GAME_MODE_H
#define GAME_MODE_H

#include "board_driver.h"
#include "led_colors.h"
#include "types.h"
#include <Arduino.h>

// Forward declarations to avoid circular dependencies
class WiFiManagerESP32;
class ChessGame;

// Base class for chess game modes (shared state and common functionality).
// All chess-state mutations flow through `chess_` (ChessGame orchestrator),
// which atomically updates the board, records moves, and notifies observers.
class GameMode {
 protected:
  BoardDriver* boardDriver_;
  WiFiManagerESP32* wifiManager_;
  ChessGame* chess_;

  // --- Resign ---
  static constexpr unsigned long RESIGN_HOLD_MS = 3000;       // Duration king must stay off its square to initiate resign
  static constexpr unsigned long RESIGN_LIFT_WINDOW_MS = 1000; // Max time per quick lift during gesture
  bool resignPending_ = false;    // Set by web resign endpoint

  // Constructor
  GameMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* cg);

  // Common initialization and game flow methods
  void waitForBoardSetup(const char targetBoard[8][8]);
  MoveResult applyMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ', bool isRemoteMove = false);
  MoveResult applyMove(const std::string& move);
  bool tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol);

  /// Try to resume a live game from ChessGame. Returns true if resumed.
  /// If no live game exists, returns false (caller should start a new game).
  bool tryResumeGame();

  // --- Resign ---
  /// Unified resign entry point. Call at the start of update() after readSensors().
  /// Returns true if the game loop should return early.
  bool processResign();
  /// Show standard invalid-move feedback (red blink) on a square.
  void showIllegalMoveFeedback(int row, int col);
  /// Handle resign confirmation and game-end sequence.
  /// Uses virtual hooks so subclasses can customize behavior without
  /// duplicating the flow.
  bool handleResign(char resignColor);

  // --- Resign hooks (override in subclasses) ---

  /// Board orientation for the confirm dialog (true = black at bottom).
  /// Default: white at bottom.
  virtual bool isFlipped() const { return false; }
  /// Called before the confirm dialog (e.g. stop thinking animation).
  virtual void onBeforeResignConfirm() {}
  /// Called when the user cancels the resign (e.g. restart thinking).
  virtual void onResignCancelled() {}
  /// Called after resign is confirmed, before endGame (e.g. API calls).
  virtual void onResignConfirmed(char resignColor) {}

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
  virtual ~GameMode() {}

  virtual void begin() = 0;
  virtual void update() = 0;

  void setBoardStateFromFEN(const std::string& fen);
  bool isGameOver() const;
  void setResignPending(bool pending) { resignPending_ = pending; }
  virtual bool isNavigationAllowed() const { return true; }
};

#endif // GAME_MODE_H
