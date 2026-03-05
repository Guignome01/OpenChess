#ifndef CORE_CHESS_GAME_H
#define CORE_CHESS_GAME_H

#include <functional>
#include <string>

#include "chess_board.h"
#include "game_observer.h"
#include "chess_history.h"
#include "types.h"

// Central chess game orchestrator — the primary API for all game-level operations.
// Composes ChessBoard (board representation, position queries, threefold detection)
// and ChessHistory (move log + persistent recording), with optional IGameObserver
// for UI notification.
//
// All chess-state mutations (makeMove, loadFEN, endGame) flow through this class.
// It handles move history recording, callback dispatch, and batching.
// All end-condition detection (including threefold repetition) is handled
// uniformly inside ChessBoard::detectGameEnd().
//
// Recording is automatic: if IGameStorage is provided, ChessGame calls
// history_.setHeader() / history_.save() at game lifecycle boundaries,
// and history_.addMove() transparently persists each move.
//
// Usage:
//   ChessGame game;
//   game.newGame();
//   MoveResult r = game.makeMove(6, 4, 4, 4);  // e2e4
//   if (game.isGameOver()) { ... }
//
// With recording and observer:
//   ChessGame game(&storage, &observer, &logger);
//   game.startNewGame(GameModeId::BOT, 'w', 5);
//   game.makeMove(6, 4, 4, 4);  // atomically updates board, records, notifies
//
class ChessGame {
 public:
  using StateCallback = std::function<void()>;

  ChessGame(IGameStorage* storage = nullptr,
            IGameObserver* observer = nullptr,
            ILogger* logger = nullptr);

  // --- Lifecycle ---

  void newGame();

  // Start a new game and begin recording.  Resets the board, sets up recording
  // header, and snapshots the initial position.
  void startNewGame(GameModeId mode, uint8_t playerColor = '?', uint8_t botDepth = 0);

  // End the game — saves recording (if active) and sets board game-over state.
  void endGame(GameResult result, char winnerColor);

  // Discard current recording without finalizing.
  void discardRecording();

  // --- Mutations ---

  // Validate and execute a move.  Atomically: applies to board (which detects all
  // end conditions including threefold repetition), records in history (persists
  // automatically if recording), auto-saves on game-end, fires callback, and
  // notifies observer.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Parse a UCI move string (e.g. "e2e4", "e7e8q") and execute it.
  // Returns invalid MoveResult if the UCI string cannot be parsed.
  MoveResult makeMove(const std::string& uci);

  // Load arbitrary position from FEN.  Snapshots FEN if recording, notifies observer.
  bool loadFEN(const std::string& fen);

  // --- Undo / Redo ---

  // Undo the last move.  Returns true if successful, false if nothing to undo.
  bool undoMove();

  // Redo a previously undone move.  Returns true if successful, false if nothing to redo.
  bool redoMove();

  // Can undo / redo?
  bool canUndo() const { return history_.canUndo(); }
  bool canRedo() const { return history_.canRedo(); }

  // --- UCI helpers ---

  // Convert array coordinates to a UCI move string (e.g. "e2e4", "e7e8q").
  static std::string toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Parse a UCI move string into array coordinates.
  // Returns true if valid; fills promotion with the promotion char (or ' ').
  static bool parseUCIMove(const std::string& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

  // --- Replay ---

  // Restore a saved game from storage into the board.
  bool resumeGame();

  // --- Resume queries ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Game state (delegated to board) ---

  bool isGameOver() const { return board_.isGameOver(); }
  GameResult gameResult() const { return board_.gameResult(); }
  char winnerColor() const { return board_.winnerColor(); }

  // --- Board pass-throughs ---

  const char (&getBoard() const)[8][8] { return board_.getBoard(); }
  char getSquare(int row, int col) const { return board_.getSquare(row, col); }
  char currentTurn() const { return board_.currentTurn(); }
  uint8_t getCastlingRights() const { return board_.getCastlingRights(); }
  const PositionState& positionState() const { return board_.positionState(); }
  std::string getFen() const { return board_.getFen(); }
  float getEvaluation() const { return board_.getEvaluation(); }

  // --- Convenience wrappers (chess queries, delegated to board) ---

  void getPossibleMoves(int row, int col, int& moveCount, int moves[][2]) const {
    board_.getPossibleMoves(row, col, moveCount, moves);
  }

  bool isKingInCheck(char kingColor) const { return board_.isKingInCheck(kingColor); }
  bool inCheck() const { return board_.inCheck(); }
  bool isCheckmate() const { return board_.isCheckmate(); }
  bool isStalemate() const { return board_.isStalemate(); }
  bool isInsufficientMaterial() const { return board_.isInsufficientMaterial(); }

  bool isAttacked(int row, int col, char byColor) const {
    return board_.isAttacked(row, col, byColor);
  }

  int findPiece(char type, char color, int positions[][2], int maxPositions) const {
    return board_.findPiece(type, color, positions, maxPositions);
  }

  bool findKingPosition(char kingColor, int& kingRow, int& kingCol) const {
    return board_.findKingPosition(kingColor, kingRow, kingCol);
  }

  int moveNumber() const { return board_.moveNumber(); }

  ChessUtils::EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol) const {
    return board_.checkEnPassant(fromRow, fromCol, toRow, toCol);
  }

  ChessUtils::CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol) const {
    return board_.checkCastling(fromRow, fromCol, toRow, toCol);
  }

  // --- Draw queries (delegated to board) ---

  // Is the position a draw (50-move, threefold repetition, or insufficient material)?
  bool isDraw() const;

  // Has the current position occurred three or more times?
  bool isThreefoldRepetition() const;

  // --- History ---

  // Access the in-game move/position history.
  const ChessHistory& history() const { return history_; }

  // --- Batching (suppress callbacks during replay) ---

  void beginBatch();
  void endBatch();  // fires one callback on exit if changes occurred

  // --- Callback ---

  void onStateChanged(StateCallback cb) { stateCallback_ = cb; }

  // --- Direct board access (for tests and edge cases) ---

  ChessBoard& board() { return board_; }
  const ChessBoard& board() const { return board_; }

 private:
  ChessBoard board_;
  ChessHistory history_;
  IGameObserver* observer_;
  StateCallback stateCallback_;
  int batchDepth_;
  bool batchDirty_;

  void fireCallback();
  void notifyObserver();
};

#endif  // CORE_CHESS_GAME_H
