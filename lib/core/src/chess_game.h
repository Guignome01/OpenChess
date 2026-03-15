// Backward-compatibility forwarder — includes the canonical game.h and
// provides a type alias so existing code using ChessGame still compiles.
#ifndef CORE_CHESS_GAME_H
#define CORE_CHESS_GAME_H

#include "game.h"
#include "chess_history.h"  // backward-compat: ChessHistory alias

using ChessGame = Game;

#endif  // CORE_CHESS_GAME_H
