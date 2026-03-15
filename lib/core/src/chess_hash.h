#ifndef CORE_CHESS_HASH_H
#define CORE_CHESS_HASH_H

// Backward-compatibility forwarder — ChessHash is now LibreChess::zobrist.
// This alias will be removed in Phase 6.

#include "zobrist.h"

namespace ChessHash = LibreChess::zobrist;

#endif  // CORE_CHESS_HASH_H
