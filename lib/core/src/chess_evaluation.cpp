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
// ---------------------------------------------------------------------------

// clang-format off
constexpr int8_t PST_PAWN[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,   // rank 1 (never occupied)
  50, 50, 50, 50, 50, 50, 50, 50,   // rank 2
  10, 10, 20, 30, 30, 20, 10, 10,   // rank 3
   5,  5, 10, 25, 25, 10,  5,  5,   // rank 4
   0,  0,  0, 20, 20,  0,  0,  0,   // rank 5
   5, -5,-10,  0,  0,-10, -5,  5,   // rank 6
   5, 10, 10,-20,-20, 10, 10,  5,   // rank 7
   0,  0,  0,  0,  0,  0,  0,  0    // rank 8 (never occupied)
};

constexpr int8_t PST_KNIGHT[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr int8_t PST_BISHOP[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr int8_t PST_ROOK[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0
};

constexpr int8_t PST_QUEEN[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5,  5,  5,  5,  0,-10,
  -5,  0,  5,  5,  5,  5,  0, -5,
   0,  0,  5,  5,  5,  5,  0, -5,
 -10,  5,  5,  5,  5,  5,  0,-10,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr int8_t PST_KING[64] = {
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,
  20, 30, 10,  0,  0, 10, 30, 20
};
// clang-format on

// Indexed by piece type offset (PAWN=0 .. KING=5).
constexpr const int8_t* PST[6] = {
    PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING};

// ---------------------------------------------------------------------------
// Pawn structure constants — centipawns.
// Values are initial estimates based on CPW's Simplified Evaluation Function.
// ---------------------------------------------------------------------------

constexpr int PASSED_PAWN_BASE =  20;  // base bonus for a passed pawn
constexpr int PASSED_PAWN_RANK =   5;  // additional bonus per rank past rank 4
constexpr int CONNECTED_PASSED =  10;  // bonus per adjacent-file pair of passers
constexpr int ISOLATED_PENALTY = -15;  // penalty for an isolated pawn
constexpr int DOUBLED_PENALTY  = -10;  // penalty for the rear pawn in a doubled pair
constexpr int BACKWARD_PENALTY = -10;  // penalty for a backward pawn

// ---------------------------------------------------------------------------
// Pawn structure evaluation — centipawns, white-relative.
// Scores passed, isolated, doubled, backward, and connected passed pawns
// using the precomputed masks in ChessPieces.
// ---------------------------------------------------------------------------

int evalPawnStructure(const ChessBitboard::BitboardSet& bb) {
  using namespace ChessBitboard;
  using namespace ChessPieces;
  using namespace ChessPiece;

  Bitboard whitePawns = bb.byPiece[0];   // pieceZobristIndex(W_PAWN) = 0
  Bitboard blackPawns = bb.byPiece[6];   // pieceZobristIndex(B_PAWN) = 6

  if (!whitePawns && !blackPawns) return 0;

  // Pawn attack spans for backward-pawn detection.
  Bitboard blackPawnAttacks = shiftSE(blackPawns) | shiftSW(blackPawns);
  Bitboard whitePawnAttacks = shiftNE(whitePawns) | shiftNW(whitePawns);

  int score = 0;
  uint8_t whitePassedFiles = 0;   // bitmask of files with white passed pawns
  uint8_t blackPassedFiles = 0;

  // --- White pawns ---
  Bitboard wp = whitePawns;
  while (wp) {
    Square sq = popLsb(wp);
    int rank = sq / 8;

    if (isPassed(sq, Color::WHITE, blackPawns)) {
      int rankBonus = rank > 3 ? (rank - 3) * PASSED_PAWN_RANK : 0;
      score += PASSED_PAWN_BASE + rankBonus;
      whitePassedFiles |= 1 << (sq & 7);
    }
    if (isIsolated(sq, whitePawns))
      score += ISOLATED_PENALTY;
    if (isDoubled(sq, Color::WHITE, whitePawns))
      score += DOUBLED_PENALTY;
    if (isBackward(sq, Color::WHITE, whitePawns, blackPawnAttacks))
      score += BACKWARD_PENALTY;
  }

  // --- Black pawns ---
  Bitboard bp = blackPawns;
  while (bp) {
    Square sq = popLsb(bp);
    int rank = sq / 8;

    if (isPassed(sq, Color::BLACK, whitePawns)) {
      int rankBonus = rank < 4 ? (4 - rank) * PASSED_PAWN_RANK : 0;
      score -= PASSED_PAWN_BASE + rankBonus;
      blackPassedFiles |= 1 << (sq & 7);
    }
    if (isIsolated(sq, blackPawns))
      score -= ISOLATED_PENALTY;
    if (isDoubled(sq, Color::BLACK, blackPawns))
      score -= DOUBLED_PENALTY;
    if (isBackward(sq, Color::BLACK, blackPawns, whitePawnAttacks))
      score -= BACKWARD_PENALTY;
  }

  // Connected passed pawns — bonus per adjacent-file pair of passed pawns.
  for (int f = 0; f < 7; ++f) {
    if ((whitePassedFiles >> f & 1) && (whitePassedFiles >> (f + 1) & 1))
      score += CONNECTED_PASSED;
    if ((blackPassedFiles >> f & 1) && (blackPassedFiles >> (f + 1) & 1))
      score -= CONNECTED_PASSED;
  }

  return score;
}

}  // namespace

namespace ChessEval {

int evaluatePosition(const ChessBitboard::BitboardSet& bb) {
  using namespace ChessBitboard;

  int score = 0;

  for (int i = 0; i < 6; ++i) {
    // White pieces — PST indexed directly by LERF square
    Bitboard white = bb.byPiece[i];
    while (white) {
      Square sq = popLsb(white);
      score += MATERIAL[i] + PST[i][sq];
    }

    // Black pieces — mirror rank via (sq ^ 56) for PST lookup
    Bitboard black = bb.byPiece[i + 6];
    while (black) {
      Square sq = popLsb(black);
      score -= MATERIAL[i] + PST[i][sq ^ 56];
    }
  }

  score += evalPawnStructure(bb);

  return score;
}

}  // namespace ChessEval
