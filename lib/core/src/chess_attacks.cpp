#include "chess_attacks.h"

namespace ChessAttacks {

using namespace ChessBitboard;

// ---------------------------------------------------------------------------
// Table storage
// ---------------------------------------------------------------------------

Bitboard KNIGHT_ATTACKS[64] = {};
Bitboard KING_ATTACKS[64] = {};
Bitboard PAWN_ATTACKS[2][64] = {};

// ---------------------------------------------------------------------------
// Table initialization
// ---------------------------------------------------------------------------

// Knight offsets as (file_delta, rank_delta) pairs.
// A knight moves in an L-shape: 2 squares in one axis, 1 in the other.
static void initKnightAttacks() {
  for (Square sq = 0; sq < 64; ++sq) {
    Bitboard bb = squareBB(sq);
    Bitboard attacks = 0;

    // Two north, one east/west
    attacks |= (bb & ~FILE_H) << 17;            // NNE
    attacks |= (bb & ~FILE_A) << 15;            // NNW
    // Two south, one east/west
    attacks |= (bb & ~FILE_H) >> 15;            // SSE
    attacks |= (bb & ~FILE_A) >> 17;            // SSW
    // Two east, one north/south
    attacks |= (bb & ~FILE_G & ~FILE_H) << 10;  // ENE
    attacks |= (bb & ~FILE_G & ~FILE_H) >> 6;   // ESE
    // Two west, one north/south
    attacks |= (bb & ~FILE_A & ~FILE_B) << 6;   // WNW
    attacks |= (bb & ~FILE_A & ~FILE_B) >> 10;  // WSW

    KNIGHT_ATTACKS[sq] = attacks;
  }
}

// King can step one square in any of the 8 compass directions.
static void initKingAttacks() {
  for (Square sq = 0; sq < 64; ++sq) {
    Bitboard bb = squareBB(sq);
    Bitboard attacks = shiftNorth(bb) | shiftSouth(bb)
                     | shiftEast(bb)  | shiftWest(bb)
                     | shiftNE(bb)    | shiftNW(bb)
                     | shiftSE(bb)    | shiftSW(bb);
    KING_ATTACKS[sq] = attacks;
  }
}

// Pawn attacks are the diagonal capture squares (not pushes).
// White pawns attack NE and NW; black pawns attack SE and SW.
static void initPawnAttacks() {
  for (Square sq = 0; sq < 64; ++sq) {
    Bitboard bb = squareBB(sq);
    PAWN_ATTACKS[0][sq] = shiftNE(bb) | shiftNW(bb);  // WHITE
    PAWN_ATTACKS[1][sq] = shiftSE(bb) | shiftSW(bb);  // BLACK
  }
}

void initAttacks() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  initKnightAttacks();
  initKingAttacks();
  initPawnAttacks();
}

// ---------------------------------------------------------------------------
// Classical ray loops for slider attacks
// ---------------------------------------------------------------------------
// Each function walks one square at a time in each of its 4 directions.
// The attacked squares include the first blocker (can be captured) but
// nothing beyond it.

// Walk a single ray direction from `sq`. Returns the attacked squares.
// `step` is the square-index delta per step (+8 for north, -1 for west, etc).
// `fileMask` prevents wrapping: the bit is ANDed before each step.
// For vertical rays (N/S) fileMask is ~0 (no wrapping possible).
static Bitboard rayAttacks(Square sq, int step, Bitboard fileMask, Bitboard occupied) {
  Bitboard attacks = 0;
  Bitboard current = squareBB(sq);

  while (true) {
    // Apply wrapping guard before stepping
    current &= fileMask;
    if (!current) break;

    // Step in the direction
    if (step > 0)
      current <<= step;
    else
      current >>= (-step);

    if (!current) break;  // shifted off the board

    attacks |= current;

    // Stop if we hit an occupied square (the blocker is included)
    if (current & occupied) break;
  }

  return attacks;
}

Bitboard rookAttacks(Square sq, Bitboard occupied) {
  Bitboard attacks = 0;
  attacks |= rayAttacks(sq,  8, ~0ULL,      occupied);  // North
  attacks |= rayAttacks(sq, -8, ~0ULL,      occupied);  // South
  attacks |= rayAttacks(sq,  1, NOT_FILE_H, occupied);  // East
  attacks |= rayAttacks(sq, -1, NOT_FILE_A, occupied);  // West
  return attacks;
}

Bitboard bishopAttacks(Square sq, Bitboard occupied) {
  Bitboard attacks = 0;
  attacks |= rayAttacks(sq,  9, NOT_FILE_H, occupied);  // North-East
  attacks |= rayAttacks(sq,  7, NOT_FILE_A, occupied);  // North-West
  attacks |= rayAttacks(sq, -7, NOT_FILE_H, occupied);  // South-East
  attacks |= rayAttacks(sq, -9, NOT_FILE_A, occupied);  // South-West
  return attacks;
}

// ---------------------------------------------------------------------------
// X-ray attack functions (for pin detection)
// ---------------------------------------------------------------------------

Bitboard xrayRookAttacks(Bitboard occupied, Bitboard friendly, Square sq) {
  Bitboard attacks = rookAttacks(sq, occupied);
  Bitboard blockers = attacks & friendly;
  return rookAttacks(sq, occupied ^ blockers);
}

Bitboard xrayBishopAttacks(Bitboard occupied, Bitboard friendly, Square sq) {
  Bitboard attacks = bishopAttacks(sq, occupied);
  Bitboard blockers = attacks & friendly;
  return bishopAttacks(sq, occupied ^ blockers);
}

Bitboard rayBetween(Square s1, Square s2) {
  int r1 = static_cast<int>(s1) / 8, c1 = static_cast<int>(s1) % 8;
  int r2 = static_cast<int>(s2) / 8, c2 = static_cast<int>(s2) % 8;
  int dr = r2 - r1, dc = c2 - c1;

  // Must be colinear: same rank, file, or diagonal.
  if (dr != 0 && dc != 0 && (dr < 0 ? -dr : dr) != (dc < 0 ? -dc : dc))
    return 0;

  Bitboard b1 = squareBB(s1), b2 = squareBB(s2);
  if (dr == 0 || dc == 0)
    return rookAttacks(s1, b2) & rookAttacks(s2, b1);
  return bishopAttacks(s1, b2) & bishopAttacks(s2, b1);
}

}  // namespace ChessAttacks
