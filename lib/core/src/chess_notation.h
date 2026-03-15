// Backward-compatibility forwarder — includes the canonical notation.h and
// provides namespace alias so existing code using ChessNotation:: still compiles.
#ifndef CORE_CHESS_NOTATION_H
#define CORE_CHESS_NOTATION_H

#include "notation.h"

namespace ChessNotation = LibreChess::notation;

#endif  // CORE_CHESS_NOTATION_H
