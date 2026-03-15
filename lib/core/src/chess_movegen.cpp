#include "chess_movegen.h"

namespace ChessMovegen {

using namespace ChessBitboard;

// ---------------------------------------------------------------------------
// Table storage
// ---------------------------------------------------------------------------

Bitboard KNIGHT_ATTACKS[64] = {};
Bitboard KING_ATTACKS[64] = {};
Bitboard PAWN_ATTACKS[2][64] = {};

// Diagonal masks (a1-h8 direction), one per square, inclusive of the square.
static Bitboard DIAG_MASK[64];

// Anti-diagonal masks (h1-a8 direction), one per square, inclusive of square.
static Bitboard ANTI_DIAG_MASK[64];

// First-rank attack table: for each file (0-7) and each 6-bit inner
// occupancy pattern (bits 1-6 of the rank), stores the 8-bit attack mask
// on that rank. Used for O(1) rank attack lookup.
static uint8_t FIRST_RANK_ATTACKS[8][64];

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

// ---------------------------------------------------------------------------
// Byte swap for Hyperbola Quintessence
// ---------------------------------------------------------------------------
// Reverses byte order of a 64-bit value, which mirrors rank order on the
// board. Used to compute negative-direction slider attacks by applying the
// positive-direction subtraction trick on the reversed occupancy.
// __builtin_bswap64 is available on GCC and Clang (including the Xtensa
// toolchain for ESP32).

static inline uint64_t byteSwap64(uint64_t v) {
  return __builtin_bswap64(v);
}

// ---------------------------------------------------------------------------
// Hyperbola Quintessence — O(1) line attacks (file/diagonal/anti-diagonal)
// ---------------------------------------------------------------------------
// Computes sliding piece attacks along a single line through square `sq`.
// The mask covers the full line including `sq`; internally excludes sq
// from the line occupancy via XOR.
//
// The o^(o-2r) subtraction trick:
//   - In the positive direction (toward higher-index squares), subtracting
//     2*sq_bit from the masked occupancy causes borrow propagation up to
//     the first blocker, yielding the attacked squares in that direction.
//   - Byte-swapping reverses rank order, applying the same trick in the
//     negative direction. The XOR combines both rays.
//
// Branchless and O(1) per line — replaces the classical ray-walking loop.
// Reference: https://www.chessprogramming.org/Hyperbola_Quintessence

static Bitboard lineHQ(Bitboard occ, Square sq, Bitboard mask) {
  Bitboard piece = squareBB(sq);
  Bitboard maskEx = mask ^ piece;   // mask excluding piece square
  Bitboard forward = occ & maskEx;
  Bitboard reverse = byteSwap64(forward);
  forward -= 2 * piece;
  reverse -= 2 * byteSwap64(piece);
  forward ^= byteSwap64(reverse);
  return forward & maskEx;
}

// ---------------------------------------------------------------------------
// Diagonal/anti-diagonal mask initialization
// ---------------------------------------------------------------------------
// Precomputes the full diagonal and anti-diagonal mask for each of the 64
// squares. A diagonal runs from lower-left to upper-right (constant
// rank-file difference). An anti-diagonal runs from lower-right to upper-left
// (constant rank+file sum). Both include the square itself.

static void initDiagMasks() {
  for (Square sq = 0; sq < 64; ++sq) {
    int rank = sq / 8, file = sq % 8;

    // Diagonal (rank - file = constant)
    Bitboard diag = 0;
    int startR = (rank >= file) ? rank - file : 0;
    int startF = (file >= rank) ? file - rank : 0;
    for (int r = startR, f = startF; r < 8 && f < 8; ++r, ++f)
      diag |= squareBB(r * 8 + f);
    DIAG_MASK[sq] = diag;

    // Anti-diagonal (rank + file = constant)
    Bitboard adiag = 0;
    int sum = rank + file;
    int startR2 = (sum <= 7) ? 0 : sum - 7;
    int startF2 = (sum <= 7) ? sum : 7;
    for (int r = startR2, f = startF2; r < 8 && f >= 0; ++r, --f)
      adiag |= squareBB(r * 8 + f);
    ANTI_DIAG_MASK[sq] = adiag;
  }
}

// ---------------------------------------------------------------------------
// First-rank attack table initialization
// ---------------------------------------------------------------------------
// For each file (0-7) and each 6-bit inner occupancy (the middle 6 files
// of a rank, representing 64 possible blocker patterns), computes the
// sliding attack mask along that rank.

static void initFirstRankAttacks() {
  for (int file = 0; file < 8; ++file) {
    for (int occ6 = 0; occ6 < 64; ++occ6) {
      uint8_t occ = occ6 << 1;  // inner occupancy at bits 1-6
      uint8_t result = 0;
      // Walk right (toward file h)
      for (int f = file + 1; f < 8; ++f) {
        result |= static_cast<uint8_t>(1 << f);
        if (occ & (1 << f)) break;
      }
      // Walk left (toward file a)
      for (int f = file - 1; f >= 0; --f) {
        result |= static_cast<uint8_t>(1 << f);
        if (occ & (1 << f)) break;
      }
      FIRST_RANK_ATTACKS[file][occ6] = result;
    }
  }
}

// ---------------------------------------------------------------------------
// Master initialization — must be called before any attack function.
// ---------------------------------------------------------------------------

void initAttacks() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  initKnightAttacks();
  initKingAttacks();
  initPawnAttacks();
  initDiagMasks();
  initFirstRankAttacks();
}

// ---------------------------------------------------------------------------
// Slider attack functions
// ---------------------------------------------------------------------------
// Rook: rank via first-rank lookup table, file via Hyperbola Quintessence.
// Bishop: both diagonals via Hyperbola Quintessence.
// All are O(1) per line — branchless except the rank table lookup which
// uses a single array index. Replaces the classical 4-direction ray walk.

Bitboard rookAttacks(Square sq, Bitboard occupied) {
  int rank = sq / 8;
  int file = sq % 8;

  // Rank attacks: extract 6 inner occupancy bits, table lookup, place on rank.
  uint8_t occ6 = static_cast<uint8_t>((occupied >> (rank * 8 + 1)) & 0x3F);
  Bitboard attacks = static_cast<Bitboard>(FIRST_RANK_ATTACKS[file][occ6])
                     << (rank * 8);

  // File attacks via Hyperbola Quintessence.
  attacks |= lineHQ(occupied, sq, FILE_A << file);

  return attacks;
}

Bitboard bishopAttacks(Square sq, Bitboard occupied) {
  return lineHQ(occupied, sq, DIAG_MASK[sq])
       | lineHQ(occupied, sq, ANTI_DIAG_MASK[sq]);
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
//
// Not yet called by production code — pre-built infrastructure for upcoming
// evaluation terms (king safety, mobility) and search (move ordering).

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
