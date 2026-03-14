#ifndef CORE_CHESS_FEN_H
#define CORE_CHESS_FEN_H

#include <string>

#include "chess_bitboard.h"
#include "types.h"

namespace ChessFEN {

// Convert board state to FEN notation.
// mailbox: flat 64-element array (LERF indexing, a1=0 h8=63).
// currentTurn: Color::WHITE or Color::BLACK.
// state: optional PositionState for castling, en passant, clocks.
std::string boardToFEN(const Piece mailbox[], Color currentTurn, const PositionState* state = nullptr);

// Parse FEN notation and populate bitboards, mailbox, turn, and state.
// Lenient parser — does not reject invalid FEN. Use validateFEN() first for untrusted input.
void fenToBoard(const std::string& fen, ChessBitboard::BitboardSet& bb, Piece mailbox[],
                Color& currentTurn, PositionState* state = nullptr);

// Validate FEN string format.
// Checks: non-empty, 7 slashes, each rank sums to 8, valid piece chars,
// turn field (w|b), castling field ([KQkq]+|-), en passant ([a-h][36]|-),
// halfmove >= 0, fullmove >= 1.
bool validateFEN(const std::string& fen);

}  // namespace ChessFEN

#endif  // CORE_CHESS_FEN_H
