#ifndef CORE_CHESS_UTILS_H
#define CORE_CHESS_UTILS_H

#include <cctype>
#include <cstdint>
#include <string>

#include "types.h"

namespace ChessUtils {

// Inline helper functions for chess logic.

inline bool isWhitePiece(char piece) {
  return (piece >= 'A' && piece <= 'Z');
}

inline bool isBlackPiece(char piece) {
  return (piece >= 'a' && piece <= 'z');
}

inline char getPieceColor(char piece) {
  if (isWhitePiece(piece)) return 'w';
  if (isBlackPiece(piece)) return 'b';
  return ' ';  // empty square or invalid
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

inline const char* gameResultName(GameResult result) {
  static constexpr const char* NAMES[] = {
      "In progress",                  // 0 = IN_PROGRESS
      "Checkmate",                    // 1 = CHECKMATE
      "Stalemate",                    // 2 = STALEMATE
      "Draw (50-move rule)",          // 3 = DRAW_50
      "Draw (threefold repetition)",  // 4 = DRAW_3FOLD
      "Resignation",                  // 5 = RESIGNATION
      "Draw (insufficient material)", // 6 = DRAW_INSUFFICIENT
      "Draw (agreement)",             // 7 = DRAW_AGREEMENT
      "Timeout",                      // 8 = TIMEOUT
      "Aborted",                      // 9 = ABORTED
  };
  auto idx = static_cast<uint8_t>(result);
  return (idx < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[idx] : "Unknown";
}

inline char opponentColor(char color) {
  return (color == 'w') ? 'b' : 'w';
}

inline int pawnDirection(char color) {
  return (color == 'w') ? -1 : 1;
}

inline int homeRow(char color) {
  return (color == 'w') ? 7 : 0;
}

// --- Castling rights ---
// Bitmask: 0x01 = K, 0x02 = Q, 0x04 = k, 0x08 = q.

inline uint8_t castlingCharToBit(char c) {
  switch (c) {
    case 'K': return 0x01;
    case 'Q': return 0x02;
    case 'k': return 0x04;
    case 'q': return 0x08;
    default: return 0;
  }
}

inline bool hasCastlingRight(uint8_t castlingRights, char pieceColor, bool kingSide) {
  char c = kingSide ? (pieceColor == 'w' ? 'K' : 'k') : (pieceColor == 'w' ? 'Q' : 'q');
  return (castlingRights & castlingCharToBit(c)) != 0;
}

inline std::string castlingRightsToString(uint8_t rights) {
  std::string s;
  for (char c : {'K', 'Q', 'k', 'q'})
    if (rights & castlingCharToBit(c)) s += c;
  if (s.empty()) s = "-";
  return s;
}

inline uint8_t castlingRightsFromString(const std::string& rightsStr) {
  uint8_t rights = 0;
  for (size_t i = 0; i < rightsStr.length(); i++)
    rights |= castlingCharToBit(rightsStr[i]);
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

// --- General-purpose board queries ---

// Is (row, col) within the 8×8 board?
inline constexpr bool isValidSquare(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8;
}

// Construct a piece char from an uppercase type letter and a color.
// makePiece('R', 'w') → 'R';  makePiece('R', 'b') → 'r'.
inline char makePiece(char type, char color) {
  return (color == 'w') ? static_cast<char>(toupper(type))
                        : static_cast<char>(tolower(type));
}

// Is char a valid promotion piece letter? (case-insensitive: q, r, b, n)
inline bool isValidPromotionChar(char c) {
  char lower = static_cast<char>(tolower(c));
  return lower == 'q' || lower == 'r' || lower == 'b' || lower == 'n';
}

// ---------------------------------------------------------------------------
// Board transform — minimal move application on a raw board array.
// Moves the piece, handles castling rook, and removes en-passant captured pawn.
// Sets capturedPiece to the piece actually captured (including EP captures).
// Does NOT update PositionState, promotion, or MoveResult — callers handle those.
// ---------------------------------------------------------------------------
void applyBoardTransform(char board[8][8], int fromRow, int fromCol,
                         int toRow, int toCol,
                         const PositionState& state, char& capturedPiece);

// Evaluate board position using simple material count
// Returns evaluation in pawns (positive = White advantage, negative = Black advantage)
// Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9
float evaluatePosition(const char board[8][8]);

// Format the board as a human-readable text block for debugging.
std::string boardToText(const char board[8][8]);

}  // namespace ChessUtils

#endif  // CORE_CHESS_UTILS_H
