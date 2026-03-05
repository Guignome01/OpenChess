#ifndef CORE_CHESS_FEN_H
#define CORE_CHESS_FEN_H

#include <string>

#include "types.h"

namespace ChessFEN {

// Convert board state to FEN notation.
// board: 8x8 array (row 0 = rank 8, col 0 = file a).
// currentTurn: 'w' or 'b'.
// state: optional PositionState for castling, en passant, clocks.
std::string boardToFEN(const char board[8][8], char currentTurn, const PositionState* state = nullptr);

// Parse FEN notation and populate board, turn, and state.
// Lenient parser — does not reject invalid FEN. Use validateFEN() first for untrusted input.
void fenToBoard(const std::string& fen, char board[8][8], char& currentTurn, PositionState* state = nullptr);

// Validate FEN string format.
// Checks: non-empty, 7 slashes, each rank sums to 8, valid piece chars,
// turn field (w|b), castling field ([KQkq]+|-), en passant ([a-h][36]|-),
// halfmove >= 0, fullmove >= 1.
bool validateFEN(const std::string& fen);

}  // namespace ChessFEN

#endif  // CORE_CHESS_FEN_H
