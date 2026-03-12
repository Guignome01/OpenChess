#include <unity.h>
#include <chess_iterator.h>

#include "../test_helpers.h"

extern Piece board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// Pawn moves
// ---------------------------------------------------------------------------

void test_white_pawn_single_push(void) {
  placePiece(board, Piece::W_PAWN, "e2");
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r - 1, c)); // e3
}

void test_white_pawn_double_push_from_start(void) {
  placePiece(board, Piece::W_PAWN, "e2");
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r - 2, c)); // e4
}

void test_white_pawn_no_double_push_from_rank3(void) {
  placePiece(board, Piece::W_PAWN, "e3");
  int r, c;
  sq("e3", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 2, c)); // e5 double push
}

void test_white_pawn_blocked(void) {
  placePiece(board, Piece::W_PAWN, "e4");
  placePiece(board, Piece::B_PAWN, "e5"); // blocker
  int r, c;
  sq("e4", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 1, c));
}

void test_white_pawn_double_push_blocked_on_first_sq(void) {
  placePiece(board, Piece::W_PAWN, "e2");
  placePiece(board, Piece::B_PAWN, "e3"); // block first square
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 2, c)); // e4
}

void test_white_pawn_capture_diagonal(void) {
  placePiece(board, Piece::W_PAWN, "d4");
  placePiece(board, Piece::B_PAWN, "e5"); // capturable
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e5", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
}

void test_white_pawn_no_capture_own_piece(void) {
  placePiece(board, Piece::W_PAWN, "d4");
  placePiece(board, Piece::W_KNIGHT, "e5"); // own piece
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e5", tr, tc);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr, tc));
}

void test_black_pawn_single_push(void) {
  placePiece(board, Piece::B_PAWN, "e7");
  int r, c;
  sq("e7", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r + 1, c)); // e6
}

void test_black_pawn_double_push_from_start(void) {
  placePiece(board, Piece::B_PAWN, "e7");
  int r, c;
  sq("e7", r, c);
  TEST_ASSERT_TRUE(moveExists(board, r, c, r + 2, c)); // e5
}

// ---------------------------------------------------------------------------
// Knight moves
// ---------------------------------------------------------------------------

