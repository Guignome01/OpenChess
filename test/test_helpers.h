#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <chess_utils.h>
#include <chess_rules.h>
#include <chess_codec.h>
#include <chess_board.h>
#include <chess_game.h>

#include <cstring>

/// Set up the standard initial chess position.
inline void setupInitialBoard(char board[8][8]) {
  const char initial[8][8] = {
      {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
      {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
      {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
      {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
      {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'},
  };
  memcpy(board, initial, sizeof(initial));
}

/// Clear the board (all empty squares).
inline void clearBoard(char board[8][8]) {
  memset(board, ' ', 64);
}

/// Place a single piece on a cleared board at the given algebraic position.
/// Example: placePiece(board, 'K', "e1") places a White King on e1.
inline void placePiece(char board[8][8], char piece, const char* square) {
  int col = square[0] - 'a';
  int row = 8 - (square[1] - '0');
  board[row][col] = piece;
}

/// Return whether a given move exists in the rules's possible‐move list.
inline bool moveExists(char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags = {}) {
  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, fromRow, fromCol, flags, moveCount, moves);
  for (int i = 0; i < moveCount; i++) {
    if (moves[i][0] == toRow && moves[i][1] == toCol)
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
inline MoveEntry makeEntry(int fr, int fc, int tr, int tc, char piece,
                           char captured = ' ', char promo = ' ') {
  MoveEntry e = {};
  e.fromRow = fr; e.fromCol = fc; e.toRow = tr; e.toCol = tc;
  e.piece = piece; e.captured = captured; e.promotion = promo;
  e.isCapture = (captured != ' ');
  e.isEnPassant = false; e.epCapturedRow = -1;
  e.isCastling = false; e.isPromotion = (promo != ' ');
  e.isCheck = false;
  return e;
}

/// Compare two enum class values via their underlying integer representation.
/// Provides clear failure messages with numeric values (e.g., "Expected 1 Was 0").
#define TEST_ASSERT_ENUM_EQ(expected, actual) \
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual))

#endif // TEST_HELPERS_H
