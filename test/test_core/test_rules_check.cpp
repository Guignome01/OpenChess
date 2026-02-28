#include <unity.h>

#include "../test_helpers.h"

extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// isKingInCheck
// ---------------------------------------------------------------------------

void test_king_not_in_check_initial(void) {
  setupInitialBoard(board);
  TEST_ASSERT_FALSE(ChessRules::isKingInCheck(board, 'w'));
  TEST_ASSERT_FALSE(ChessRules::isKingInCheck(board, 'b'));
}

void test_king_in_check_by_rook(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "e8"); // rook on same file
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'w'));
}

void test_king_in_check_by_bishop(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'b', "h4"); // bishop on diagonal
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'w'));
}

void test_king_in_check_by_knight(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'n', "f3"); // knight checks
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'w'));
}

void test_king_in_check_by_pawn(void) {
  placePiece(board, 'K', "e4");
  placePiece(board, 'p', "d5"); // black pawn attacks e4 from d5
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'w'));
}

void test_king_in_check_by_queen(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'q', "e8"); // queen on same file
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'w'));
}

void test_king_not_in_check_blocked(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "e8");
  placePiece(board, 'P', "e2"); // own pawn blocks rook
  TEST_ASSERT_FALSE(ChessRules::isKingInCheck(board, 'w'));
}

void test_black_king_in_check(void) {
  placePiece(board, 'k', "e8");
  placePiece(board, 'R', "e1"); // white rook attacks
  TEST_ASSERT_TRUE(ChessRules::isKingInCheck(board, 'b'));
}

// ---------------------------------------------------------------------------
// isCheckmate
// ---------------------------------------------------------------------------

void test_back_rank_mate(void) {
  // Classic back-rank mate: Black king on g8, pawns on f7/g7/h7, White rook on e8
  placePiece(board, 'k', "g8");
  placePiece(board, 'p', "f7");
  placePiece(board, 'p', "g7");
  placePiece(board, 'p', "h7");
  placePiece(board, 'R', "e8"); // delivers mate
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_TRUE(ChessRules::isCheckmate(board, 'b', flags));
}

void test_scholars_mate(void) {
  // Scholar's mate position: White queen on f7 delivers checkmate
  PositionState state;
  ChessUtils::fenToBoard("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR", board, *(new char('b')), &state);
  TEST_ASSERT_TRUE(ChessRules::isCheckmate(board, 'b', state));
}

void test_not_checkmate_can_block(void) {
  // King in check but can block
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "d1"); // own rook can block/interpose
  placePiece(board, 'r', "e8"); // attacking rook
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(board, 'w', flags));
}

void test_not_checkmate_can_escape(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "e8"); // rook checks
  // King can escape to d1, d2, f1, f2
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(board, 'w', flags));
}

void test_not_checkmate_can_capture_attacker(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "e2"); // rook checks from e2
  // King can capture the rook (assuming no support)
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(board, 'w', flags));
}

// ---------------------------------------------------------------------------
// isStalemate
// ---------------------------------------------------------------------------

void test_stalemate_king_only(void) {
  // Classic stalemate: Black king on a8, White queen on b6, White king on c6
  placePiece(board, 'k', "a8");
  placePiece(board, 'Q', "b6");
  placePiece(board, 'K', "c6");
  PositionState flags{0x00, -1, -1};
  // Black to move — king has no legal moves, not in check
  TEST_ASSERT_FALSE(ChessRules::isKingInCheck(board, 'b'));
  TEST_ASSERT_TRUE(ChessRules::isStalemate(board, 'b', flags));
}

void test_not_stalemate_has_move(void) {
  placePiece(board, 'k', "a8");
  placePiece(board, 'K', "c6");
  PositionState flags{0x00, -1, -1};
  // Black king can move to b8, b7, a7
  TEST_ASSERT_FALSE(ChessRules::isStalemate(board, 'b', flags));
}

// ---------------------------------------------------------------------------
// Move legality (can't move into check)
// ---------------------------------------------------------------------------

void test_king_cannot_move_into_check(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "f8"); // rook controls f-file
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("f1", tr, tc);
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_FALSE(ChessRules::isValidMove(board, r, c, tr, tc, flags));
}

void test_pinned_piece_cannot_move(void) {
  // Bishop pinned to its own king
  placePiece(board, 'K', "e1");
  placePiece(board, 'B', "e2"); // bishop on same file, between king and rook
  placePiece(board, 'r', "e8"); // enemy rook pins bishop
  int r, c;
  sq("e2", r, c);
  int tr, tc;
  sq("d3", tr, tc);
  PositionState flags{0x00, -1, -1};
  // Moving bishop exposes king to check → illegal
  TEST_ASSERT_FALSE(ChessRules::isValidMove(board, r, c, tr, tc, flags));
}

void test_pinned_piece_can_move_along_pin(void) {
  // Rook pinned along file — can move along that file
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "e4"); // own rook on same file
  placePiece(board, 'r', "e8"); // enemy rook pins
  int r, c;
  sq("e4", r, c);
  int tr, tc;
  sq("e8", tr, tc); // capture the pinning rook
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_TRUE(ChessRules::isValidMove(board, r, c, tr, tc, flags));
}

// ---------------------------------------------------------------------------
// findKingPosition
// ---------------------------------------------------------------------------

void test_find_king_position_white(void) {
  setupInitialBoard(board);
  int row, col;
  TEST_ASSERT_TRUE(ChessRules::findKingPosition(board, 'w', row, col));
  TEST_ASSERT_EQUAL_INT(7, row); // rank 1 = row 7
  TEST_ASSERT_EQUAL_INT(4, col); // e-file = col 4
}

void test_find_king_position_black(void) {
  setupInitialBoard(board);
  int row, col;
  TEST_ASSERT_TRUE(ChessRules::findKingPosition(board, 'b', row, col));
  TEST_ASSERT_EQUAL_INT(0, row); // rank 8 = row 0
  TEST_ASSERT_EQUAL_INT(4, col); // e-file = col 4
}

void register_rules_check_tests() {
  needsDefaultKings = false;

  // Check detection
  RUN_TEST(test_king_not_in_check_initial);
  RUN_TEST(test_king_in_check_by_rook);
  RUN_TEST(test_king_in_check_by_bishop);
  RUN_TEST(test_king_in_check_by_knight);
  RUN_TEST(test_king_in_check_by_pawn);
  RUN_TEST(test_king_in_check_by_queen);
  RUN_TEST(test_king_not_in_check_blocked);
  RUN_TEST(test_black_king_in_check);

  // Checkmate
  RUN_TEST(test_back_rank_mate);
  RUN_TEST(test_scholars_mate);
  RUN_TEST(test_not_checkmate_can_block);
  RUN_TEST(test_not_checkmate_can_escape);
  RUN_TEST(test_not_checkmate_can_capture_attacker);

  // Stalemate
  RUN_TEST(test_stalemate_king_only);
  RUN_TEST(test_not_stalemate_has_move);

  // Move legality
  RUN_TEST(test_king_cannot_move_into_check);
  RUN_TEST(test_pinned_piece_cannot_move);
  RUN_TEST(test_pinned_piece_can_move_along_pin);

  // King position
  RUN_TEST(test_find_king_position_white);
  RUN_TEST(test_find_king_position_black);
}
