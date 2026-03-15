#include "chess_movegen.h"

#include "chess_pieces.h"

namespace ChessMovegen {

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
  ChessPieces::initPawnMasks();
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

// ---------------------------------------------------------------------------
// Ray geometry functions
// ---------------------------------------------------------------------------
// rayBetween and lineBB exploit a slider-intersection trick:
//   - Slider attacks from s1 with s2 as the only blocker gives the ray from
//     s1 toward s2, stopping AT s2 (inclusive).
//   - Intersecting the ray from s1→s2 with the ray from s2→s1 yields the
//     squares strictly between them (rayBetween, exclusive of endpoints).
//   - Slider attacks with empty occupancy give the full unobstructed ray to
//     the board edge. Intersecting those from both squares gives all squares
//     on their shared line, minus the endpoints (lineBB adds them back).
// Both check colinearity first: s1 and s2 must share a rank, file, or
// diagonal (|dr| == |dc| for diagonals). Non-colinear pairs return 0.

Bitboard rayBetween(Square s1, Square s2) {
  int r1 = static_cast<int>(s1) / 8, c1 = static_cast<int>(s1) % 8;
  int r2 = static_cast<int>(s2) / 8, c2 = static_cast<int>(s2) % 8;
  int dr = r2 - r1, dc = c2 - c1;

  // Colinearity check: orthogonal if dr==0 or dc==0, diagonal if |dr|==|dc|.
  if (dr != 0 && dc != 0 && (dr < 0 ? -dr : dr) != (dc < 0 ? -dc : dc))
    return 0;

  // Use each square as the sole blocker for the other's ray.
  // The intersection is the squares strictly between them.
  Bitboard b1 = squareBB(s1), b2 = squareBB(s2);
  if (dr == 0 || dc == 0)
    return rookAttacks(s1, b2) & rookAttacks(s2, b1);
  return bishopAttacks(s1, b2) & bishopAttacks(s2, b1);
}

Bitboard lineBB(Square s1, Square s2) {
  int r1 = static_cast<int>(s1) / 8, c1 = static_cast<int>(s1) % 8;
  int r2 = static_cast<int>(s2) / 8, c2 = static_cast<int>(s2) % 8;
  int dr = r2 - r1, dc = c2 - c1;

  // Colinearity check (same as rayBetween).
  if (dr != 0 && dc != 0 && (dr < 0 ? -dr : dr) != (dc < 0 ? -dc : dc))
    return 0;

  // Empty occupancy (0) gives full unobstructed rays to the board edge.
  // Intersecting from both squares yields the shared line minus endpoints.
  Bitboard endpoints = squareBB(s1) | squareBB(s2);
  if (dr == 0 || dc == 0)
    return (rookAttacks(s1, 0) & rookAttacks(s2, 0)) | endpoints;
  return (bishopAttacks(s1, 0) & bishopAttacks(s2, 0)) | endpoints;
}

// ---------------------------------------------------------------------------
// Attacked-by bitboard computation
// ---------------------------------------------------------------------------
// Builds per-piece-type attack maps for both colors in a single pass.
// Pawns use bulk shift (entire bitboard shifted diagonally) — no per-square
// loop. Leapers (knight, king) use precomputed table lookup per square.
// Sliders (bishop, rook, queen) use classical ray loops per square.

AttackInfo computeAttackInfo(const BitboardSet& bb) {
  using namespace ChessPiece;

  AttackInfo info = {};

  for (int c = 0; c < 2; ++c) {
    Color color = static_cast<Color>(c);

    // --- Pawns (bulk shift, no per-square loop) ---
    Bitboard pawns = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::PAWN))];
    if (c == 0) {
      // White pawns attack NE and NW.
      info.byPiece[c][raw(PieceType::PAWN)] = shiftNE(pawns) | shiftNW(pawns);
    } else {
      // Black pawns attack SE and SW.
      info.byPiece[c][raw(PieceType::PAWN)] = shiftSE(pawns) | shiftSW(pawns);
    }

    // --- Knights (table lookup per square) ---
    Bitboard knights = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::KNIGHT))];
    Bitboard knightAtk = 0;
    Bitboard tmp = knights;
    while (tmp) { knightAtk |= KNIGHT_ATTACKS[popLsb(tmp)]; }
    info.byPiece[c][raw(PieceType::KNIGHT)] = knightAtk;

    // --- Bishops (classical rays per square) ---
    Bitboard bishops = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::BISHOP))];
    Bitboard bishopAtk = 0;
    tmp = bishops;
    while (tmp) { bishopAtk |= bishopAttacks(popLsb(tmp), bb.occupied); }
    info.byPiece[c][raw(PieceType::BISHOP)] = bishopAtk;

    // --- Rooks (classical rays per square) ---
    Bitboard rooks = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::ROOK))];
    Bitboard rookAtk = 0;
    tmp = rooks;
    while (tmp) { rookAtk |= rookAttacks(popLsb(tmp), bb.occupied); }
    info.byPiece[c][raw(PieceType::ROOK)] = rookAtk;

    // --- Queens (combined slider rays per square) ---
    Bitboard queens = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::QUEEN))];
    Bitboard queenAtk = 0;
    tmp = queens;
    while (tmp) { queenAtk |= queenAttacks(popLsb(tmp), bb.occupied); }
    info.byPiece[c][raw(PieceType::QUEEN)] = queenAtk;

    // --- King (single table lookup) ---
    Bitboard king = bb.byPiece[pieceZobristIndex(makePiece(color, PieceType::KING))];
    info.byPiece[c][raw(PieceType::KING)] = king ? KING_ATTACKS[lsb(king)] : 0;

    // --- Color union ---
    info.byColor[c] = info.byPiece[c][raw(PieceType::PAWN)]
                     | info.byPiece[c][raw(PieceType::KNIGHT)]
                     | info.byPiece[c][raw(PieceType::BISHOP)]
                     | info.byPiece[c][raw(PieceType::ROOK)]
                     | info.byPiece[c][raw(PieceType::QUEEN)]
                     | info.byPiece[c][raw(PieceType::KING)];
  }

  info.allAttacks = info.byColor[0] | info.byColor[1];
  return info;
}

}  // namespace ChessMovegen
