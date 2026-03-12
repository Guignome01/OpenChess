#include <unity.h>

#include <chess_iterator.h>

#include "../test_helpers.h"

extern Piece board[8][8];

// ── forEachSquare ────────────────────────────────────────────────────────────

static void test_forEachSquare_visits_all_64(void) {
  int count = 0;
  ChessIterator::forEachSquare(board, [&](int r, int c, Piece) { ++count; });
  TEST_ASSERT_EQUAL(64, count);
}

static void test_forEachSquare_visits_occupied_and_empty(void) {
  placePiece(board, Piece::W_KING, "e1");
  placePiece(board, Piece::B_KING, "e8");

  int occupied = 0, empty = 0;
  ChessIterator::forEachSquare(board, [&](int, int, Piece p) {
    if (p != Piece::NONE)
      ++occupied;
    else
      ++empty;
  });
  TEST_ASSERT_EQUAL(2, occupied);
  TEST_ASSERT_EQUAL(62, empty);
}

// ── forEachPiece ─────────────────────────────────────────────────────────────

static void test_forEachPiece_empty_board(void) {
  int count = 0;
  ChessIterator::forEachPiece(board, [&](int, int, Piece) { ++count; });
  TEST_ASSERT_EQUAL(0, count);
}

static void test_forEachPiece_skips_empty_squares(void) {
  placePiece(board, Piece::W_ROOK, "a1");
  placePiece(board, Piece::B_KNIGHT, "c6");
  placePiece(board, Piece::W_PAWN, "d4");

  int count = 0;
  ChessIterator::forEachPiece(board, [&](int, int, Piece) { ++count; });
  TEST_ASSERT_EQUAL(3, count);
}

static void test_forEachPiece_initial_position(void) {
  setupInitialBoard(board);

  int count = 0;
  ChessIterator::forEachPiece(board, [&](int, int, Piece) { ++count; });
  TEST_ASSERT_EQUAL(32, count);
}

// ── somePiece ────────────────────────────────────────────────────────────────

static void test_somePiece_returns_false_empty_board(void) {
  bool found = ChessIterator::somePiece(board, [](int, int, Piece) {
    return true;  // would match anything — but board is empty
  });
  TEST_ASSERT_FALSE(found);
}

static void test_somePiece_finds_matching_piece(void) {
  placePiece(board, Piece::W_QUEEN, "d1");

  bool found = ChessIterator::somePiece(
      board, [](int, int, Piece p) { return p == Piece::W_QUEEN; });
  TEST_ASSERT_TRUE(found);
}

static void test_somePiece_no_match(void) {
  placePiece(board, Piece::W_KING, "e1");

  bool found = ChessIterator::somePiece(
      board, [](int, int, Piece p) { return p == Piece::W_QUEEN; });
  TEST_ASSERT_FALSE(found);
}

static void test_somePiece_stops_early(void) {
  placePiece(board, Piece::W_PAWN, "a2");
  placePiece(board, Piece::W_PAWN, "b2");
  placePiece(board, Piece::W_PAWN, "c2");

  int visited = 0;
  ChessIterator::somePiece(board, [&](int, int, Piece) {
    ++visited;
    return true;  // stop on first piece
  });
  TEST_ASSERT_EQUAL(1, visited);
}

// ── findPiece ────────────────────────────────────────────────────────────────

static void test_findPiece_not_found(void) {
  int positions[2][2];
  int count = ChessIterator::findPiece(board, Piece::W_KING, positions, 2);
  TEST_ASSERT_EQUAL(0, count);
}

static void test_findPiece_single(void) {
  placePiece(board, Piece::W_KING, "e1");

  int positions[2][2];
  int count = ChessIterator::findPiece(board, Piece::W_KING, positions, 2);
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL(7, positions[0][0]);  // row 7 = rank 1
  TEST_ASSERT_EQUAL(4, positions[0][1]);  // col 4 = file e
}

static void test_findPiece_multiple(void) {
  placePiece(board, Piece::W_ROOK, "a1");
  placePiece(board, Piece::W_ROOK, "h1");

  int positions[4][2];
  int count = ChessIterator::findPiece(board, Piece::W_ROOK, positions, 4);
  TEST_ASSERT_EQUAL(2, count);
}

static void test_findPiece_respects_max(void) {
  placePiece(board, Piece::W_PAWN, "a2");
  placePiece(board, Piece::W_PAWN, "b2");
  placePiece(board, Piece::W_PAWN, "c2");
  placePiece(board, Piece::W_PAWN, "d2");

  int positions[2][2];
  int count = ChessIterator::findPiece(board, Piece::W_PAWN, positions, 2);
  TEST_ASSERT_EQUAL(2, count);
}

static void test_findPiece_distinguishes_color(void) {
  placePiece(board, Piece::B_ROOK, "a8");  // black rook
  placePiece(board, Piece::W_ROOK, "a1");  // white rook

  int positions[2][2];
  int countWhite = ChessIterator::findPiece(board, Piece::W_ROOK, positions, 2);
  TEST_ASSERT_EQUAL(1, countWhite);

  int countBlack = ChessIterator::findPiece(board, Piece::B_ROOK, positions, 2);
  TEST_ASSERT_EQUAL(1, countBlack);
}

// ── Registration ─────────────────────────────────────────────────────────────

void register_chess_iterator_tests() {
  // forEachSquare
  RUN_TEST(test_forEachSquare_visits_all_64);
  RUN_TEST(test_forEachSquare_visits_occupied_and_empty);

  // forEachPiece
  RUN_TEST(test_forEachPiece_empty_board);
  RUN_TEST(test_forEachPiece_skips_empty_squares);
  RUN_TEST(test_forEachPiece_initial_position);

  // somePiece
  RUN_TEST(test_somePiece_returns_false_empty_board);
  RUN_TEST(test_somePiece_finds_matching_piece);
  RUN_TEST(test_somePiece_no_match);
  RUN_TEST(test_somePiece_stops_early);

  // findPiece
  RUN_TEST(test_findPiece_not_found);
  RUN_TEST(test_findPiece_single);
  RUN_TEST(test_findPiece_multiple);
  RUN_TEST(test_findPiece_respects_max);
  RUN_TEST(test_findPiece_distinguishes_color);
}
