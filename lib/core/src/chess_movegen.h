#ifndef CORE_CHESS_MOVEGEN_H
#define CORE_CHESS_MOVEGEN_H

// Legacy forwarder — all attack tables and slider functions now live in
// attacks.h under the LibreChess::attacks namespace.
// The ChessMovegen namespace alias is provided below.
// This header is retained during migration; removed in Phase 6.

#include "attacks.h"

namespace ChessMovegen = LibreChess::attacks;

#endif  // CORE_CHESS_MOVEGEN_H
