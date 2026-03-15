#ifndef CORE_CHESS_MOVEGEN_H
#define CORE_CHESS_MOVEGEN_H

// Precomputed attack tables and slider attack functions.
//
// Leaper attacks (knight, king, pawn) are stored in lookup tables —
// one Bitboard per square, computed once at startup via initAttacks().
//
// Slider attacks (rook, bishop, queen) use two O(1) techniques:
//   - Rank attacks: first-rank lookup table (512 bytes, indexed by
//     file + 6-bit inner occupancy).
//   - File/diagonal/anti-diagonal attacks: Hyperbola Quintessence —
//     branchless o^(o-2r) subtraction trick with byte-swap for the
//     negative ray direction. Diagonal masks (DIAG_MASK[64],
//     ANTI_DIAG_MASK[64]) are precomputed at startup.
//
// References:
//   https://www.chessprogramming.org/Bitboards
//   https://www.chessprogramming.org/Hyperbola_Quintessence

#include "chess_bitboard.h"

namespace ChessMovegen {

using ChessBitboard::Bitboard;
using ChessBitboard::Square;

// ---------------------------------------------------------------------------
// Precomputed leaper tables (initialized by initAttacks())
// ---------------------------------------------------------------------------

extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];  // [0] = WHITE, [1] = BLACK

// Must be called once before using any attack or pawn-structure function.
// Initializes leaper tables and calls ChessPieces::initPawnMasks().
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

// ---------------------------------------------------------------------------
// X-ray attack functions (for pin detection)
// ---------------------------------------------------------------------------

// Rook attacks from `sq` shooting through exactly one layer of friendly pieces.
// Returns the extended attack set after removing friendly blockers from occupancy.
// ANDing with enemy rook/queen bitboards gives potential pinners.
Bitboard xrayRookAttacks(Bitboard occupied, Bitboard friendly, Square sq);

// Bishop attacks from `sq` shooting through exactly one layer of friendly pieces.
Bitboard xrayBishopAttacks(Bitboard occupied, Bitboard friendly, Square sq);

// ---------------------------------------------------------------------------
// Attacked-by bitboards (on-demand evaluation helper)
// ---------------------------------------------------------------------------
// Computes per-piece-type and per-color attack maps from scratch.
// Called once per evaluation, not per move — avoids complicating
// makeMove/reverseMove with incremental update logic.
//
// Not yet called by production code. Planned consumers:
//   - Evaluation: king safety (counting attackers near king), mobility
//     (squares controlled per piece).
//   - Search: move ordering (captures of undefended pieces, MVV-LVA).
//
// PieceType::NONE (index 0) slots are unused; indices 1–6 for PAWN..KING.

struct AttackInfo {
  Bitboard byPiece[2][7];  // [color][pieceType] — squares attacked by that piece type
  Bitboard byColor[2];     // [color] — union of all piece attacks for that color
  Bitboard allAttacks;     // union of both colors
};

// Build full attack maps for both colors from the given position.
// Requires initAttacks() to have been called.
AttackInfo computeAttackInfo(const ChessBitboard::BitboardSet& bb);

// ---------------------------------------------------------------------------
// Ray geometry functions
// ---------------------------------------------------------------------------

// Squares strictly between s1 and s2 on the same rank, file, or diagonal
// (exclusive of both endpoints). Returns 0 if s1 and s2 are not colinear.
Bitboard rayBetween(Square s1, Square s2);

// Full line through s1 and s2, extending to both board edges, inclusive of
// both endpoints. Returns 0 if s1 and s2 are not colinear (different rank,
// file, and diagonal).
// Not yet called by production code. Planned consumers:
//   - SEE (Static Exchange Evaluation): finds all x-ray attackers behind a
//     capture target along the full line.
//   - Advanced alignment/pin detection in evaluation.
Bitboard lineBB(Square s1, Square s2);

}  // namespace ChessMovegen

#endif  // CORE_CHESS_MOVEGEN_H
