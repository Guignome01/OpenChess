#ifndef CORE_CHESS_EVALUATION_H
#define CORE_CHESS_EVALUATION_H

#include "chess_bitboard.h"

namespace ChessEval {

// Evaluate board position using material count + piece-square tables (PSTs).
// Returns evaluation in pawns (positive = White advantage, negative = Black).
// PSTs add positional bonuses/penalties in centipawns (e.g. centralized knights,
// advanced pawns, castled kings score higher). Material values: P=1 N=3 B=3 R=5 Q=9.
float evaluatePosition(const ChessBitboard::BitboardSet& bb);

}  // namespace ChessEval

#endif  // CORE_CHESS_EVALUATION_H
