#include "evaluation.h"

namespace {

using namespace LibreChess;

// Material values indexed by piece type offset (P=0 N=1 B=2 R=3 Q=4 K=5).
// In centipawns to avoid mixing float/int arithmetic in the inner loop.
constexpr int MATERIAL[] = {100, 300, 300, 500, 900, 0};

// ---------------------------------------------------------------------------
// Piece-square tables — centipawns, LERF order (a1=0, h8=63).
// White's perspective; black mirrors via (sq ^ 56).
// Based on the simplified evaluation function (CPW / Tomasz Michniewski).
//
// Midgame (MG) tables: king should hide behind pawns, minor pieces want
// center control. Endgame (EG) tables: king should be active and central,
// passed pawns and advancement matter more. Non-king/pawn pieces share the
// same table for both phases initially — differentiate later if tuning shows
// a benefit.
// ---------------------------------------------------------------------------

// clang-format off

// --- Midgame PSTs --- (identical to the original single-phase tables)

constexpr int8_t PST_PAWN_MG[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,   // rank 1 (never occupied)
  50, 50, 50, 50, 50, 50, 50, 50,   // rank 2
  10, 10, 20, 30, 30, 20, 10, 10,   // rank 3
   5,  5, 10, 25, 25, 10,  5,  5,   // rank 4
   0,  0,  0, 20, 20,  0,  0,  0,   // rank 5
   5, -5,-10,  0,  0,-10, -5,  5,   // rank 6
   5, 10, 10,-20,-20, 10, 10,  5,   // rank 7
   0,  0,  0,  0,  0,  0,  0,  0    // rank 8 (never occupied)
};

constexpr int8_t PST_KNIGHT_MG[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr int8_t PST_BISHOP_MG[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr int8_t PST_ROOK_MG[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0
};

constexpr int8_t PST_QUEEN_MG[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5,  5,  5,  5,  0,-10,
  -5,  0,  5,  5,  5,  5,  0, -5,
   0,  0,  5,  5,  5,  5,  0, -5,
 -10,  5,  5,  5,  5,  5,  0,-10,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr int8_t PST_KING_MG[64] = {
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,
  20, 30, 10,  0,  0, 10, 30, 20
};

// --- Endgame PSTs ---

// Pawn EG: advancement is critical — passed pawns on high ranks are valuable.
constexpr int8_t PST_PAWN_EG[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,   // rank 1 (never occupied)
  70, 70, 70, 70, 70, 70, 70, 70,   // rank 7 — about to promote
  40, 40, 40, 40, 40, 40, 40, 40,   // rank 6
  20, 20, 20, 20, 20, 20, 20, 20,   // rank 5
  10, 10, 10, 10, 10, 10, 10, 10,   // rank 4
   5,  5,  5,  5,  5,  5,  5,  5,   // rank 3
   0,  0,  0,  0,  0,  0,  0,  0,   // rank 2
   0,  0,  0,  0,  0,  0,  0,  0    // rank 8 (never occupied)
};

// King EG: wants to be active and central (opposite of MG).
constexpr int8_t PST_KING_EG[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10, 10, 20, 20, 20, 20, 10,-10,
 -10, 10, 20, 30, 30, 20, 10,-10,
 -10, 10, 20, 30, 30, 20, 10,-10,
 -10, 10, 20, 20, 20, 20, 10,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20
};

// clang-format on

// Indexed by piece type offset (PAWN=0 .. KING=5).
constexpr const int8_t* PST_MG[6] = {
    PST_PAWN_MG, PST_KNIGHT_MG, PST_BISHOP_MG,
    PST_ROOK_MG, PST_QUEEN_MG,  PST_KING_MG};

// EG tables — non-king/pawn pieces reuse MG tables.
constexpr const int8_t* PST_EG[6] = {
    PST_PAWN_EG, PST_KNIGHT_MG, PST_BISHOP_MG,
    PST_ROOK_MG, PST_QUEEN_MG,  PST_KING_EG};

// ---------------------------------------------------------------------------
// Game phase — non-pawn material determines how much the position resembles
// an opening (phase = MAX_PHASE) vs a pure endgame (phase = 0).
// Weights: N=1, B=1, R=2, Q=4 → max 4*(1+1+2+4) = 24.
// ---------------------------------------------------------------------------
constexpr int PHASE_KNIGHT = 1;
constexpr int PHASE_BISHOP = 1;
constexpr int PHASE_ROOK   = 2;
constexpr int PHASE_QUEEN  = 4;
constexpr int MAX_PHASE     = 24;  // 2*(1+1+2*2+4) = 24

// ---------------------------------------------------------------------------
// Pawn-structure masks (absorbed from chess_pieces.cpp).
// File-local — initialized lazily.
// ---------------------------------------------------------------------------

Bitboard pawnPassedMask[2][64] = {};
Bitboard pawnIsolatedMask[8] = {};
Bitboard pawnForwardMask[2][64] = {};

Bitboard adjacentFilesMask(int file) {
  Bitboard mask = 0;
  if (file > 0) mask |= fileBB(file - 1);
  if (file < 7) mask |= fileBB(file + 1);
  return mask;
}

void initPawnMasksImpl() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  for (int file = 0; file < 8; ++file)
    pawnIsolatedMask[file] = adjacentFilesMask(file);

  for (Square sq = 0; sq < 64; ++sq) {
    const int rank = sq / 8;  // 0=rank1, 7=rank8 (LERF)
    const int file = sq & 7;

    Bitboard sameAndAdjacentFiles = fileBB(file);
    if (file > 0) sameAndAdjacentFiles |= fileBB(file - 1);
    if (file < 7) sameAndAdjacentFiles |= fileBB(file + 1);

    Bitboard whitePassed = 0, blackPassed = 0;
    Bitboard whiteForward = 0, blackForward = 0;

    for (int r = rank + 1; r < 8; ++r) {
      Bitboard rankMask = rankBB(r);
      whitePassed |= sameAndAdjacentFiles & rankMask;
      whiteForward |= fileBB(file) & rankMask;
    }
    for (int r = rank - 1; r >= 0; --r) {
      Bitboard rankMask = rankBB(r);
      blackPassed |= sameAndAdjacentFiles & rankMask;
      blackForward |= fileBB(file) & rankMask;
    }

    pawnPassedMask[raw(Color::WHITE)][sq] = whitePassed;
    pawnPassedMask[raw(Color::BLACK)][sq] = blackPassed;
    pawnForwardMask[raw(Color::WHITE)][sq] = whiteForward;
    pawnForwardMask[raw(Color::BLACK)][sq] = blackForward;
  }
}

// ---------------------------------------------------------------------------
// Pawn structure constants — centipawns.
// Values are initial estimates based on CPW's Simplified Evaluation Function.
// ---------------------------------------------------------------------------

}  // anonymous namespace

// ===========================================================================
// eval — public API + internal helpers
// ===========================================================================

namespace LibreChess {
namespace eval {

using namespace LibreChess;

// ---------------------------------------------------------------------------
// Pawn-structure query functions (public for testing).
// Use the file-local mask arrays initialized by initPawnMasksImpl().
// ---------------------------------------------------------------------------

void initPawnMasks() { initPawnMasksImpl(); }

bool isPassed(Square sq, Color color, Bitboard enemyPawns) {
  return (enemyPawns & pawnPassedMask[raw(color)][sq]) == 0;
}

bool isIsolated(Square sq, Bitboard friendlyPawns) {
  return (friendlyPawns & pawnIsolatedMask[colOf(sq)]) == 0;
}

bool isDoubled(Square sq, Color color, Bitboard friendlyPawns) {
  return (friendlyPawns & pawnForwardMask[raw(color)][sq]) != 0;
}

bool isBackward(Square sq, Color color, Bitboard friendlyPawns, Bitboard enemyPawnAttacks) {
  const int file = colOf(sq);
  Bitboard adjacentPawns = friendlyPawns & pawnIsolatedMask[file];
  if (!adjacentPawns) return false;

  Square nextSq = SQ_NONE;
  if (color == Color::WHITE) {
    if (sq >= 56) return false;
    nextSq = sq + 8;
  } else {
    if (sq < 8) return false;
    nextSq = sq - 8;
  }

  Bitboard nextSquare = squareBB(nextSq);
  if ((enemyPawnAttacks & nextSquare) == 0) return false;

  Bitboard adjacentAttacks = (color == Color::WHITE)
      ? (shiftNE(adjacentPawns) | shiftNW(adjacentPawns))
      : (shiftSE(adjacentPawns) | shiftSW(adjacentPawns));

  return (adjacentAttacks & nextSquare) == 0;
}

// ---------------------------------------------------------------------------
// Pawn structure constants — centipawns.
// ---------------------------------------------------------------------------

static constexpr int PASSED_PAWN_BASE    =  20;
static constexpr int PASSED_PAWN_BASE_EG =  30;
static constexpr int PASSED_PAWN_RANK    =   5;
static constexpr int CONNECTED_PASSED    =  10;
static constexpr int ISOLATED_PENALTY    = -15;
static constexpr int DOUBLED_PENALTY     = -10;
static constexpr int BACKWARD_PENALTY    = -10;

// ---------------------------------------------------------------------------
// Pawn structure scoring — centipawns, white-relative.
// ---------------------------------------------------------------------------

static void evalPawnStructure(const BitboardSet& bb,
                              int& mgScore, int& egScore) {
  Bitboard whitePawns = bb.byPiece[0];   // pieceZobristIndex(W_PAWN) = 0
  Bitboard blackPawns = bb.byPiece[6];   // pieceZobristIndex(B_PAWN) = 6

  if (!whitePawns && !blackPawns) return;

  Bitboard blackPawnAttacks = shiftSE(blackPawns) | shiftSW(blackPawns);
  Bitboard whitePawnAttacks = shiftNE(whitePawns) | shiftNW(whitePawns);

  int mg = 0, eg = 0;
  uint8_t whitePassedFiles = 0;
  uint8_t blackPassedFiles = 0;

  // --- White pawns ---
  Bitboard wp = whitePawns;
  while (wp) {
    Square sq = popLsb(wp);
    int rank = sq / 8;

    if (isPassed(sq, Color::WHITE, blackPawns)) {
      int rankBonus = rank > 3 ? (rank - 3) * PASSED_PAWN_RANK : 0;
      mg += PASSED_PAWN_BASE + rankBonus;
      eg += PASSED_PAWN_BASE_EG + rankBonus * 2;
      whitePassedFiles |= 1 << (sq & 7);
    }
    if (isIsolated(sq, whitePawns)) {
      mg += ISOLATED_PENALTY;
      eg += ISOLATED_PENALTY;
    }
    if (isDoubled(sq, Color::WHITE, whitePawns)) {
      mg += DOUBLED_PENALTY;
      eg += DOUBLED_PENALTY;
    }
    if (isBackward(sq, Color::WHITE, whitePawns, blackPawnAttacks)) {
      mg += BACKWARD_PENALTY;
      eg += BACKWARD_PENALTY;
    }
  }

  // --- Black pawns ---
  Bitboard bp = blackPawns;
  while (bp) {
    Square sq = popLsb(bp);
    int rank = sq / 8;

    if (isPassed(sq, Color::BLACK, whitePawns)) {
      int rankBonus = rank < 4 ? (4 - rank) * PASSED_PAWN_RANK : 0;
      mg -= PASSED_PAWN_BASE + rankBonus;
      eg -= PASSED_PAWN_BASE_EG + rankBonus * 2;
      blackPassedFiles |= 1 << (sq & 7);
    }
    if (isIsolated(sq, blackPawns)) {
      mg -= ISOLATED_PENALTY;
      eg -= ISOLATED_PENALTY;
    }
    if (isDoubled(sq, Color::BLACK, blackPawns)) {
      mg -= DOUBLED_PENALTY;
      eg -= DOUBLED_PENALTY;
    }
    if (isBackward(sq, Color::BLACK, blackPawns, whitePawnAttacks)) {
      mg -= BACKWARD_PENALTY;
      eg -= BACKWARD_PENALTY;
    }
  }

  // Connected passed pawns — bonus per adjacent-file pair of passed pawns.
  for (int f = 0; f < 7; ++f) {
    if ((whitePassedFiles >> f & 1) && (whitePassedFiles >> (f + 1) & 1)) {
      mg += CONNECTED_PASSED;
      eg += CONNECTED_PASSED;
    }
    if ((blackPassedFiles >> f & 1) && (blackPassedFiles >> (f + 1) & 1)) {
      mg -= CONNECTED_PASSED;
      eg -= CONNECTED_PASSED;
    }
  }

  mgScore += mg;
  egScore += eg;
}

// ---------------------------------------------------------------------------
// Main evaluation entry point
// ---------------------------------------------------------------------------

int evaluatePosition(const BitboardSet& bb) {
  // Lazy-init pawn structure masks on first call.
  initPawnMasks();

  int mgScore = 0;
  int egScore = 0;

  for (int i = 0; i < 6; ++i) {
    Bitboard white = bb.byPiece[i];
    while (white) {
      Square sq = popLsb(white);
      mgScore += MATERIAL[i] + PST_MG[i][sq];
      egScore += MATERIAL[i] + PST_EG[i][sq];
    }

    Bitboard black = bb.byPiece[i + 6];
    while (black) {
      Square sq = popLsb(black);
      mgScore -= MATERIAL[i] + PST_MG[i][sq ^ 56];
      egScore -= MATERIAL[i] + PST_EG[i][sq ^ 56];
    }
  }

  evalPawnStructure(bb, mgScore, egScore);

  int phase = popcount(bb.byPiece[1] | bb.byPiece[7])  * PHASE_KNIGHT
            + popcount(bb.byPiece[2] | bb.byPiece[8])  * PHASE_BISHOP
            + popcount(bb.byPiece[3] | bb.byPiece[9])  * PHASE_ROOK
            + popcount(bb.byPiece[4] | bb.byPiece[10]) * PHASE_QUEEN;

  if (phase > MAX_PHASE) phase = MAX_PHASE;

  return (mgScore * phase + egScore * (MAX_PHASE - phase)) / MAX_PHASE;
}

}  // namespace eval
}  // namespace LibreChess
