#ifndef UTILS_H
#define UTILS_H

#include <cctype>
#include <cstdint>
#include <string>

#include "types.h"

namespace ChessUtils {

// Inline helper functions for chess logic.
// No Arduino dependencies — safe for native compilation.

inline char getPieceColor(char piece) {
  return (piece >= 'a' && piece <= 'z') ? 'b' : 'w';
}

inline bool isWhitePiece(char piece) {
  return (piece >= 'A' && piece <= 'Z');
}

inline bool isBlackPiece(char piece) {
  return (piece >= 'a' && piece <= 'z');
}

inline bool isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece) {
  return (toupper(piece) == 'P' && fromCol != toCol && capturedPiece == ' ');
}

inline int getEnPassantCapturedPawnRow(int toRow, char piece) {
  return toRow - (isWhitePiece(piece) ? -1 : 1);
}

inline bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
  return (toupper(piece) == 'K' && fromRow == toRow && (toCol - fromCol == 2 || toCol - fromCol == -2));
}

inline const char* colorName(char color) {
  return (color == 'w') ? "White" : ((color == 'b') ? "Black" : "Unknown");
}

// Convert board state to FEN notation
// board: 8x8 array representing the chess board
// currentTurn: 'w' for White's turn, 'b' for Black's turn
// state: PositionState pointer to get castling rights, en passant, and clocks
// Returns: FEN string representation
std::string boardToFEN(const char board[8][8], char currentTurn, const PositionState* state = nullptr);

// Parse FEN notation and update board state
// fen: FEN string to parse
// board: 8x8 array to update with parsed position
// currentTurn: output parameter for whose turn it is - 'w' or 'b' (optional)
// state: PositionState pointer to set castling rights, en passant, and clocks
void fenToBoard(const std::string& fen, char board[8][8], char& currentTurn, PositionState* state = nullptr);

// Evaluate board position using simple material count
// Returns evaluation in pawns (positive = White advantage, negative = Black advantage)
// Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9
float evaluatePosition(const char board[8][8]);

}  // namespace ChessUtils

#endif  // UTILS_H
