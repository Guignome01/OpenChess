#ifndef CORE_GAME_CONTROLLER_H
#define CORE_GAME_CONTROLLER_H

#include <string>

#include "board.h"
#include "game_observer.h"
#include "recorder.h"
#include "types.h"

// Facade that coordinates ChessBoard + GameRecorder + IGameObserver.
// Game classes call GameController for all chess-state mutations;
// recording and UI notification happen automatically.
//
// Nullable dependencies:
//   - recorder: nullptr disables recording (e.g. Lichess mode).
//   - observer: nullptr disables UI notification.
class GameController {
 public:
  GameController(GameRecorder* recorder = nullptr,
                 IGameObserver* observer = nullptr);

  // --- Lifecycle ---

  void newGame();

  // Start a new game and begin recording. Resets the board, then starts recording.
  void startNewGame(GameMode mode, uint8_t playerColor = '?', uint8_t botDepth = 0);

  // Start recording (delegates to GameRecorder + auto-snapshots initial FEN).
  void startRecording(GameMode mode, uint8_t playerColor = '?', uint8_t botDepth = 0);

  // End the game — finishes recording (if active) and sets board game-over state.
  void endGame(GameResult result, char winnerColor);

  // Discard current recording without finalizing.
  void discardRecording();

  // --- Mutations ---

  // Validate and execute a move.  Atomically: applies to board, records move,
  // auto-finishes recording on game-end, notifies observer.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Load arbitrary position from FEN.  Records FEN if recording, notifies observer.
  bool loadFEN(const std::string& fen);

  // --- Replay ---

  // Restore a saved game from storage into the board.
  bool resumeGame();

  // --- Resume queries ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameMode& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Read-only pass-throughs to ChessBoard ---

  const char (&getBoard() const)[8][8] { return board_.getBoard(); }
  char getSquare(int row, int col) const { return board_.getSquare(row, col); }
  char currentTurn() const { return board_.currentTurn(); }
  bool isGameOver() const { return board_.isGameOver(); }
  GameResult gameResult() const { return board_.gameResult(); }
  char winnerColor() const { return board_.winnerColor(); }
  uint8_t getCastlingRights() const { return board_.getCastlingRights(); }
  const PositionState& positionState() const { return board_.positionState(); }
  std::string getFen() const { return board_.getFen(); }
  float getEvaluation() const { return board_.getEvaluation(); }

  // State-change callback (delegates to ChessBoard).
  void onStateChanged(ChessBoard::StateCallback cb) { board_.onStateChanged(cb); }

 private:
  ChessBoard board_;
  GameRecorder* recorder_;
  IGameObserver* observer_;

  void notifyObserver();
};

#endif  // CORE_GAME_CONTROLLER_H
