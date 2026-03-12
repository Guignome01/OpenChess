#ifndef CORE_CHESS_ITERATOR_H
#define CORE_CHESS_ITERATOR_H

#include "chess_piece.h"

namespace ChessIterator {

namespace detail {

// The single nested loop powering every public iterator.
// fn(row, col, piece) -> bool: return true to stop early.
template <typename Fn>
inline bool forEachSquareUntil(const Piece board[8][8], Fn&& fn) {
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c)
      if (fn(r, c, board[r][c])) return true;
  return false;
}

}  // namespace detail

// Call fn(row, col, piece) for every square on the board.
template <typename Fn>
inline void forEachSquare(const Piece board[8][8], Fn&& fn) {
  detail::forEachSquareUntil(board, [&](int r, int c, Piece piece) {
    fn(r, c, piece);
    return false;
  });
}

// Call fn(row, col, piece) for every occupied square.
template <typename Fn>
inline void forEachPiece(const Piece board[8][8], Fn&& fn) {
  detail::forEachSquareUntil(board, [&](int r, int c, Piece piece) {
    if (piece != Piece::NONE) fn(r, c, piece);
    return false;
  });
}

// Return true if fn(row, col, piece) returns true for any occupied square.
// Stops on first match.
template <typename Fn>
inline bool somePiece(const Piece board[8][8], Fn&& fn) {
  return detail::forEachSquareUntil(board, [&](int r, int c, Piece piece) {
    if (piece == Piece::NONE) return false;
    return fn(r, c, piece);
  });
}

// Find all instances of the given piece on the board.
// Returns count; fills positions[][2] with [row, col] pairs.
inline int findPiece(const Piece board[8][8], Piece target,
                     int positions[][2], int maxPositions) {
  int count = 0;
  detail::forEachSquareUntil(board, [&](int r, int c, Piece piece) {
    if (piece == target) {
      positions[count][0] = r;
      positions[count][1] = c;
      ++count;
    }
    return count >= maxPositions;
  });
  return count;
}

}  // namespace ChessIterator

#endif  // CORE_CHESS_ITERATOR_H
