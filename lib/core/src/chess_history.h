// Backward-compatibility forwarder — includes the canonical history.h and
// provides a type alias so existing code using ChessHistory still compiles.
#ifndef CORE_CHESS_HISTORY_H
#define CORE_CHESS_HISTORY_H

#include "history.h"

using ChessHistory = History;
using ChessBoard = Position;  // backward-compat alias (also in chess_board.h)

#endif  // CORE_CHESS_HISTORY_H
