#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <chess_utils.h>
#include <chess_fen.h>
#include <chess_rules.h>
#include <chess_notation.h>
#include <chess_board.h>
#include <chess_game.h>

#include <cstring>

/// Set up the standard initial chess position.
inline void setupInitialBoard(Piece board[8][8]) {
  memcpy(board, ChessBoard::INITIAL_BOARD, sizeof(ChessBoard::INITIAL_BOARD));
}

/// Clear the board (all empty squares).
inline void clearBoard(Piece board[8][8]) {
  memset(board, 0, 64 * sizeof(Piece));  // Piece::NONE == 0
}

/// Place a single piece on a cleared board at the given algebraic position.
/// Example: placePiece(board, Piece::W_KING, "e1")
inline void placePiece(Piece board[8][8], Piece piece, const char* square) {
  int col = square[0] - 'a';
  int row = 8 - (square[1] - '0');
  board[row][col] = piece;
}

/// Return whether a given move exists in the rules's possible‐move list.
inline bool moveExists(Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state = {}) {
  MoveList moves;
  ChessRules::getPossibleMoves(board, fromRow, fromCol, state, moves);
  for (int i = 0; i < moves.count; i++) {
    if (moves.row(i) == toRow && moves.col(i) == toCol)
      return true;
  }
  return false;
}

/// Algebraic square to (row, col). "e4" → (4, 4).
inline void sq(const char* s, int& row, int& col) {
  col = s[0] - 'a';
  row = 8 - (s[1] - '0');
}

/// Build a MoveEntry for common test moves.
inline MoveEntry makeEntry(int fr, int fc, int tr, int tc, Piece piece,
                           Piece captured = Piece::NONE, Piece promo = Piece::NONE) {
  MoveEntry e = {};
  e.fromRow = fr; e.fromCol = fc; e.toRow = tr; e.toCol = tc;
  e.piece = piece; e.captured = captured; e.promotion = promo;
  e.isCapture = (captured != Piece::NONE);
  e.isEnPassant = false; e.epCapturedRow = -1;
  e.isCastling = false; e.isPromotion = (promo != Piece::NONE);
  e.isCheck = false;
  return e;
}

/// Compare two enum class values via their underlying integer representation.
/// Provides clear failure messages with numeric values (e.g., "Expected 1 Was 0").
#define TEST_ASSERT_ENUM_EQ(expected, actual) \
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual))

#endif // TEST_HELPERS_H
