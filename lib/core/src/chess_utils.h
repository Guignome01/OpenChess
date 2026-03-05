#ifndef CORE_CHESS_UTILS_H
#define CORE_CHESS_UTILS_H

#include <cctype>
#include <cstdint>
#include <string>

#include "types.h"

namespace ChessUtils {

// Inline helper functions for chess logic.
// No Arduino dependencies — safe for native compilation.

inline char getPieceColor(char piece) {
  if (piece >= 'a' && piece <= 'z') return 'b';
  if (piece >= 'A' && piece <= 'Z') return 'w';
  return ' ';  // empty square or invalid
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

inline char opponentColor(char color) {
  return (color == 'w') ? 'b' : 'w';
}

// --- Castling rights bitmask ↔ FEN string ---
// Bitmask: 0x01 = K, 0x02 = Q, 0x04 = k, 0x08 = q.

inline std::string castlingRightsToString(uint8_t rights) {
  std::string s;
  if (rights & 0x01) s += 'K';
  if (rights & 0x02) s += 'Q';
  if (rights & 0x04) s += 'k';
  if (rights & 0x08) s += 'q';
  if (s.empty()) s = "-";
  return s;
}

inline uint8_t castlingRightsFromString(const std::string& rightsStr) {
  uint8_t rights = 0;
  for (size_t i = 0; i < rightsStr.length(); i++) {
    switch (rightsStr[i]) {
      case 'K': rights |= 0x01; break;
      case 'Q': rights |= 0x02; break;
      case 'k': rights |= 0x04; break;
      case 'q': rights |= 0x08; break;
      default: break;
    }
  }
  return rights;
}

// Coordinate helpers — single source of truth for the row/col ↔ rank/file mapping.
// Board convention: row 0 = rank 8 (black back rank), col 0 = file 'a'.
inline constexpr char fileChar(int col) { return 'a' + col; }
inline constexpr char rankChar(int row) { return '1' + (7 - row); }
inline constexpr int fileIndex(char file) { return file - 'a'; }
inline constexpr int rankIndex(char rank) { return 8 - (rank - '0'); }

inline std::string squareName(int row, int col) {
  return {fileChar(col), rankChar(row)};
}

// ---------------------------------------------------------------------------
// En passant analysis — combines EP-capture detection and EP-target setting
// into one return value so callers don't scatter multiple inline checks.
// ---------------------------------------------------------------------------

struct EnPassantInfo {
  bool isCapture;       // This move is an EP capture
  int capturedPawnRow;  // Row of captured EP pawn (-1 if not EP)
  int nextEpRow;        // EP target row for the *next* move (-1 if none)
  int nextEpCol;        // EP target col for the *next* move (-1 if none)
};

inline EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol, char piece, char targetSquare) {
  EnPassantInfo info{};
  info.capturedPawnRow = -1;
  info.nextEpRow = -1;
  info.nextEpCol = -1;

  info.isCapture = isEnPassantMove(fromRow, fromCol, toRow, toCol, piece, targetSquare);
  if (info.isCapture)
    info.capturedPawnRow = getEnPassantCapturedPawnRow(toRow, piece);

  // Pawn double-push creates an EP target for the opponent
  if (toupper(piece) == 'P' && abs(toRow - fromRow) == 2) {
    info.nextEpRow = (fromRow + toRow) / 2;
    info.nextEpCol = fromCol;
  }

  return info;
}

// ---------------------------------------------------------------------------
// Castling analysis — combines castling detection + rook positions so the
// board layer can apply the move in one pass.
// ---------------------------------------------------------------------------

struct CastlingInfo {
  bool isCastling;   // This move is castling
  int rookFromCol;   // Rook source column (-1 if not castling)
  int rookToCol;     // Rook destination column (-1 if not castling)
};

inline CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol, char piece) {
  CastlingInfo info{};
  info.rookFromCol = -1;
  info.rookToCol = -1;

  info.isCastling = isCastlingMove(fromRow, fromCol, toRow, toCol, piece);
  if (info.isCastling) {
    int deltaCol = toCol - fromCol;
    info.rookFromCol = (deltaCol == 2) ? 7 : 0;
    info.rookToCol = (deltaCol == 2) ? 5 : 3;
  }

  return info;
}

// ---------------------------------------------------------------------------
// Castling rights update — pure function returning new rights bitmask.
// ---------------------------------------------------------------------------

inline uint8_t updateCastlingRights(uint8_t rights, int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece) {
  // King moved — lose both rights for that color
  if (movedPiece == 'K')
    rights &= ~(0x01 | 0x02);
  else if (movedPiece == 'k')
    rights &= ~(0x04 | 0x08);

  // Rook moved from corner — lose that side's right
  if (movedPiece == 'R') {
    if (fromRow == 7 && fromCol == 7) rights &= ~0x01;
    if (fromRow == 7 && fromCol == 0) rights &= ~0x02;
  } else if (movedPiece == 'r') {
    if (fromRow == 0 && fromCol == 7) rights &= ~0x04;
    if (fromRow == 0 && fromCol == 0) rights &= ~0x08;
  }

  // Rook captured on corner — lose that side's right
  if (capturedPiece == 'R') {
    if (toRow == 7 && toCol == 7) rights &= ~0x01;
    if (toRow == 7 && toCol == 0) rights &= ~0x02;
  } else if (capturedPiece == 'r') {
    if (toRow == 0 && toCol == 7) rights &= ~0x04;
    if (toRow == 0 && toCol == 0) rights &= ~0x08;
  }

  return rights;
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

#endif  // CORE_CHESS_UTILS_H
