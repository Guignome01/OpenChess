#ifndef CORE_CHESS_BOARD_H
#define CORE_CHESS_BOARD_H

// Backward-compatibility forwarder — ChessBoard is now Position.
// Include position.h for the real class; this alias will be removed in Phase 6.

#include "position.h"

using ChessBoard = Position;

#endif  // CORE_CHESS_BOARD_H
