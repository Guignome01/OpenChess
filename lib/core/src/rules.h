#ifndef LIBRECHESS_RULES_H
#define LIBRECHESS_RULES_H

// ---------------------------------------------------------------------------
// Game-state detection — check, checkmate, stalemate, draw conditions.
//
// All functions are stateless: board representation (BitboardSet + mailbox)
// and position state are passed in as parameters.
//
// Delegates to movegen::hasAnyLegalMove for checkmate/stalemate detection.
// Delegates to attacks::isSquareUnderAttack for check detection.
// ---------------------------------------------------------------------------

#include "bitboard.h"
#include "types.h"

namespace LibreChess {
namespace rules {

// ---------------------------------------------------------------------------
// Check detection
// ---------------------------------------------------------------------------

// Is the king of `kingColor` in check?  Finds king from bitboards.
bool isCheck(const BitboardSet& bb, Color kingColor);

// Is a king of `kingColor` at `kingSq` in check?  Caller supplies king pos.
bool isCheck(const BitboardSet& bb, Square kingSq, Color kingColor);

// ---------------------------------------------------------------------------
// Checkmate / Stalemate
// ---------------------------------------------------------------------------

bool isCheckmate(const BitboardSet& bb, const Piece mailbox[],
                 Color kingColor, const PositionState& state);

bool isStalemate(const BitboardSet& bb, const Piece mailbox[],
                 Color colorToMove, const PositionState& state);

// ---------------------------------------------------------------------------
// Draw conditions
// ---------------------------------------------------------------------------

bool isInsufficientMaterial(const BitboardSet& bb);

bool isThreefoldRepetition(const HashHistory& hashes);

bool isFiftyMoveRule(const PositionState& state);

// Any draw condition met? (stalemate, 50-move, insufficient, threefold)
bool isDraw(const BitboardSet& bb, const Piece mailbox[],
            Color colorToMove, const PositionState& state,
            const HashHistory& hashes);

// ---------------------------------------------------------------------------
// Composite game-over detection
// ---------------------------------------------------------------------------

// Full game-over detection: returns GameResult and sets `winner` char
// ('w', 'b', 'd' for draw, ' ' for in-progress).
GameResult isGameOver(const BitboardSet& bb, const Piece mailbox[],
                      Color colorToMove, const PositionState& state,
                      const HashHistory& hashes, char& winner);

}  // namespace rules
}  // namespace LibreChess

#endif  // LIBRECHESS_RULES_H
