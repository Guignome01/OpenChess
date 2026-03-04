#include <unity.h>

#include "../test_helpers.h"
#include <history.h>

// Shared globals from test_core.cpp
extern char board[8][8];
extern bool needsDefaultKings;

static ChessHistory hist;

// ---------------------------------------------------------------------------
// Standalone ChessHistory tests
// ---------------------------------------------------------------------------

void test_history_initial_empty(void) {
  ChessHistory h;
  TEST_ASSERT_EQUAL(0, h.moveCount());
  TEST_ASSERT_TRUE(h.empty());
}

void test_history_clear(void) {
  hist = ChessHistory();
  MoveEntry e = {};
  e.fromRow = 6; e.fromCol = 4; e.toRow = 4; e.toCol = 4;
  e.piece = 'P'; e.captured = ' '; e.promotion = ' ';
  hist.addMove(e);
  TEST_ASSERT_EQUAL(1, hist.moveCount());

  hist.clear();
  TEST_ASSERT_EQUAL(0, hist.moveCount());
  TEST_ASSERT_TRUE(hist.empty());
}

void test_history_add_and_get_move(void) {
  hist = ChessHistory();
  MoveEntry e = {};
  e.fromRow = 6; e.fromCol = 4; e.toRow = 4; e.toCol = 4;
  e.piece = 'P'; e.captured = ' '; e.promotion = ' ';
  e.isCapture = false; e.isEnPassant = false; e.isCastling = false;
  e.isPromotion = false; e.isCheck = false; e.epCapturedRow = -1;
  hist.addMove(e);

  TEST_ASSERT_EQUAL(1, hist.moveCount());
  TEST_ASSERT_FALSE(hist.empty());
  const MoveEntry& got = hist.getMove(0);
  TEST_ASSERT_EQUAL(6, got.fromRow);
  TEST_ASSERT_EQUAL(4, got.fromCol);
  TEST_ASSERT_EQUAL(4, got.toRow);
  TEST_ASSERT_EQUAL(4, got.toCol);
  TEST_ASSERT_EQUAL('P', got.piece);
  TEST_ASSERT_EQUAL(' ', got.captured);
}

void test_history_last_move(void) {
  hist = ChessHistory();
  MoveEntry e1 = {};
  e1.fromRow = 6; e1.fromCol = 4; e1.toRow = 4; e1.toCol = 4;
  e1.piece = 'P'; e1.captured = ' '; e1.promotion = ' ';
  hist.addMove(e1);

  MoveEntry e2 = {};
  e2.fromRow = 1; e2.fromCol = 4; e2.toRow = 3; e2.toCol = 4;
  e2.piece = 'p'; e2.captured = ' '; e2.promotion = ' ';
  hist.addMove(e2);

  TEST_ASSERT_EQUAL(2, hist.moveCount());
  const MoveEntry& last = hist.lastMove();
  TEST_ASSERT_EQUAL(1, last.fromRow);
  TEST_ASSERT_EQUAL('p', last.piece);
}

void test_history_pop_move(void) {
  hist = ChessHistory();
  MoveEntry e = {};
  e.fromRow = 6; e.fromCol = 0; e.toRow = 4; e.toCol = 0;
  e.piece = 'P'; e.captured = ' '; e.promotion = ' ';
  hist.addMove(e);

  MoveEntry popped = {};
  TEST_ASSERT_TRUE(hist.popMove(popped));
  TEST_ASSERT_EQUAL(6, popped.fromRow);
  TEST_ASSERT_EQUAL('P', popped.piece);
  TEST_ASSERT_EQUAL(0, hist.moveCount());
  TEST_ASSERT_TRUE(hist.empty());
}

void test_history_pop_empty(void) {
  hist = ChessHistory();
  MoveEntry popped = {};
  TEST_ASSERT_FALSE(hist.popMove(popped));
}

void test_history_multiple_moves(void) {
  hist = ChessHistory();
  for (int i = 0; i < 10; i++) {
    MoveEntry e = {};
    e.fromRow = i % 8;
    e.fromCol = i % 8;
    e.toRow = (i + 1) % 8;
    e.toCol = (i + 1) % 8;
    e.piece = 'N';
    e.captured = ' ';
    e.promotion = ' ';
    hist.addMove(e);
  }
  TEST_ASSERT_EQUAL(10, hist.moveCount());
  TEST_ASSERT_EQUAL(0, hist.getMove(0).fromRow);
  TEST_ASSERT_EQUAL(1, hist.getMove(9).fromRow);  // 9 % 8 = 1
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_history_tests() {
  needsDefaultKings = false;

  // Standalone ChessHistory
  RUN_TEST(test_history_initial_empty);
  RUN_TEST(test_history_clear);
  RUN_TEST(test_history_add_and_get_move);
  RUN_TEST(test_history_last_move);
  RUN_TEST(test_history_pop_move);
  RUN_TEST(test_history_pop_empty);
  RUN_TEST(test_history_multiple_moves);
}
