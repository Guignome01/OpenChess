#include "chess_evaluation.h"

#include "chess_pieces.h"

namespace {

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
// Pawn structure constants — centipawns.
// Values are initial estimates based on CPW's Simplified Evaluation Function.
// ---------------------------------------------------------------------------

constexpr int PASSED_PAWN_BASE =  20;  // base bonus for a passed pawn (midgame)
constexpr int PASSED_PAWN_BASE_EG = 30;  // base bonus for a passed pawn (endgame)
constexpr int PASSED_PAWN_RANK =   5;  // additional bonus per rank past rank 4
constexpr int CONNECTED_PASSED =  10;  // bonus per adjacent-file pair of passers
constexpr int ISOLATED_PENALTY = -15;  // penalty for an isolated pawn
constexpr int DOUBLED_PENALTY  = -10;  // penalty for the rear pawn in a doubled pair
constexpr int BACKWARD_PENALTY = -10;  // penalty for a backward pawn

// ---------------------------------------------------------------------------
// Pawn structure evaluation — centipawns, white-relative.
// Scores passed, isolated, doubled, backward, and connected passed pawns
// using the precomputed masks in ChessPieces.
// Returns separate midgame and endgame scores via out-parameters.
// ---------------------------------------------------------------------------

void evalPawnStructure(const ChessBitboard::BitboardSet& bb,
                       int& mgScore, int& egScore) {
  using namespace ChessBitboard;
  using namespace ChessPieces;
  using namespace ChessPiece;

  Bitboard whitePawns = bb.byPiece[0];   // pieceZobristIndex(W_PAWN) = 0
  Bitboard blackPawns = bb.byPiece[6];   // pieceZobristIndex(B_PAWN) = 6

  if (!whitePawns && !blackPawns) return;

  // Pawn attack spans for backward-pawn detection.
  Bitboard blackPawnAttacks = shiftSE(blackPawns) | shiftSW(blackPawns);
  Bitboard whitePawnAttacks = shiftNE(whitePawns) | shiftNW(whitePawns);

  int mg = 0, eg = 0;
  uint8_t whitePassedFiles = 0;   // bitmask of files with white passed pawns
  uint8_t blackPassedFiles = 0;

  // --- White pawns ---
  Bitboard wp = whitePawns;
  while (wp) {
    Square sq = popLsb(wp);
    int rank = sq / 8;

    if (isPassed(sq, Color::WHITE, blackPawns)) {
      int rankBonus = rank > 3 ? (rank - 3) * PASSED_PAWN_RANK : 0;
      mg += PASSED_PAWN_BASE + rankBonus;
      // Passed pawns are more valuable in endgames.
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

}  // namespace

namespace ChessEval {

int evaluatePosition(const ChessBitboard::BitboardSet& bb) {
  using namespace ChessBitboard;

  int mgScore = 0;
  int egScore = 0;

  for (int i = 0; i < 6; ++i) {
    // White pieces — PST indexed directly by LERF square
    Bitboard white = bb.byPiece[i];
    while (white) {
      Square sq = popLsb(white);
      mgScore += MATERIAL[i] + PST_MG[i][sq];
      egScore += MATERIAL[i] + PST_EG[i][sq];
    }

    // Black pieces — mirror rank via (sq ^ 56) for PST lookup
    Bitboard black = bb.byPiece[i + 6];
    while (black) {
      Square sq = popLsb(black);
      mgScore -= MATERIAL[i] + PST_MG[i][sq ^ 56];
      egScore -= MATERIAL[i] + PST_EG[i][sq ^ 56];
    }
  }

  // Pawn structure bonuses/penalties (accumulated into both scores).
  evalPawnStructure(bb, mgScore, egScore);

  // Game phase from non-pawn material (both sides combined).
  // byPiece indices: 1=W_KNIGHT, 2=W_BISHOP, 3=W_ROOK, 4=W_QUEEN,
  //                  7=B_KNIGHT, 8=B_BISHOP, 9=B_ROOK, 10=B_QUEEN.
  int phase = popcount(bb.byPiece[1] | bb.byPiece[7])  * PHASE_KNIGHT
            + popcount(bb.byPiece[2] | bb.byPiece[8])  * PHASE_BISHOP
            + popcount(bb.byPiece[3] | bb.byPiece[9])  * PHASE_ROOK
            + popcount(bb.byPiece[4] | bb.byPiece[10]) * PHASE_QUEEN;

  // Clamp to MAX_PHASE (shouldn't exceed it, but be safe).
  if (phase > MAX_PHASE) phase = MAX_PHASE;

  // Tapered interpolation: opening → mg, endgame → eg.
  return (mgScore * phase + egScore * (MAX_PHASE - phase)) / MAX_PHASE;
}

}  // namespace ChessEval
