#include <unity.h>

#include "../test_helpers.h"

extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// Pawn moves
// ---------------------------------------------------------------------------

void test_white_pawn_single_push(void) {
  placePiece(board, 'P', "e2");
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r - 1, c)); // e3
}

void test_white_pawn_double_push_from_start(void) {
  placePiece(board, 'P', "e2");
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r - 2, c)); // e4
}

void test_white_pawn_no_double_push_from_rank3(void) {
  placePiece(board, 'P', "e3");
  int r, c;
  sq("e3", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 2, c)); // e5 double push
}

void test_white_pawn_blocked(void) {
  placePiece(board, 'P', "e4");
  placePiece(board, 'p', "e5"); // blocker
  int r, c;
  sq("e4", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 1, c));
}

void test_white_pawn_double_push_blocked_on_first_sq(void) {
  placePiece(board, 'P', "e2");
  placePiece(board, 'p', "e3"); // block first square
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 2, c)); // e4
}

void test_white_pawn_capture_diagonal(void) {
  placePiece(board, 'P', "d4");
  placePiece(board, 'p', "e5"); // capturable
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e5", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
}

void test_white_pawn_no_capture_own_piece(void) {
  placePiece(board, 'P', "d4");
  placePiece(board, 'N', "e5"); // own piece
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e5", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc));
}

void test_black_pawn_single_push(void) {
  placePiece(board, 'p', "e7");
  int r, c;
  sq("e7", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r + 1, c)); // e6
}

void test_black_pawn_double_push_from_start(void) {
  placePiece(board, 'p', "e7");
  int r, c;
  sq("e7", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r + 2, c)); // e5
}

// ---------------------------------------------------------------------------
// Knight moves
// ---------------------------------------------------------------------------

void test_knight_center_moves(void) {
  placePiece(board, 'N', "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(8, moveCount); // Knight in center has 8 moves
}

void test_knight_corner_moves(void) {
  placePiece(board, 'N', "a1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(2, moveCount); // Corner knight has 2 moves
}

void test_knight_captures_enemy(void) {
  placePiece(board, 'N', "d4");
  placePiece(board, 'p', "e6"); // enemy on one of the squares
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e6", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
}

void test_knight_blocked_by_own(void) {
  placePiece(board, 'N', "d4");
  placePiece(board, 'P', "e6"); // own piece on target
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e6", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc));
}

// ---------------------------------------------------------------------------
// Bishop moves
// ---------------------------------------------------------------------------

void test_bishop_center_moves(void) {
  placePiece(board, 'B', "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(13, moveCount); // d4 bishop on empty board
}

void test_bishop_blocked_by_own(void) {
  placePiece(board, 'B', "c1");
  placePiece(board, 'P', "d2"); // own piece blocks
  placePiece(board, 'P', "b2"); // own piece blocks
  int r, c;
  sq("c1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

void test_bishop_captures_and_stops(void) {
  placePiece(board, 'B', "a1");
  placePiece(board, 'p', "d4"); // enemy blocks diagonal
  int r, c;
  sq("a1", r, c);
  int tr, tc;
  sq("d4", tr, tc);
  // Can capture d4
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
  // But cannot go past d4 to e5
  int tr2, tc2;
  sq("e5", tr2, tc2);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr2, tc2));
}

// ---------------------------------------------------------------------------
// Rook moves
// ---------------------------------------------------------------------------

void test_rook_center_moves(void) {
  placePiece(board, 'R', "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(14, moveCount); // Rook in center on empty board
}

void test_rook_blocked_by_own(void) {
  placePiece(board, 'R', "a1");
  placePiece(board, 'P', "a2"); // above
  placePiece(board, 'P', "b1"); // right
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

// ---------------------------------------------------------------------------
// Queen moves
// ---------------------------------------------------------------------------

void test_queen_center_moves(void) {
  placePiece(board, 'Q', "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(27, moveCount); // 13 bishop + 14 rook
}

// ---------------------------------------------------------------------------
// King moves
// ---------------------------------------------------------------------------

void test_king_center_moves(void) {
  placePiece(board, 'K', "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(8, moveCount); // King in center has 8 moves
}

void test_king_corner_moves(void) {
  placePiece(board, 'K', "a1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(3, moveCount); // Corner king has 3 moves
}

void test_king_no_move_onto_own(void) {
  placePiece(board, 'K', "a1");
  placePiece(board, 'P', "a2");
  placePiece(board, 'P', "b2");
  placePiece(board, 'P', "b1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

// ---------------------------------------------------------------------------
// Initial position move counts
// ---------------------------------------------------------------------------

void test_initial_position_white_moves(void) {
  setupInitialBoard(board);
  PositionState flags{0x0F, -1, -1};

  // Each of the 8 pawns has 2 moves, each of 2 knights has 2 moves = 20 total
  int totalMoves = 0;
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      char piece = board[row][col];
      if (piece >= 'A' && piece <= 'Z') {
        int moveCount = 0;
        int moves[28][2];
        ChessRules::getPossibleMoves(board, row, col, flags, moveCount, moves);
        totalMoves += moveCount;
      }
    }
  TEST_ASSERT_EQUAL_INT(20, totalMoves);
}

void register_rules_moves_tests() {
  needsDefaultKings = true;

  // Pawns
  RUN_TEST(test_white_pawn_single_push);
  RUN_TEST(test_white_pawn_double_push_from_start);
  RUN_TEST(test_white_pawn_no_double_push_from_rank3);
  RUN_TEST(test_white_pawn_blocked);
  RUN_TEST(test_white_pawn_double_push_blocked_on_first_sq);
  RUN_TEST(test_white_pawn_capture_diagonal);
  RUN_TEST(test_white_pawn_no_capture_own_piece);
  RUN_TEST(test_black_pawn_single_push);
  RUN_TEST(test_black_pawn_double_push_from_start);

  // Knights
  RUN_TEST(test_knight_center_moves);
  RUN_TEST(test_knight_corner_moves);
  RUN_TEST(test_knight_captures_enemy);
  RUN_TEST(test_knight_blocked_by_own);

  // Bishops
  RUN_TEST(test_bishop_center_moves);
  RUN_TEST(test_bishop_blocked_by_own);
  RUN_TEST(test_bishop_captures_and_stops);

  // Rooks
  RUN_TEST(test_rook_center_moves);
  RUN_TEST(test_rook_blocked_by_own);

  // Queen
  RUN_TEST(test_queen_center_moves);

  // King
  RUN_TEST(test_king_center_moves);
  RUN_TEST(test_king_corner_moves);
  RUN_TEST(test_king_no_move_onto_own);

  // Initial position
  RUN_TEST(test_initial_position_white_moves);
}
