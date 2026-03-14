#ifndef CORE_CHESS_EVALUATION_H
#define CORE_CHESS_EVALUATION_H

#include "chess_bitboard.h"

namespace ChessEval {

// Evaluate board position using material count + piece-square tables (PSTs).
// Returns evaluation in centipawns (positive = White advantage, negative = Black).
// PSTs add positional bonuses/penalties (e.g. centralized knights,
// advanced pawns, castled kings score higher). Material: P=100 N=300 B=300 R=500 Q=900.
int evaluatePosition(const ChessBitboard::BitboardSet& bb);

}  // namespace ChessEval

#endif  // CORE_CHESS_EVALUATION_H
