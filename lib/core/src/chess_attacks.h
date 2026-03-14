#ifndef CORE_CHESS_ATTACKS_H
#define CORE_CHESS_ATTACKS_H

// Precomputed attack tables and slider attack functions.
//
// Leaper attacks (knight, king, pawn) are stored in lookup tables —
// one Bitboard per square, computed once at startup via initAttacks().
//
// Slider attacks (rook, bishop, queen) use classical ray loops:
// walk one square at a time in each direction, stopping at the first
// occupied square. Zero memory overhead, readable, and sufficient for
// board UI + future puzzle solver. Can be upgraded to magic bitboards
// later if search profiling demands it.
//
// Reference: https://www.chessprogramming.org/Bitboards

#include "chess_bitboard.h"

namespace ChessAttacks {

using ChessBitboard::Bitboard;
using ChessBitboard::Square;

// ---------------------------------------------------------------------------
// Precomputed leaper tables (initialized by initAttacks())
// ---------------------------------------------------------------------------

extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];  // [0] = WHITE, [1] = BLACK

// Must be called once before using any attack function.
// Safe to call multiple times (idempotent).
void initAttacks();

// ---------------------------------------------------------------------------
// Slider attack functions (classical ray loops)
// ---------------------------------------------------------------------------

// Rook attacks from `sq` given `occupied` — walks N, S, E, W rays.
Bitboard rookAttacks(Square sq, Bitboard occupied);

// Bishop attacks from `sq` given `occupied` — walks NE, NW, SE, SW rays.
Bitboard bishopAttacks(Square sq, Bitboard occupied);

// Queen attacks = rook + bishop combined.
inline Bitboard queenAttacks(Square sq, Bitboard occupied) {
  return rookAttacks(sq, occupied) | bishopAttacks(sq, occupied);
}

}  // namespace ChessAttacks

#endif  // CORE_CHESS_ATTACKS_H
