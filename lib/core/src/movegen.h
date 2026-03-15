#ifndef LIBRECHESS_MOVEGEN_H
#define LIBRECHESS_MOVEGEN_H

// ---------------------------------------------------------------------------
// Legal move generation — pin+check mask filtered.
//
// All functions are stateless: board representation (BitboardSet + mailbox)
// and position state are passed in as parameters.
//
// Two generation modes:
//   • Per-piece: getPossibleMoves() — legal moves for one piece (LED hints).
//   • Bulk: generateAllMoves() / generateCaptures() — full position
//     enumeration for search and game-end detection.
//
// Also provides single-move validation (isValidMove) and the EP legality
// query used by Zobrist hashing (hasLegalEnPassantCapture).
// ---------------------------------------------------------------------------

#include "attacks.h"
#include "bitboard.h"
#include "move.h"
#include "types.h"

namespace LibreChess {
namespace movegen {

// ---------------------------------------------------------------------------
// Per-piece legal move generation
// ---------------------------------------------------------------------------

// Returns only legal moves for the piece at (row, col).
// Uses pin+check mask filtering with copy-make fallback for king and EP moves.
void getPossibleMoves(const BitboardSet& bb, const Piece mailbox[],
                      int row, int col, const PositionState& state,
                      MoveList& moves);

// ---------------------------------------------------------------------------
// Bulk legal move generation
// ---------------------------------------------------------------------------

// All legal moves for `color`.  Computes pin+check masks once, then iterates
// all friendly pieces via bitboard serialization.
void generateAllMoves(const BitboardSet& bb, const Piece mailbox[],
                      Color color, const PositionState& state,
                      MoveList& moves);

// Captures and capture-promotions only (for quiescence search).
void generateCaptures(const BitboardSet& bb, const Piece mailbox[],
                      Color color, const PositionState& state,
                      MoveList& moves);

// ---------------------------------------------------------------------------
// Single-move validation
// ---------------------------------------------------------------------------

// Validate that moving (fromRow,fromCol)→(toRow,toCol) is legal.
// Finds king square internally.
bool isValidMove(const BitboardSet& bb, const Piece mailbox[],
                 int fromRow, int fromCol, int toRow, int toCol,
                 const PositionState& state);

// Validate with pre-found king square (avoids redundant king search).
bool isValidMove(const BitboardSet& bb, const Piece mailbox[],
                 Square from, Square to,
                 const PositionState& state, Square kingSq);

// ---------------------------------------------------------------------------
// Utility queries
// ---------------------------------------------------------------------------

// Does `color` have at least one legal move?
// Used by rules::isCheckmate / rules::isStalemate.
bool hasAnyLegalMove(const BitboardSet& bb, const Piece mailbox[],
                     Color color, const PositionState& state);

// Does `sideToMove` have a legal EP capture?
// Used by Zobrist hashing — only hash the EP file when the capture is legal.
bool hasLegalEnPassantCapture(const BitboardSet& bb, const Piece mailbox[],
                              Color sideToMove, const PositionState& state);

}  // namespace movegen
}  // namespace LibreChess

#endif  // LIBRECHESS_MOVEGEN_H
