#ifndef CORE_CHESS_EVALUATION_H
#define CORE_CHESS_EVALUATION_H

#include "chess_bitboard.h"

namespace ChessEval {

// Evaluate board position using tapered evaluation: interpolates between
// midgame and endgame scores based on remaining non-pawn material (game phase).
// Components: material count, phase-specific piece-square tables (PSTs), and
// pawn structure analysis (passed, isolated, doubled, backward, connected).
// Returns evaluation in centipawns (positive = White, negative = Black).
// Phase: N=1, B=1, R=2, Q=4; max=24 (opening) → 0 (pure endgame).
int evaluatePosition(const ChessBitboard::BitboardSet& bb);

}  // namespace ChessEval

#endif  // CORE_CHESS_EVALUATION_H
