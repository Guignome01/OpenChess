#ifndef CORE_CHESS_EVALUATION_H
#define CORE_CHESS_EVALUATION_H

// Backward-compatibility forwarder — delegates to LibreChess::eval.
// Removed in Phase 6.

#include "evaluation.h"

namespace ChessEval = LibreChess::eval;

#endif  // CORE_CHESS_EVALUATION_H
