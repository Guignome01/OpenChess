#ifndef CORE_CHESS_GAME_H
#define CORE_CHESS_GAME_H

#include <string>

#include "chess_board.h"
#include "chess_iterator.h"
#include "chess_notation.h"
#include "game_observer.h"
#include "chess_history.h"
#include "types.h"

// Central chess game orchestrator — the primary API for all game-level operations.
// Composes ChessBoard (board representation, position queries, threefold detection)
// and ChessHistory (move log + persistent recording), with optional IGameObserver
// for UI notification.
//
// All chess-state mutations (makeMove, loadFEN, endGame) flow through this class.
// It handles move history recording, observer notification, and batching.
// All end-condition detection (including threefold repetition) is handled
// uniformly via ChessRules::isGameOver().
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
  // automatically if recording), auto-saves on game-end, and notifies observer.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Parse a move in coordinate notation (e.g. "e2e4", "e7e8q") and execute it.
  // This is the notation used by the UCI protocol for move communication.
  // Returns invalid MoveResult if the string cannot be parsed.
  MoveResult makeMove(const std::string& move);

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

  // Current move cursor position (0 = before any move, 1 = after first move, etc.)
  // Note: offset by +1 from ChessHistory::currentMoveIndex() for a 0-based "start position" semantic.
  int currentMoveIndex() const { return history_.currentMoveIndex() + 1; }

  // Total number of moves in the history log.
  int moveCount() const { return history_.moveCount(); }

  // Return all moves in the history formatted as strings.
  // Populates `out` with up to `maxMoves` entries, returns actual count written.
  // Includes all moves in the log (not just up to cursor).
  //   COORDINATE (default): "e2e4", "e7e8q" — fast, iterates MoveEntry array.
  //   SAN: "e4", "Nxf3+", "O-O", "e8=Q#" — replays through a temp board.
  //   LAN: "e2-e4", "Ng1xf3+", "O-O" — replays through a temp board.
  int getHistory(std::string out[], int maxMoves,
                 MoveFormat format = MoveFormat::COORDINATE) const;

  // --- Notation helpers ---

  // Convert array coordinates to coordinate notation (e.g. "e2e4", "e7e8q").
  static std::string toCoordinate(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Parse coordinate notation into array coordinates.
  // Returns true if valid; fills promotion with the promotion char (or ' ').
  static bool parseCoordinate(const std::string& move, int& fromRow, int& fromCol,
                              int& toRow, int& toCol, char& promotion);

  // Parse a move in any supported notation format (coordinate, LAN, SAN).
  // Requires board context for SAN disambiguation.
  bool parseMove(const std::string& move, int& fromRow, int& fromCol,
                 int& toRow, int& toCol, char& promotion) const;

  // --- Replay ---

  // Restore a saved game from storage into the board.
  bool resumeGame();

  // --- Resume queries ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Game state (owned by ChessGame) ---

  bool isGameOver() const { return gameOver_; }
  GameResult gameResult() const { return gameResult_; }
  char winnerColor() const { return winnerColor_; }

  // --- Board pass-throughs ---

  const Piece (&getBoard() const)[8][8] { return board_.getBoard(); }
  Piece getSquare(int row, int col) const { return board_.getSquare(row, col); }
  Color currentTurn() const { return board_.currentTurn(); }
  uint8_t getCastlingRights() const { return board_.getCastlingRights(); }
  const PositionState& positionState() const { return board_.positionState(); }
  std::string getFen() const { return board_.getFen(); }
  float getEvaluation() const { return board_.getEvaluation(); }

  // --- Convenience wrappers (chess queries, delegated to board) ---

  void getPossibleMoves(int row, int col, int& moveCount, int moves[][2]) const {
    board_.getPossibleMoves(row, col, moveCount, moves);
  }

  bool isCheck(Color kingColor) const { return board_.isCheck(kingColor); }
  bool inCheck() const { return board_.inCheck(); }
  bool isCheckmate() const { return board_.isCheckmate(); }
  bool isStalemate() const { return board_.isStalemate(); }
  bool isInsufficientMaterial() const { return board_.isInsufficientMaterial(); }
  bool isFiftyMoves() const { return board_.isFiftyMoves(); }
  bool isDraw() const { return board_.isDraw(); }
  bool isThreefoldRepetition() const { return board_.isThreefoldRepetition(); }

  bool isAttacked(int row, int col, Color byColor) const {
    return board_.isAttacked(row, col, byColor);
  }

  int findPiece(Piece target, int positions[][2], int maxPositions) const {
    return board_.findPiece(target, positions, maxPositions);
  }

  std::string boardToText() const { return board_.boardToText(); }

  // --- Board iteration wrappers (delegated to ChessIterator on the game board) ---

  template <typename Fn>
  void forEachSquare(Fn&& fn) const {
    ChessIterator::forEachSquare(board_.getBoard(), static_cast<Fn&&>(fn));
  }

  template <typename Fn>
  void forEachPiece(Fn&& fn) const {
    ChessIterator::forEachPiece(board_.getBoard(), static_cast<Fn&&>(fn));
  }

  template <typename Fn>
  bool somePiece(Fn&& fn) const {
    return ChessIterator::somePiece(board_.getBoard(), static_cast<Fn&&>(fn));
  }

  int moveNumber() const { return board_.moveNumber(); }

  ChessUtils::EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol) const {
    return board_.checkEnPassant(fromRow, fromCol, toRow, toCol);
  }

  ChessUtils::CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol) const {
    return board_.checkCastling(fromRow, fromCol, toRow, toCol);
  }

  // --- History ---

  // Access the in-game move/position history.
  const ChessHistory& history() const { return history_; }

  // --- Batching (suppress notifications during replay) ---

  void beginBatch();
  void endBatch();  // observer notification on exit if changes occurred

  // --- Direct board access (read-only, for tests and edge cases) ---

  const ChessBoard& board() const { return board_; }

 private:
  ChessBoard board_;
  ChessHistory history_;
  IGameObserver* observer_;
  Log logger_;
  int batchDepth_;
  bool batchDirty_;
  std::string startFen_;  // initial FEN for SAN/LAN replay in getHistory()
  bool gameOver_;
  GameResult gameResult_;
  char winnerColor_;

  void notifyObserver();
};

#endif  // CORE_CHESS_GAME_H
