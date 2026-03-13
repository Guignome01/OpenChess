#ifndef CORE_CHESS_PIECE_H
#define CORE_CHESS_PIECE_H

#include <cstdint>

// Piece encoding types and helpers for the core chess library.
// Uses a standard bit-layout: Piece = (Color << 3) | PieceType
// enabling O(1) extraction of type and color via bit operations.
//
// Board squares hold a Piece value; Piece::NONE (0) represents an empty square,
// which makes zero-initialization equivalent to clearing the board.

namespace ChessPiece {

// ---------------------------------------------------------------------------
// Core enums
// ---------------------------------------------------------------------------

enum class Color : uint8_t { WHITE = 0, BLACK = 1 };

enum class PieceType : uint8_t {
  NONE = 0,
  PAWN = 1,
  KNIGHT = 2,
  BISHOP = 3,
  ROOK = 4,
  QUEEN = 5,
  KING = 6
};

// Unwrap enum to underlying uint8_t — eliminates static_cast noise.
constexpr uint8_t raw(Color c)     { return static_cast<uint8_t>(c); }
constexpr uint8_t raw(PieceType t) { return static_cast<uint8_t>(t); }

// Color bit offset for Piece composition.
constexpr uint8_t BLACK_BIT = raw(Color::BLACK) << 3;  // = 8

// Piece = (Color << 3) | PieceType.
// Values 7, 8, 15 are unused gaps — this is intentional for bit extraction.
enum class Piece : uint8_t {
  NONE     = 0,
  W_PAWN   = raw(PieceType::PAWN),
  W_KNIGHT = raw(PieceType::KNIGHT),
  W_BISHOP = raw(PieceType::BISHOP),
  W_ROOK   = raw(PieceType::ROOK),
  W_QUEEN  = raw(PieceType::QUEEN),
  W_KING   = raw(PieceType::KING),
  B_PAWN   = BLACK_BIT | raw(PieceType::PAWN),
  B_KNIGHT = BLACK_BIT | raw(PieceType::KNIGHT),
  B_BISHOP = BLACK_BIT | raw(PieceType::BISHOP),
  B_ROOK   = BLACK_BIT | raw(PieceType::ROOK),
  B_QUEEN  = BLACK_BIT | raw(PieceType::QUEEN),
  B_KING   = BLACK_BIT | raw(PieceType::KING),
};

// Deferred definition (needs complete Piece type).
constexpr uint8_t raw(Piece p) { return static_cast<uint8_t>(p); }

// ---------------------------------------------------------------------------
// Bit extraction (constexpr, O(1))
// ---------------------------------------------------------------------------

constexpr PieceType pieceType(Piece p) {
  return static_cast<PieceType>(raw(p) & 0x07);
}

// Only valid when p != Piece::NONE.
constexpr Color pieceColor(Piece p) {
  return static_cast<Color>(raw(p) >> 3);
}

constexpr Piece makePiece(Color c, PieceType t) {
  return static_cast<Piece>((raw(c) << 3) | raw(t));
}

// ---------------------------------------------------------------------------
// Predicates
// ---------------------------------------------------------------------------

constexpr bool isEmpty(Piece p) { return p == Piece::NONE; }

constexpr bool isWhite(Piece p) {
  return !isEmpty(p) && pieceColor(p) == Color::WHITE;
}

constexpr bool isBlack(Piece p) {
  return !isEmpty(p) && pieceColor(p) == Color::BLACK;
}

constexpr bool isColor(Piece p, Color c) {
  return !isEmpty(p) && pieceColor(p) == c;
}

// ---------------------------------------------------------------------------
// Color flip operators
// ---------------------------------------------------------------------------

constexpr Color operator~(Color c) {
  return static_cast<Color>(raw(c) ^ 1);
}

constexpr Piece operator~(Piece p) {
  // Flip color bit (bit 3). Only meaningful for non-NONE pieces.
  return static_cast<Piece>(raw(p) ^ BLACK_BIT);
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

constexpr int pawnDirection(Color c) {
  return c == Color::WHITE ? -1 : 1;
}

constexpr int homeRow(Color c) {
  return c == Color::WHITE ? 7 : 0;
}

constexpr int promotionRow(Color c) {
  return c == Color::WHITE ? 0 : 7;
}

constexpr bool isPromotion(Piece piece, int targetRow) {
  return pieceType(piece) == PieceType::PAWN &&
         targetRow == promotionRow(pieceColor(piece));
}

inline const char* colorName(Color c) {
  return c == Color::WHITE ? "White" : "Black";
}

// ---------------------------------------------------------------------------
// FEN boundary conversion — lookup tables for char ↔ Piece
// ---------------------------------------------------------------------------

constexpr Piece charToPiece(char c) {
  // Covers the 12 standard FEN piece characters.
  switch (c) {
    case 'P': return Piece::W_PAWN;
    case 'N': return Piece::W_KNIGHT;
    case 'B': return Piece::W_BISHOP;
    case 'R': return Piece::W_ROOK;
    case 'Q': return Piece::W_QUEEN;
    case 'K': return Piece::W_KING;
    case 'p': return Piece::B_PAWN;
    case 'n': return Piece::B_KNIGHT;
    case 'b': return Piece::B_BISHOP;
    case 'r': return Piece::B_ROOK;
    case 'q': return Piece::B_QUEEN;
    case 'k': return Piece::B_KING;
    default:  return Piece::NONE;
  }
}

constexpr char pieceToChar(Piece p) {
  constexpr char TABLE[] = {
      ' ',  // 0  NONE
      'P',  // 1  W_PAWN
      'N',  // 2  W_KNIGHT
      'B',  // 3  W_BISHOP
      'R',  // 4  W_ROOK
      'Q',  // 5  W_QUEEN
      'K',  // 6  W_KING
      '?',  // 7  (unused)
      '?',  // 8  (unused)
      'p',  // 9  B_PAWN
      'n',  // 10 B_KNIGHT
      'b',  // 11 B_BISHOP
      'r',  // 12 B_ROOK
      'q',  // 13 B_QUEEN
      'k',  // 14 B_KING
      '?',  // 15 (unused)
  };
  return (raw(p) < sizeof(TABLE)) ? TABLE[raw(p)] : '?';
}

// Char to PieceType (case-insensitive: 'Q' → QUEEN, 'n' → KNIGHT).
constexpr PieceType charToPieceType(char c) {
  switch (c >= 'a' ? (c - 32) : c) {  // toupper equivalent
    case 'P': return PieceType::PAWN;
    case 'N': return PieceType::KNIGHT;
    case 'B': return PieceType::BISHOP;
    case 'R': return PieceType::ROOK;
    case 'Q': return PieceType::QUEEN;
    case 'K': return PieceType::KING;
    default:  return PieceType::NONE;
  }
}

constexpr char pieceTypeToChar(PieceType t) {
  constexpr char TABLE[] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};
  return (raw(t) < sizeof(TABLE)) ? TABLE[raw(t)] : '?';
}

// ---------------------------------------------------------------------------
// Material values — indexed by PieceType (color-independent).
// ---------------------------------------------------------------------------

constexpr float pieceTypeValue(PieceType t) {
  constexpr float V[] = {0, 1, 3, 3, 5, 9, 0};  // NONE..KING
  return (raw(t) < sizeof(V) / sizeof(V[0])) ? V[raw(t)] : 0;
}

constexpr float pieceValue(Piece p) { return pieceTypeValue(pieceType(p)); }

// ---------------------------------------------------------------------------
// Zobrist index — maps Piece to 0..11 for the Zobrist key table.
// Replaces ChessHash::pieceIndex(char).
// ---------------------------------------------------------------------------

constexpr int pieceZobristIndex(Piece p) {
  // White pieces: PAWN=0 KNIGHT=1 BISHOP=2 ROOK=3 QUEEN=4 KING=5
  // Black pieces: PAWN=6 KNIGHT=7 BISHOP=8 ROOK=9 QUEEN=10 KING=11
  if (isEmpty(p)) return -1;
  int typeIdx = raw(pieceType(p)) - 1;  // PAWN=0..KING=5
  return (pieceColor(p) == Color::BLACK) ? typeIdx + 6 : typeIdx;
}

// ---------------------------------------------------------------------------
// Color conversion helpers (for migration boundary)
// ---------------------------------------------------------------------------

constexpr Color charToColor(char c) {
  return (c == 'b' || c == 'B') ? Color::BLACK : Color::WHITE;
}

constexpr char colorToChar(Color c) {
  return c == Color::WHITE ? 'w' : 'b';
}

}  // namespace ChessPiece

// Bring core piece types into enclosing scope (project-wide convenience,
// consistent with GameResult / PositionState being at file scope).
using ChessPiece::Color;
using ChessPiece::Piece;
using ChessPiece::PieceType;

#endif  // CORE_CHESS_PIECE_H