void test_knight_center_moves(void) {
  placePiece(board, Piece::W_KNIGHT, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(8, moveCount); // Knight in center has 8 moves
}

void test_knight_corner_moves(void) {
  placePiece(board, Piece::W_KNIGHT, "a1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(2, moveCount); // Corner knight has 2 moves
}

void test_knight_captures_enemy(void) {
  placePiece(board, Piece::W_KNIGHT, "d4");
  placePiece(board, Piece::B_PAWN, "e6"); // enemy on one of the squares
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("e6", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
}

void test_knight_blocked_by_own(void) {
  placePiece(board, Piece::W_KNIGHT, "d4");
  placePiece(board, Piece::W_PAWN, "e6"); // own piece on target
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
  placePiece(board, Piece::W_BISHOP, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(13, moveCount); // d4 bishop on empty board
}

void test_bishop_blocked_by_own(void) {
  placePiece(board, Piece::W_BISHOP, "c1");
  placePiece(board, Piece::W_PAWN, "d2"); // own piece blocks
  placePiece(board, Piece::W_PAWN, "b2"); // own piece blocks
  int r, c;
  sq("c1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

void test_bishop_captures_and_stops(void) {
  placePiece(board, Piece::W_BISHOP, "a1");
  placePiece(board, Piece::B_PAWN, "d4"); // enemy blocks diagonal
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
  placePiece(board, Piece::W_ROOK, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(14, moveCount); // Rook in center on empty board
}

void test_rook_blocked_by_own(void) {
  placePiece(board, Piece::W_ROOK, "a1");
  placePiece(board, Piece::W_PAWN, "a2"); // above
  placePiece(board, Piece::W_PAWN, "b1"); // right
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
  placePiece(board, Piece::W_QUEEN, "d4");
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
  placePiece(board, Piece::W_KING, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(8, moveCount); // King in center has 8 moves
}

void test_king_corner_moves(void) {
  placePiece(board, Piece::W_KING, "a1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(3, moveCount); // Corner king has 3 moves
}

void test_king_no_move_onto_own(void) {
  placePiece(board, Piece::W_KING, "a1");
  placePiece(board, Piece::W_PAWN, "a2");
  placePiece(board, Piece::W_PAWN, "b2");
  placePiece(board, Piece::W_PAWN, "b1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

// ---------------------------------------------------------------------------
// Black piece moves (verify lowercase piece handling)
// ---------------------------------------------------------------------------

void test_black_pawn_diagonal_capture(void) {
  placePiece(board, Piece::B_PAWN, "d5");
  placePiece(board, Piece::W_PAWN, "e4"); // white piece to capture
  int r, c;
  sq("d5", r, c);
  int tr, tc;
  sq("e4", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
}

void test_black_knight_center(void) {
  clearBoard(board);
  placePiece(board, Piece::B_KING, "a8");
  placePiece(board, Piece::W_KING, "a1");
  placePiece(board, Piece::B_KNIGHT, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(8, moveCount);
}

void test_black_rook_center(void) {
  clearBoard(board);
  placePiece(board, Piece::B_KING, "a8");
  placePiece(board, Piece::W_KING, "a1");
  placePiece(board, Piece::B_ROOK, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(14, moveCount);
}

void test_black_bishop_center(void) {
  clearBoard(board);
  placePiece(board, Piece::B_KING, "a8");
  placePiece(board, Piece::W_KING, "a1");
  placePiece(board, Piece::B_BISHOP, "d4");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(13, moveCount);
}

void test_rook_capture_and_stop(void) {
  placePiece(board, Piece::W_ROOK, "d4");
  placePiece(board, Piece::B_PAWN, "d7"); // enemy rook captures d7
  int r, c;
  sq("d4", r, c);
  int tr, tc;
  sq("d7", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
  // Cannot continue past d7 to d8
  int tr2, tc2;
  sq("d8", tr2, tc2);
  TEST_ASSERT_FALSE(moveExists(board, r, c, tr2, tc2));
}

void test_white_pawn_double_push_blocked_second_sq(void) {
  placePiece(board, Piece::W_PAWN, "e2");
  // e3 is empty, e4 is blocked
  placePiece(board, Piece::B_PAWN, "e4");
  int r, c;
  sq("e2", r, c);
  TEST_ASSERT_FALSE(moveExists(board, r, c, r - 2, c)); // e4 blocked
  TEST_ASSERT_TRUE(moveExists(board, r, c, r - 1, c));  // e3 still ok
}

// ---------------------------------------------------------------------------
// Edge/boundary positions
// ---------------------------------------------------------------------------

void test_pawn_a_file_capture(void) {
  placePiece(board, Piece::W_PAWN, "a4");
  placePiece(board, Piece::B_PAWN, "b5"); // can capture right
  int r, c;
  sq("a4", r, c);
  int tr, tc;
  sq("b5", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
  // No left capture possible (off-board)
  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  // Should have: a5 (push) + b5 (capture) = 2
  TEST_ASSERT_EQUAL_INT(2, moveCount);
}

void test_pawn_h_file_capture(void) {
  placePiece(board, Piece::W_PAWN, "h4");
  placePiece(board, Piece::B_PAWN, "g5"); // can capture left
  int r, c;
  sq("h4", r, c);
  int tr, tc;
  sq("g5", tr, tc);
  TEST_ASSERT_TRUE(moveExists(board, r, c, tr, tc));
  // Should have: h5 (push) + g5 (capture) = 2
  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(2, moveCount);
}

void test_knight_edge_b1(void) {
  placePiece(board, Piece::W_KNIGHT, "b1");
  int r, c;
  sq("b1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(3, moveCount); // a3, c3, d2
}

void test_queen_blocked(void) {
  placePiece(board, Piece::W_QUEEN, "d4");
  // Surround with own pieces on all 8 directions
  placePiece(board, Piece::W_PAWN, "d5");
  placePiece(board, Piece::W_PAWN, "d3");
  placePiece(board, Piece::W_PAWN, "c4");
  placePiece(board, Piece::W_PAWN, "e4");
  placePiece(board, Piece::W_PAWN, "c5");
  placePiece(board, Piece::W_PAWN, "e5");
  placePiece(board, Piece::W_PAWN, "c3");
  placePiece(board, Piece::W_PAWN, "e3");
  int r, c;
  sq("d4", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(0, moveCount);
}

void test_bishop_corner_a1(void) {
  placePiece(board, Piece::W_BISHOP, "a1");
  int r, c;
  sq("a1", r, c);

  int moveCount = 0;
  int moves[28][2];
  ChessRules::getPossibleMoves(board, r, c, {}, moveCount, moves);
  TEST_ASSERT_EQUAL_INT(7, moveCount); // a1-h8 diagonal
}

// ---------------------------------------------------------------------------
// Initial position move counts
// ---------------------------------------------------------------------------

void test_initial_position_white_moves(void) {
  setupInitialBoard(board);
  PositionState flags{0x0F, -1, -1};

  // Each of the 8 pawns has 2 moves, each of 2 knights has 2 moves = 20 total
  int totalMoves = 0;
  ChessIterator::forEachPiece(board, [&](int row, int col, Piece piece) {
    if (!ChessPiece::isWhite(piece)) return;
    int moveCount = 0;
    int moves[28][2];
    ChessRules::getPossibleMoves(board, row, col, flags, moveCount, moves);
    totalMoves += moveCount;
  });
  TEST_ASSERT_EQUAL_INT(20, totalMoves);
}

void test_initial_position_black_moves(void) {
  setupInitialBoard(board);
  PositionState flags{0x0F, -1, -1};

  int totalMoves = 0;
  ChessIterator::forEachPiece(board, [&](int row, int col, Piece piece) {
    if (!ChessPiece::isBlack(piece)) return;
    int moveCount = 0;
    int moves[28][2];
    ChessRules::getPossibleMoves(board, row, col, flags, moveCount, moves);
    totalMoves += moveCount;
  });
  TEST_ASSERT_EQUAL_INT(20, totalMoves);
}

void register_chess_rules_moves_tests() {
  needsDefaultKings = true;

  // Pawns
  RUN_TEST(test_white_pawn_single_push);
  RUN_TEST(test_white_pawn_double_push_from_start);
  RUN_TEST(test_white_pawn_no_double_push_from_rank3);
  RUN_TEST(test_white_pawn_blocked);
  RUN_TEST(test_white_pawn_double_push_blocked_on_first_sq);
  RUN_TEST(test_white_pawn_double_push_blocked_second_sq);
  RUN_TEST(test_white_pawn_capture_diagonal);
  RUN_TEST(test_white_pawn_no_capture_own_piece);
  RUN_TEST(test_black_pawn_single_push);
  RUN_TEST(test_black_pawn_double_push_from_start);
  RUN_TEST(test_black_pawn_diagonal_capture);
  RUN_TEST(test_pawn_a_file_capture);
  RUN_TEST(test_pawn_h_file_capture);

  // Knights
  RUN_TEST(test_knight_center_moves);
  RUN_TEST(test_knight_corner_moves);
  RUN_TEST(test_knight_captures_enemy);
  RUN_TEST(test_knight_blocked_by_own);
  RUN_TEST(test_knight_edge_b1);

  // Bishops
  RUN_TEST(test_bishop_center_moves);
  RUN_TEST(test_bishop_blocked_by_own);
  RUN_TEST(test_bishop_captures_and_stops);
  RUN_TEST(test_bishop_corner_a1);

  // Rooks
  RUN_TEST(test_rook_center_moves);
  RUN_TEST(test_rook_blocked_by_own);
  RUN_TEST(test_rook_capture_and_stop);

  // Queen
  RUN_TEST(test_queen_center_moves);
  RUN_TEST(test_queen_blocked);

  // King
  RUN_TEST(test_king_center_moves);
  RUN_TEST(test_king_corner_moves);
  RUN_TEST(test_king_no_move_onto_own);

  // Black pieces
  RUN_TEST(test_black_knight_center);
  RUN_TEST(test_black_rook_center);
  RUN_TEST(test_black_bishop_center);

  // Initial position
  RUN_TEST(test_initial_position_white_moves);
  RUN_TEST(test_initial_position_black_moves);
}
