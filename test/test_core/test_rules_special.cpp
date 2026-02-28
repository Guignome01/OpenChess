#include <unity.h>

#include "../test_helpers.h"

extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// Castling
// ---------------------------------------------------------------------------

void test_white_kingside_castle_available(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "h1");
  PositionState flags{0x0F, -1, -1}; // all rights
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("g1", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc, flags));
}

void test_white_queenside_castle_available(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "a1");
  PositionState flags{0x0F, -1, -1};
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("c1", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc, flags));
}

void test_black_kingside_castle_available(void) {
  placePiece(board, 'k', "e8");
  placePiece(board, 'r', "h8");
  PositionState flags{0x0F, -1, -1};
  int r, c;
  sq("e8", r, c);
  int tr, tc;
  sq("g8", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc, flags));
}

void test_castle_blocked_by_piece(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "h1");
  placePiece(board, 'N', "g1"); // knight blocks
  PositionState flags{0x0F, -1, -1};
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("g1", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc, flags));
}

void test_castle_through_check_forbidden(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "h1");
  placePiece(board, 'r', "f8"); // rook controls f1 — king passes through check
  PositionState flags{0x0F, -1, -1};
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("g1", tr, tc);
  TEST_ASSERT_FALSE(ChessRules::isValidMove(board, r, c, tr, tc, flags));
}

void test_castle_while_in_check_forbidden(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "h1");
  placePiece(board, 'r', "e8"); // rook gives check on e-file
  PositionState flags{0x0F, -1, -1};
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("g1", tr, tc);
  TEST_ASSERT_FALSE(ChessRules::isValidMove(board, r, c, tr, tc, flags));
}

void test_no_castle_right_revoked(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "h1");
  PositionState flags{0x00, -1, -1}; // no rights
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("g1", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc, flags));
}

// ---------------------------------------------------------------------------
// En passant
// ---------------------------------------------------------------------------

void test_en_passant_white_captures(void) {
  // White pawn on e5, Black pawn just double-pushed to d5
  placePiece(board, 'P', "e5");
  placePiece(board, 'p', "d5");
  int epR, epC;
  sq("d6", epR, epC);
  PositionState flags{0x0F, epR, epC}; // d6

  int r, c;
  sq("e5", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, epR, epC, flags));
}

void test_en_passant_black_captures(void) {
  placePiece(board, 'p', "d4");
  placePiece(board, 'P', "e4");
  int epR, epC;
  sq("e3", epR, epC);
  PositionState flags{0x0F, epR, epC}; // e3

  int r, c;
  sq("d4", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, epR, epC, flags));
}

void test_en_passant_not_available(void) {
  // Pawns adjacent but no en passant target set
  placePiece(board, 'P', "e5");
  placePiece(board, 'p', "d5");
  // Default flags — no en passant

  int r, c;
  sq("e5", r, c);
  int tr, tc;
  sq("d6", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc));
}

// ---------------------------------------------------------------------------
// Pawn promotion detection
// ---------------------------------------------------------------------------

void test_white_pawn_promotes_on_rank8(void) {
  TEST_ASSERT_TRUE(ChessRules::isPawnPromotion('P', 0));  // row 0 = rank 8
}

void test_white_pawn_no_promote_other_rank(void) {
  TEST_ASSERT_FALSE(ChessRules::isPawnPromotion('P', 1)); // row 1 = rank 7
}

void test_black_pawn_promotes_on_rank1(void) {
  TEST_ASSERT_TRUE(ChessRules::isPawnPromotion('p', 7));  // row 7 = rank 1
}

void test_non_pawn_no_promote(void) {
  TEST_ASSERT_FALSE(ChessRules::isPawnPromotion('R', 0));
}

// ---------------------------------------------------------------------------
// Helpers: isEnPassantMove, isCastlingMove, etc.
// ---------------------------------------------------------------------------

void test_isEnPassantMove_true(void) {
  // Pawn moves diagonally to empty square → en passant
  TEST_ASSERT_TRUE(ChessUtils::isEnPassantMove(4, 4, 3, 3, 'P', ' '));
}

void test_isEnPassantMove_false_normal_capture(void) {
  // Pawn captures a piece diagonally — not en passant
  TEST_ASSERT_FALSE(ChessUtils::isEnPassantMove(4, 4, 3, 3, 'P', 'p'));
}

void test_isCastlingMove_true(void) {
  // King moves 2 squares
  TEST_ASSERT_TRUE(ChessUtils::isCastlingMove(7, 4, 7, 6, 'K'));
  TEST_ASSERT_TRUE(ChessUtils::isCastlingMove(7, 4, 7, 2, 'K'));
}

void test_isCastlingMove_false(void) {
  // King moves 1 square
  TEST_ASSERT_FALSE(ChessUtils::isCastlingMove(7, 4, 7, 5, 'K'));
}

void test_getEnPassantCapturedPawnRow(void) {
  // White captures en passant moving to row 2 (rank 6) — captured pawn is on row 3
  TEST_ASSERT_EQUAL_INT(3, ChessUtils::getEnPassantCapturedPawnRow(2, 'P'));
  // Black captures en passant moving to row 5 (rank 3) — captured pawn is on row 4
  TEST_ASSERT_EQUAL_INT(4, ChessUtils::getEnPassantCapturedPawnRow(5, 'p'));
}

void register_rules_special_tests() {
  needsDefaultKings = true;

  // Castling
  RUN_TEST(test_white_kingside_castle_available);
  RUN_TEST(test_white_queenside_castle_available);
  RUN_TEST(test_black_kingside_castle_available);
  RUN_TEST(test_castle_blocked_by_piece);
  RUN_TEST(test_castle_through_check_forbidden);
  RUN_TEST(test_castle_while_in_check_forbidden);
  RUN_TEST(test_no_castle_right_revoked);

  // En passant
  RUN_TEST(test_en_passant_white_captures);
  RUN_TEST(test_en_passant_black_captures);
  RUN_TEST(test_en_passant_not_available);

  // Promotion
  RUN_TEST(test_white_pawn_promotes_on_rank8);
  RUN_TEST(test_white_pawn_no_promote_other_rank);
  RUN_TEST(test_black_pawn_promotes_on_rank1);
  RUN_TEST(test_non_pawn_no_promote);

  // Helpers
  RUN_TEST(test_isEnPassantMove_true);
  RUN_TEST(test_isEnPassantMove_false_normal_capture);
  RUN_TEST(test_isCastlingMove_true);
  RUN_TEST(test_isCastlingMove_false);
  RUN_TEST(test_getEnPassantCapturedPawnRow);
}
