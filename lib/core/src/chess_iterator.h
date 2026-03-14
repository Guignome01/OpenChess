#ifndef CORE_CHESS_ITERATOR_H
#define CORE_CHESS_ITERATOR_H

// Board iteration helpers — bitboard-based traversal.
//
// forEachSquare: iterates all 64 squares reading from the mailbox.
// forEachPiece:  iterates only occupied squares via popLsb serialization.
// somePiece:     like forEachPiece but with early-exit on true.
// findPiece:     locates all instances of a specific piece via its bitboard.
//
// All callbacks receive (int row, int col, Piece piece) for firmware compatibility.

#include "chess_bitboard.h"
#include "types.h"

namespace ChessIterator {

// Iterate all 64 squares in board order (row 0 col 0 → row 7 col 7).
// Callback: fn(int row, int col, Piece piece)
template <typename Fn>
inline void forEachSquare(const Piece mailbox[], Fn&& fn) {
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col)
      fn(row, col, mailbox[ChessBitboard::squareOf(row, col)]);
}

// Iterate only occupied squares via bitboard serialization.
// Callback: fn(int row, int col, Piece piece)
template <typename Fn>
inline void forEachPiece(const ChessBitboard::BitboardSet& bb,
                         const Piece mailbox[], Fn&& fn) {
  ChessBitboard::Bitboard occ = bb.occupied;
  while (occ) {
    ChessBitboard::Square sq = ChessBitboard::popLsb(occ);
    fn(ChessBitboard::rowOf(sq), ChessBitboard::colOf(sq), mailbox[sq]);
  }
}

// Iterate occupied squares with early exit — returns true if fn returns true.
// Callback: fn(int row, int col, Piece piece) → bool
template <typename Fn>
inline bool somePiece(const ChessBitboard::BitboardSet& bb,
                      const Piece mailbox[], Fn&& fn) {
  ChessBitboard::Bitboard occ = bb.occupied;
  while (occ) {
    ChessBitboard::Square sq = ChessBitboard::popLsb(occ);
    if (fn(ChessBitboard::rowOf(sq), ChessBitboard::colOf(sq), mailbox[sq]))
      return true;
  }
  return false;
}

// Find all instances of a specific piece.  Returns count found.
// Fills positions[][2] with [row, col] pairs, up to maxPositions.
inline int findPiece(const ChessBitboard::BitboardSet& bb, Piece target,
                     int positions[][2], int maxPositions) {
  int idx = ChessPiece::pieceZobristIndex(target);
  ChessBitboard::Bitboard pieces = bb.byPiece[idx];
  int found = 0;
  while (pieces && found < maxPositions) {
    ChessBitboard::Square sq = ChessBitboard::popLsb(pieces);
    positions[found][0] = ChessBitboard::rowOf(sq);
    positions[found][1] = ChessBitboard::colOf(sq);
    ++found;
  }
  return found;
}

}  // namespace ChessIterator

#endif  // CORE_CHESS_ITERATOR_H
