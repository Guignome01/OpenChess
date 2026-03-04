#ifndef GAME_BOT_MODE_H
#define GAME_BOT_MODE_H

#include "game_mode.h"
#include <atomic>

// Abstract intermediate class for all remote-engine game modes (human vs engine).
// Uses the Template Method pattern: update() defines the game loop skeleton,
// subclasses override only the engine-specific hooks.
//
// Hierarchy: GameMode → BotMode → StockfishMode / LichessMode
//
class BotMode : public GameMode {
 protected:
  char playerColor_;                    // 'w' or 'b' — the color the local player controls
  std::atomic<bool>* thinkingAnimation_;  // Thinking animation stop flag (nullptr when not running)

  BotMode(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc, char playerColor);

  // --- Template Method hooks (override in subclasses) ---

  // Called when it's the engine's turn. Subclass should compute/fetch the engine
  // move and call applyMove() with isRemoteMove=true.
  virtual void requestEngineMove() = 0;

  // Called after a local player move is applied via applyMove().
  // Subclass can react (e.g., send the move to a remote server).
  // Default: no-op.
  virtual void onPlayerMoveApplied(const MoveResult& result, int fromRow, int fromCol, int toRow, int toCol) {}

  // Return the engine's evaluation for relay to the web UI.
  // Default: material evaluation from ChessBoard.
  virtual float getEngineEvaluation();

  // --- Resign hooks ---
  bool isFlipped() const override { return playerColor_ == 'b'; }
  void onBeforeResignConfirm() override;
  void onResignCancelled() override;

  // --- Thinking animation helpers ---
  void startThinking();
  void stopThinking();

  bool wasThinkingBeforeResign_;  // Preserved across resign dialog

 private:
  // Concrete implementation of remote move guidance (LED + sensor blocking).
  // Shared by all engine subclasses.
  void waitForRemoteMoveCompletion(int fromRow, int fromCol, int toRow, int toCol, bool isCapture, bool isEnPassant = false, int enPassantCapturedPawnRow = -1) override;

 public:
  ~BotMode() override {}

  void update() override;
};

#endif  // GAME_BOT_MODE_H
