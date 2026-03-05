#include <unity.h>

#include "../test_helpers.h"
#include <chess_history.h>
#include <types.h>

// Shared globals from test_core.cpp
extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// Shared state
// ---------------------------------------------------------------------------

static ChessHistory hist;

// ---------------------------------------------------------------------------
// Standalone ChessHistory (move log) tests
// ---------------------------------------------------------------------------

void test_history_initial_empty(void) {
  ChessHistory h;
  TEST_ASSERT_EQUAL(0, h.moveCount());
  TEST_ASSERT_TRUE(h.empty());
  TEST_ASSERT_FALSE(h.canUndo());
  TEST_ASSERT_FALSE(h.canRedo());
  TEST_ASSERT_EQUAL(-1, h.currentMoveIndex());
}

void test_history_clear(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));
  TEST_ASSERT_EQUAL(1, hist.moveCount());

  hist.clear();
  TEST_ASSERT_EQUAL(0, hist.moveCount());
  TEST_ASSERT_TRUE(hist.empty());
  TEST_ASSERT_EQUAL(-1, hist.currentMoveIndex());
}

void test_history_add_and_get_move(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));

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
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));
  hist.addMove(makeEntry(1, 4, 3, 4, 'p'));

  TEST_ASSERT_EQUAL(2, hist.moveCount());
  const MoveEntry& last = hist.lastMove();
  TEST_ASSERT_EQUAL(1, last.fromRow);
  TEST_ASSERT_EQUAL('p', last.piece);
}

void test_history_undo_move(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 0, 4, 0, 'P'));

  TEST_ASSERT_TRUE(hist.canUndo());
  const MoveEntry* undone = hist.undoMove();
  TEST_ASSERT_NOT_NULL(undone);
  TEST_ASSERT_EQUAL(6, undone->fromRow);
  TEST_ASSERT_EQUAL('P', undone->piece);
  TEST_ASSERT_EQUAL(-1, hist.currentMoveIndex());
  TEST_ASSERT_FALSE(hist.canUndo());
  // Move is still in the log, cursor stepped back
  TEST_ASSERT_EQUAL(1, hist.moveCount());
}

void test_history_undo_empty(void) {
  hist = ChessHistory();
  const MoveEntry* result = hist.undoMove();
  TEST_ASSERT_NULL(result);
}

void test_history_redo_after_undo(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));
  hist.undoMove();

  TEST_ASSERT_TRUE(hist.canRedo());
  const MoveEntry* redone = hist.redoMove();
  TEST_ASSERT_NOT_NULL(redone);
  TEST_ASSERT_EQUAL(6, redone->fromRow);
  TEST_ASSERT_EQUAL('P', redone->piece);
  TEST_ASSERT_EQUAL(0, hist.currentMoveIndex());
  TEST_ASSERT_FALSE(hist.canRedo());
}

void test_history_redo_at_end(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));
  // No undo — already at end
  const MoveEntry* result = hist.redoMove();
  TEST_ASSERT_NULL(result);
}

void test_history_undo_redo_sequence(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));   // move 0
  hist.addMove(makeEntry(1, 4, 3, 4, 'p'));   // move 1
  hist.addMove(makeEntry(6, 3, 4, 3, 'P'));   // move 2

  TEST_ASSERT_EQUAL(2, hist.currentMoveIndex());

  // Undo twice
  hist.undoMove();  // cursor → 1
  TEST_ASSERT_EQUAL(1, hist.currentMoveIndex());
  hist.undoMove();  // cursor → 0
  TEST_ASSERT_EQUAL(0, hist.currentMoveIndex());

  // Redo once
  hist.redoMove();  // cursor → 1
  TEST_ASSERT_EQUAL(1, hist.currentMoveIndex());
  TEST_ASSERT_EQUAL('p', hist.lastMove().piece);
}

void test_history_branch_wipes_future(void) {
  hist = ChessHistory();
  hist.addMove(makeEntry(6, 4, 4, 4, 'P'));   // move 0
  hist.addMove(makeEntry(1, 4, 3, 4, 'p'));   // move 1

  hist.undoMove();  // cursor → 0
  // Add a different move — should wipe move 1
  hist.addMove(makeEntry(1, 3, 3, 3, 'p'));   // new move 1
  TEST_ASSERT_EQUAL(2, hist.moveCount());
  TEST_ASSERT_EQUAL(3, hist.getMove(1).toCol);  // d5, not e5
  TEST_ASSERT_FALSE(hist.canRedo());
}

void test_history_multiple_moves(void) {
  hist = ChessHistory();
  for (int i = 0; i < 10; i++) {
    hist.addMove(makeEntry(i % 8, i % 8, (i + 1) % 8, (i + 1) % 8, 'N'));
  }
  TEST_ASSERT_EQUAL(10, hist.moveCount());
  TEST_ASSERT_EQUAL(0, hist.getMove(0).fromRow);
  TEST_ASSERT_EQUAL(1, hist.getMove(9).fromRow);  // 9 % 8 = 1
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_history_tests() {
  needsDefaultKings = false;

  // Standalone ChessHistory (move log + undo/redo)
  RUN_TEST(test_history_initial_empty);
  RUN_TEST(test_history_clear);
  RUN_TEST(test_history_add_and_get_move);
  RUN_TEST(test_history_last_move);
  RUN_TEST(test_history_undo_move);
  RUN_TEST(test_history_undo_empty);
  RUN_TEST(test_history_redo_after_undo);
  RUN_TEST(test_history_redo_at_end);
  RUN_TEST(test_history_undo_redo_sequence);
  RUN_TEST(test_history_branch_wipes_future);
  RUN_TEST(test_history_multiple_moves);
}
