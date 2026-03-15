#ifndef LIBRECHESS_EVALUATION_H
#define LIBRECHESS_EVALUATION_H

// ---------------------------------------------------------------------------
// Position evaluation — tapered eval with midgame/endgame PSTs and pawn
// structure analysis.
// ---------------------------------------------------------------------------

#include "bitboard.h"

namespace LibreChess {
namespace eval {

// Evaluate board position using tapered evaluation.
// Returns evaluation in centipawns (positive = White, negative = Black).
int evaluatePosition(const BitboardSet& bb);

// Pawn-structure query functions (exposed for unit testing).
void initPawnMasks();
bool isPassed(int sq, Color color, uint64_t enemyPawns);
bool isIsolated(int sq, uint64_t friendlyPawns);
bool isDoubled(int sq, Color color, uint64_t friendlyPawns);
bool isBackward(int sq, Color color, uint64_t friendlyPawns, uint64_t enemyPawnAttacks);

}  // namespace eval
}  // namespace LibreChess

#endif  // LIBRECHESS_EVALUATION_H
