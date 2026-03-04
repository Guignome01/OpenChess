#ifndef CORE_HISTORY_H
#define CORE_HISTORY_H

#include <cstdint>

#include "types.h"

// A single move record in the game history.
// Stores enough information to query the move log and support future undo.
struct MoveEntry {
  int fromRow, fromCol;
  int toRow, toCol;
  char piece;           // piece that moved (original, before any promotion)
  char captured;        // piece captured (' ' if none)
  char promotion;       // piece promoted to (' ' if not a promotion)
  bool isCapture;
  bool isEnPassant;
  int epCapturedRow;    // en passant captured pawn row (-1 if N/A)
  bool isCastling;
  bool isPromotion;
  bool isCheck;
  PositionState prevState;  // position state before the move (enables undo)
};

// In-memory game history — tracks moves and position hashes.
// Separates history tracking from board representation so each has a
// single, clear responsibility.
//
// Two concerns:
//   1. Move log: ordered list of all moves played in the game
//   2. Position tracking: Zobrist hash history for threefold repetition
class ChessHistory {
 public:
  ChessHistory();

  // Reset all history (new game, FEN load, etc.).
  void clear();

  // --- Move log ---

  // Append a move entry to the log.
  void addMove(const MoveEntry& entry);

  // Remove and return the last move (for undo). Returns false if empty.
  bool popMove(MoveEntry& entry);

  // Number of half-moves recorded.
  int moveCount() const { return moveCount_; }

  // True when no moves have been recorded.
  bool empty() const { return moveCount_ == 0; }

  // Access the move at the given index (0 = first move).
  // Caller must ensure 0 <= index < moveCount().
  const MoveEntry& getMove(int index) const;

  // Access the most recent move.
  // Caller must ensure !empty().
  const MoveEntry& lastMove() const;

  // --- Position hash tracking (threefold repetition) ---

  // Record a Zobrist hash for the current position.
  void recordPosition(uint64_t hash);

  // Clear position hash history (called on irreversible moves).
  void resetPositionHistory();

  // Has the current position occurred three or more times?
  bool isThreefoldRepetition() const;

  // Number of position hashes stored.
  int positionCount() const { return positionHistoryCount_; }

  // --- Constants ---

  static constexpr int MAX_MOVES = 300;
  static constexpr int MAX_POSITION_HISTORY = 128;

 private:
  MoveEntry moves_[MAX_MOVES];
  int moveCount_;

  uint64_t positionHistory_[MAX_POSITION_HISTORY];
  int positionHistoryCount_;
};

#endif  // CORE_HISTORY_H
