#include <unity.h>

#include "../test_helpers.h"
#include <history.h>
#include <board.h>

// Shared globals from test_core.cpp
extern char board[8][8];
extern bool needsDefaultKings;

static ChessHistory hist;
static ChessBoard brd;

// ---------------------------------------------------------------------------
// Standalone ChessHistory tests
// ---------------------------------------------------------------------------

void test_history_initial_empty(void) {
  ChessHistory h;
  TEST_ASSERT_EQUAL(0, h.moveCount());
  TEST_ASSERT_TRUE(h.empty());
  TEST_ASSERT_EQUAL(0, h.positionCount());
}

void test_history_clear(void) {
  hist = ChessHistory();
  MoveEntry e = {};
  e.fromRow = 6; e.fromCol = 4; e.toRow = 4; e.toCol = 4;
  e.piece = 'P'; e.captured = ' '; e.promotion = ' ';
  hist.addMove(e);
  hist.recordPosition(0x1234);
  TEST_ASSERT_EQUAL(1, hist.moveCount());
  TEST_ASSERT_EQUAL(1, hist.positionCount());

  hist.clear();
  TEST_ASSERT_EQUAL(0, hist.moveCount());
  TEST_ASSERT_TRUE(hist.empty());
  TEST_ASSERT_EQUAL(0, hist.positionCount());
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

void test_history_position_record(void) {
  hist = ChessHistory();
  hist.recordPosition(0xAABBCCDD);
  hist.recordPosition(0x11223344);
  TEST_ASSERT_EQUAL(2, hist.positionCount());
}

void test_history_threefold_repetition(void) {
  hist = ChessHistory();
  // A-B-A-B-A pattern (5 entries, 3 occurrences of A on same side-to-move)
  hist.recordPosition(0x1111);  // pos 0: A
  hist.recordPosition(0x2222);  // pos 1: B
  hist.recordPosition(0x1111);  // pos 2: A
  hist.recordPosition(0x2222);  // pos 3: B
  hist.recordPosition(0x1111);  // pos 4: A (3rd time)
  TEST_ASSERT_TRUE(hist.isThreefoldRepetition());
}

void test_history_threefold_not_enough(void) {
  hist = ChessHistory();
  // Only 2 occurrences of A
  hist.recordPosition(0x1111);  // A
  hist.recordPosition(0x2222);  // B
  hist.recordPosition(0x1111);  // A (2nd time)
  TEST_ASSERT_FALSE(hist.isThreefoldRepetition());
}

void test_history_threefold_needs_five_entries(void) {
  hist = ChessHistory();
  // Only 4 entries — can't have 3 at indices 0,2,4
  hist.recordPosition(0x1111);
  hist.recordPosition(0x2222);
  hist.recordPosition(0x1111);
  hist.recordPosition(0x2222);
  TEST_ASSERT_FALSE(hist.isThreefoldRepetition());
}

void test_history_reset_position_history(void) {
  hist = ChessHistory();
  hist.recordPosition(0x1111);
  hist.recordPosition(0x2222);
  TEST_ASSERT_EQUAL(2, hist.positionCount());

  hist.resetPositionHistory();
  TEST_ASSERT_EQUAL(0, hist.positionCount());
  TEST_ASSERT_FALSE(hist.isThreefoldRepetition());
}

void test_history_clear_preserves_independence(void) {
  // Adding moves doesn't affect position count and vice versa
  hist = ChessHistory();
  MoveEntry e = {};
  e.piece = 'N'; e.captured = ' '; e.promotion = ' ';
  hist.addMove(e);
  hist.recordPosition(0x5555);

  TEST_ASSERT_EQUAL(1, hist.moveCount());
  TEST_ASSERT_EQUAL(1, hist.positionCount());

  // Clear only resets both together
  hist.clear();
  TEST_ASSERT_EQUAL(0, hist.moveCount());
  TEST_ASSERT_EQUAL(0, hist.positionCount());
}

// ---------------------------------------------------------------------------
// ChessBoard integration: history records moves correctly
// ---------------------------------------------------------------------------

static void setUpBoard(void) {
  brd = ChessBoard();
  brd.newGame();
}

void test_board_history_records_moves(void) {
  setUpBoard();
  // 1. e4
  brd.makeMove(6, 4, 4, 4);
  TEST_ASSERT_EQUAL(1, brd.history().moveCount());

  // 1... e5
  brd.makeMove(1, 4, 3, 4);
  TEST_ASSERT_EQUAL(2, brd.history().moveCount());
}

void test_board_history_correct_fields(void) {
  setUpBoard();
  // 1. e4 (pawn from e2 to e4)
  brd.makeMove(6, 4, 4, 4);
  const MoveEntry& m = brd.history().getMove(0);
  TEST_ASSERT_EQUAL(6, m.fromRow);
  TEST_ASSERT_EQUAL(4, m.fromCol);
  TEST_ASSERT_EQUAL(4, m.toRow);
  TEST_ASSERT_EQUAL(4, m.toCol);
  TEST_ASSERT_EQUAL('P', m.piece);
  TEST_ASSERT_EQUAL(' ', m.captured);
  TEST_ASSERT_EQUAL(' ', m.promotion);
  TEST_ASSERT_FALSE(m.isCapture);
  TEST_ASSERT_FALSE(m.isEnPassant);
  TEST_ASSERT_FALSE(m.isCastling);
  TEST_ASSERT_FALSE(m.isPromotion);
}

void test_board_history_capture_recorded(void) {
  setUpBoard();
  // Set up a simple capture: 1. e4 d5 2. exd5
  brd.makeMove(6, 4, 4, 4);  // e4
  brd.makeMove(1, 3, 3, 3);  // d5
  brd.makeMove(4, 4, 3, 3);  // exd5 (capture)

  const MoveEntry& m = brd.history().getMove(2);
  TEST_ASSERT_TRUE(m.isCapture);
  TEST_ASSERT_EQUAL('p', m.captured);  // captured black pawn
  TEST_ASSERT_EQUAL('P', m.piece);     // white pawn captured
}

void test_board_history_en_passant_recorded(void) {
  setUpBoard();
  // Set up en passant: 1. e4 a6 2. e5 d5 3. exd6 (en passant)
  brd.makeMove(6, 4, 4, 4);  // e4
  brd.makeMove(1, 0, 2, 0);  // a6
  brd.makeMove(4, 4, 3, 4);  // e5
  brd.makeMove(1, 3, 3, 3);  // d5
  brd.makeMove(3, 4, 2, 3);  // exd6 (en passant)

  const MoveEntry& m = brd.history().getMove(4);
  TEST_ASSERT_TRUE(m.isEnPassant);
  TEST_ASSERT_TRUE(m.isCapture);
  TEST_ASSERT_EQUAL('p', m.captured);  // captured black pawn
  TEST_ASSERT_EQUAL(3, m.epCapturedRow);
}

void test_board_history_castling_recorded(void) {
  setUpBoard();
  // Clear path for white kingside castling
  brd.loadFEN("r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
  // Move bishop out: Bb5
  brd.makeMove(7, 5, 3, 1);  // Bc4 or similar... let me use a simpler FEN

  // Direct setup: empty between king and rook
  brd.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  brd.makeMove(7, 4, 7, 6);  // O-O (white kingside)

  const MoveEntry& m = brd.history().lastMove();
  TEST_ASSERT_TRUE(m.isCastling);
  TEST_ASSERT_EQUAL('K', m.piece);
  TEST_ASSERT_FALSE(m.isCapture);
}

void test_board_history_promotion_recorded(void) {
  // White pawn on 7th rank promotes
  brd = ChessBoard();
  brd.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  brd.makeMove(1, 0, 0, 0, 'q');  // a8=Q

  const MoveEntry& m = brd.history().lastMove();
  TEST_ASSERT_TRUE(m.isPromotion);
  TEST_ASSERT_EQUAL('Q', m.promotion);
  TEST_ASSERT_EQUAL('P', m.piece);
}

void test_board_history_check_recorded(void) {
  // Set up a position where a move gives check
  brd = ChessBoard();
  brd.loadFEN("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
  // Qh5 gives check? No. Let's use a simpler setup.
  brd.loadFEN("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
  brd.makeMove(6, 4, 1, 4);  // Re8+ (check)

  const MoveEntry& m = brd.history().lastMove();
  TEST_ASSERT_TRUE(m.isCheck);
}

void test_board_history_new_game_clears(void) {
  brd = ChessBoard();
  brd.newGame();
  brd.makeMove(6, 4, 4, 4);
  TEST_ASSERT_EQUAL(1, brd.history().moveCount());

  brd.newGame();
  TEST_ASSERT_EQUAL(0, brd.history().moveCount());
  TEST_ASSERT_TRUE(brd.history().empty());
}

void test_board_history_load_fen_clears(void) {
  brd = ChessBoard();
  brd.newGame();
  brd.makeMove(6, 4, 4, 4);
  TEST_ASSERT_EQUAL(1, brd.history().moveCount());

  brd.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_EQUAL(0, brd.history().moveCount());
}

void test_board_history_prevstate_saved(void) {
  setUpBoard();
  // Before e4: castling=KQkq (0x0F), ep=none, halfmove=0, fullmove=1
  PositionState before = brd.positionState();
  brd.makeMove(6, 4, 4, 4);  // e4

  const MoveEntry& m = brd.history().getMove(0);
  TEST_ASSERT_EQUAL(before.castlingRights, m.prevState.castlingRights);
  TEST_ASSERT_EQUAL(before.halfmoveClock, m.prevState.halfmoveClock);
  TEST_ASSERT_EQUAL(before.fullmoveClock, m.prevState.fullmoveClock);
}

void test_board_history_invalid_move_not_recorded(void) {
  setUpBoard();
  // Try an illegal move
  MoveResult r = brd.makeMove(6, 4, 3, 4);  // e2-e5 (illegal: 3 squares)
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_EQUAL(0, brd.history().moveCount());
}

void test_board_history_last_move_after_sequence(void) {
  setUpBoard();
  brd.makeMove(6, 4, 4, 4);  // 1. e4
  brd.makeMove(1, 4, 3, 4);  // 1... e5
  brd.makeMove(7, 6, 5, 5);  // 2. Nf3

  TEST_ASSERT_EQUAL(3, brd.history().moveCount());
  const MoveEntry& last = brd.history().lastMove();
  TEST_ASSERT_EQUAL('N', last.piece);
  TEST_ASSERT_EQUAL(7, last.fromRow);
  TEST_ASSERT_EQUAL(6, last.fromCol);
  TEST_ASSERT_EQUAL(5, last.toRow);
  TEST_ASSERT_EQUAL(5, last.toCol);
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
  RUN_TEST(test_history_position_record);
  RUN_TEST(test_history_threefold_repetition);
  RUN_TEST(test_history_threefold_not_enough);
  RUN_TEST(test_history_threefold_needs_five_entries);
  RUN_TEST(test_history_reset_position_history);
  RUN_TEST(test_history_clear_preserves_independence);

  // Board integration
  RUN_TEST(test_board_history_records_moves);
  RUN_TEST(test_board_history_correct_fields);
  RUN_TEST(test_board_history_capture_recorded);
  RUN_TEST(test_board_history_en_passant_recorded);
  RUN_TEST(test_board_history_castling_recorded);
  RUN_TEST(test_board_history_promotion_recorded);
  RUN_TEST(test_board_history_check_recorded);
  RUN_TEST(test_board_history_new_game_clears);
  RUN_TEST(test_board_history_load_fen_clears);
  RUN_TEST(test_board_history_prevstate_saved);
  RUN_TEST(test_board_history_invalid_move_not_recorded);
  RUN_TEST(test_board_history_last_move_after_sequence);
}
