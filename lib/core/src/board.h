#ifndef CORE_BOARD_H
#define CORE_BOARD_H

#include <cstring>
#include <functional>
#include <string>

#include "types.h"
#include "utils.h"

// Complete chess game state manager — the "chess.js" of this project.
// Owns the board, turn, rules, and game-over state.
// Pure logic, no hardware dependencies, natively compilable for unit tests.
//
// Usage:
//   ChessBoard cb;
//   cb.newGame();                       // standard starting position
//   MoveResult r = cb.makeMove(6,4, 4,4);  // e2e4
//   if (!r.valid) { /* illegal */ }
//   cb.loadFEN("...");                  // load arbitrary position
//   cb.endGame(GameResult::RESIGNATION, 'b'); // manual termination
//
class ChessBoard {
 public:
  // State-change callback — fired after every mutating operation
  // (makeMove, loadFEN, newGame, endGame) unless batching is active.
  using StateCallback = std::function<void()>;

  ChessBoard();

  // --- Lifecycle ---

  // Reset to standard initial position.
  void newGame();

  // Load arbitrary position from FEN.  Resets game-over state.
  void loadFEN(const std::string& fen);

  // Manually end the game (resignation, timeout, abort, etc.).
  void endGame(GameResult result, char winnerColor);

  // --- Moves ---

  // Validate and execute a move.  Returns full MoveResult.
  // Promotion char: 'q','r','b','n' or ' ' for auto-queen.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // --- Queries ---

  const char (&getBoard() const)[8][8] { return board_; }
  char getSquare(int row, int col) const { return board_[row][col]; }
  char currentTurn() const { return currentTurn_; }
  bool isGameOver() const { return gameOver_; }
  GameResult gameResult() const { return gameResult_; }
  char winnerColor() const { return winnerColor_; }

  // Position state accessors
  uint8_t getCastlingRights() const { return state_.castlingRights; }
  const PositionState& positionState() const { return state_; }

  // Current position as FEN string.
  std::string getFen() const;

  // Material evaluation (positive = white advantage).
  float getEvaluation() const;

  // --- Batching (suppress callbacks during replay) ---

  void beginBatch();
  void endBatch();  // fires one callback on exit if changes occurred

  // --- Callback ---

  void onStateChanged(StateCallback cb) { stateCallback_ = cb; }

  // --- Constants ---

  static const char INITIAL_BOARD[8][8];

 private:
  char board_[8][8];
  char currentTurn_;
  bool gameOver_;
  GameResult gameResult_;
  char winnerColor_;
  PositionState state_;  // castling rights, en passant, clocks

  // FEN / evaluation cache (mutable: updated lazily from const getters)
  mutable std::string cachedFen_;
  mutable float cachedEval_;
  mutable bool fenDirty_;
  mutable bool evalDirty_;
  void invalidateCache();

  // Batching
  int batchDepth_;
  bool batchDirty_;

  StateCallback stateCallback_;

  // --- Position history (Zobrist hash-based threefold repetition) ---
  static constexpr int MAX_POSITION_HISTORY = 128;
  uint64_t positionHistory_[MAX_POSITION_HISTORY];
  int positionHistoryCount_;

  static inline int pieceToZobristIndex(char piece) {
    const char* pieces = "PNBRQKpnbrqk";
    const char* p = __builtin_strchr(pieces, piece);
    return p ? static_cast<int>(p - pieces) : -1;
  }

  uint64_t computeZobristHash() const;
  void recordPosition();
  bool isThreefoldRepetition() const;

  // Pure chess logic extracted from ChessGame::applyMove / updateGameStatus
  void applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result);
  void updateCastlingRightsAfterMove(int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece);
  void applyCastling(int fromRow, int fromCol, int toRow, int toCol, char kingPiece);
  void advanceTurn();
  GameResult detectGameEnd(char& winner);

  void fireCallback();
};

#endif  // CORE_BOARD_H
