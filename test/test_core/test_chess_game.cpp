#include <unity.h>

#include "../test_helpers.h"
#include <chess_game.h>
#include <game_observer.h>
#include <types.h>

// Shared globals from test_core.cpp
extern char board[8][8];
extern bool needsDefaultKings;

static ChessGame gm;

// Reset ChessGame before every test
static void setUpGame(void) {
  gm = ChessGame();
  gm.newGame();
}

// ---------------------------------------------------------------------------
// Threefold repetition (detected inside ChessBoard::detectGameEnd)
// ---------------------------------------------------------------------------

void test_game_threefold_repetition(void) {
  setUpGame();
  // Position where Ke1 and Ke8 shuffle back and forth (pawns provide sufficient material)
  gm.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  // Move 1: Ke1-d1, Ke8-d8
  gm.makeMove(7, 4, 7, 3); // Ke1-d1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  // Move 2: Kd1-e1, Kd8-e8 (back to original — occurrence 2)
  gm.makeMove(7, 3, 7, 4); // Kd1-e1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8
  // Move 3: Ke1-d1, Ke8-d8
  gm.makeMove(7, 4, 7, 3); // Ke1-d1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  // Move 4: Kd1-e1, Kd8-e8 (back to original — occurrence 3)
  gm.makeMove(7, 3, 7, 4); // Kd1-e1
  MoveResult r = gm.makeMove(0, 3, 0, 4); // Kd8-e8 — third repetition
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(gm.isGameOver());
}

void test_game_threefold_different_castling_rights(void) {
  setUpGame();
  // Same as threefold test but with castling rights.
  // King move on first half-move loses castling → initial position hash
  // differs from subsequent "same board" hashes, so 8 half-moves is NOT
  // enough for threefold (only 2 occurrences of castling-less position).
  gm.loadFEN("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
  gm.makeMove(7, 4, 7, 3); // Ke1-d1 (loses K castling)
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 3, 7, 4); // Kd1-e1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8 — board same as start, but castling=0
  gm.makeMove(7, 4, 7, 3); // Ke1-d1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 3, 7, 4); // Kd1-e1
  MoveResult r = gm.makeMove(0, 3, 0, 4); // Kd8-e8 — occurrence 2 (not 3)
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isGameOver());
}

// ---------------------------------------------------------------------------
// isDraw (aggregate query — delegates to board for all conditions)
// ---------------------------------------------------------------------------

void test_game_is_draw_insufficient_material(void) {
  setUpGame();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_game_is_draw_fifty_move(void) {
  setUpGame();
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 100 50");
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_game_is_draw_false(void) {
  setUpGame();
  TEST_ASSERT_FALSE(gm.isDraw());
}

// ---------------------------------------------------------------------------
// isThreefoldRepetition (public — delegates to board)
// ---------------------------------------------------------------------------

void test_game_is_threefold_repetition(void) {
  setUpGame();
  // Repeat rook moves to produce threefold repetition (sufficient material)
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  gm.makeMove(7, 0, 7, 1); // Ra1-b1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 1, 7, 0); // Rb1-a1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8
  gm.makeMove(7, 0, 7, 1); // Ra1-b1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 1, 7, 0); // Rb1-a1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8
  // Position has now occurred 3 times
  TEST_ASSERT_TRUE(gm.isThreefoldRepetition());
}

void test_game_is_threefold_repetition_false(void) {
  setUpGame();
  TEST_ASSERT_FALSE(gm.isThreefoldRepetition());
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Observer notification / batching
// ---------------------------------------------------------------------------

// Local mock observer
class TestObserver : public IGameObserver {
 public:
  int callCount = 0;
  void onBoardStateChanged(const std::string&, float) override { callCount++; }
};

void test_game_observer_fires_on_move(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  obs.callCount = 0;
  g.makeMove(6, 4, 4, 4); // e2e4
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);
}

void test_game_observer_fires_on_new_game(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  obs.callCount = 0;
  g.newGame();
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);
}

void test_game_batch_suppresses_observer(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  obs.callCount = 0;

  g.beginBatch();
  g.makeMove(6, 4, 4, 4);  // e2e4
  g.makeMove(1, 4, 3, 4);  // e5
  TEST_ASSERT_EQUAL_INT(0, obs.callCount);
  g.endBatch();
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);  // single coalesced notification
}

void test_game_no_observer_no_crash(void) {
  setUpGame();
  // No observer set — ensure no crash
  gm.makeMove(6, 4, 4, 4); // should not crash
  TEST_ASSERT_TRUE(true);
}

void test_game_nested_batch(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  obs.callCount = 0;
  g.beginBatch();
  g.beginBatch();  // depth 2
  g.makeMove(6, 4, 4, 4);
  g.endBatch();    // depth 1 — observer still suppressed
  TEST_ASSERT_EQUAL_INT(0, obs.callCount);
  g.endBatch();    // depth 0 — fires coalesced notification
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);
}

void test_game_end_batch_without_begin(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  obs.callCount = 0;
  // endBatch with no beginBatch should not crash or fire observer
  g.endBatch();
  TEST_ASSERT_EQUAL_INT(0, obs.callCount);
}

// ---------------------------------------------------------------------------
// History integration (ChessGame owns history, records moves via makeMove)
// ---------------------------------------------------------------------------

void test_game_history_records_moves(void) {
  setUpGame();
  TEST_ASSERT_TRUE(gm.history().empty());
  gm.makeMove(6, 4, 4, 4);  // e4
  TEST_ASSERT_EQUAL(1, gm.history().moveCount());
  gm.makeMove(1, 4, 3, 4);  // e5
  TEST_ASSERT_EQUAL(2, gm.history().moveCount());
}

void test_game_history_correct_fields(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // e4

  const MoveEntry& m = gm.history().getMove(0);
  TEST_ASSERT_EQUAL(6, m.fromRow);
  TEST_ASSERT_EQUAL(4, m.fromCol);
  TEST_ASSERT_EQUAL(4, m.toRow);
  TEST_ASSERT_EQUAL(4, m.toCol);
  TEST_ASSERT_EQUAL('P', m.piece);
  TEST_ASSERT_EQUAL(' ', m.captured);
  TEST_ASSERT_FALSE(m.isCapture);
  TEST_ASSERT_FALSE(m.isEnPassant);
  TEST_ASSERT_FALSE(m.isCastling);
  TEST_ASSERT_FALSE(m.isPromotion);
}

void test_game_history_capture_recorded(void) {
  setUpGame();
  // Set up a simple capture: 1. e4 d5 2. exd5
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 3, 3, 3);  // d5
  gm.makeMove(4, 4, 3, 3);  // exd5 (capture)

  const MoveEntry& m = gm.history().getMove(2);
  TEST_ASSERT_TRUE(m.isCapture);
  TEST_ASSERT_EQUAL('p', m.captured);  // captured black pawn
  TEST_ASSERT_EQUAL('P', m.piece);     // white pawn captured
}

void test_game_history_en_passant_recorded(void) {
  setUpGame();
  // Set up en passant: 1. e4 a6 2. e5 d5 3. exd6 (en passant)
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 0, 2, 0);  // a6
  gm.makeMove(4, 4, 3, 4);  // e5
  gm.makeMove(1, 3, 3, 3);  // d5
  gm.makeMove(3, 4, 2, 3);  // exd6 (en passant)

  const MoveEntry& m = gm.history().getMove(4);
  TEST_ASSERT_TRUE(m.isEnPassant);
  TEST_ASSERT_TRUE(m.isCapture);
  TEST_ASSERT_EQUAL('p', m.captured);  // captured black pawn
  TEST_ASSERT_EQUAL(3, m.epCapturedRow);
}

void test_game_history_castling_recorded(void) {
  setUpGame();
  // Direct setup: empty between king and rook
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  gm.makeMove(7, 4, 7, 6);  // O-O (white kingside)

  const MoveEntry& m = gm.history().lastMove();
  TEST_ASSERT_TRUE(m.isCastling);
  TEST_ASSERT_EQUAL('K', m.piece);
  TEST_ASSERT_FALSE(m.isCapture);
}

void test_game_history_promotion_recorded(void) {
  setUpGame();
  // White pawn on 7th rank promotes
  gm.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  gm.makeMove(1, 0, 0, 0, 'q');  // a8=Q

  const MoveEntry& m = gm.history().lastMove();
  TEST_ASSERT_TRUE(m.isPromotion);
  TEST_ASSERT_EQUAL('Q', m.promotion);
  TEST_ASSERT_EQUAL('P', m.piece);
}

void test_game_history_check_recorded(void) {
  setUpGame();
  // Set up a position where a move gives check
  gm.loadFEN("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
  gm.makeMove(6, 4, 1, 4);  // Re8+ (check)

  const MoveEntry& m = gm.history().lastMove();
  TEST_ASSERT_TRUE(m.isCheck);
}

void test_game_history_new_game_clears(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);
  TEST_ASSERT_EQUAL(1, gm.history().moveCount());

  gm.newGame();
  TEST_ASSERT_EQUAL(0, gm.history().moveCount());
  TEST_ASSERT_TRUE(gm.history().empty());
}

void test_game_history_load_fen_clears(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);
  TEST_ASSERT_EQUAL(1, gm.history().moveCount());

  gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_EQUAL(0, gm.history().moveCount());
}

void test_game_history_prevstate_saved(void) {
  setUpGame();
  // Before e4: castling=KQkq (0x0F), ep=none, halfmove=0, fullmove=1
  PositionState before = gm.positionState();
  gm.makeMove(6, 4, 4, 4);  // e4

  const MoveEntry& m = gm.history().getMove(0);
  TEST_ASSERT_EQUAL(before.castlingRights, m.prevState.castlingRights);
  TEST_ASSERT_EQUAL(before.halfmoveClock, m.prevState.halfmoveClock);
  TEST_ASSERT_EQUAL(before.fullmoveClock, m.prevState.fullmoveClock);
}

void test_game_history_invalid_move_not_recorded(void) {
  setUpGame();
  // Try an illegal move
  MoveResult r = gm.makeMove(6, 4, 3, 4);  // e2-e5 (illegal: 3 squares)
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_EQUAL(0, gm.history().moveCount());
}

void test_game_history_last_move_after_sequence(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.makeMove(7, 6, 5, 5);  // 2. Nf3

  TEST_ASSERT_EQUAL(3, gm.history().moveCount());
  const MoveEntry& last = gm.history().lastMove();
  TEST_ASSERT_EQUAL('N', last.piece);
  TEST_ASSERT_EQUAL(7, last.fromRow);
  TEST_ASSERT_EQUAL(6, last.fromCol);
  TEST_ASSERT_EQUAL(5, last.toRow);
  TEST_ASSERT_EQUAL(5, last.toCol);
}

void test_game_batch_first_last_single_observer(void) {
  TestObserver obs;
  ChessGame g(nullptr, &obs);
  g.newGame();
  g.makeMove(6, 4, 4, 4);  // e2e4
  g.makeMove(1, 4, 3, 4);  // e5
  g.makeMove(7, 6, 5, 5);  // Nf3
  obs.callCount = 0;

  // "first" pattern: undo all inside a batch → one observer call
  g.beginBatch();
  while (g.canUndo()) g.undoMove();
  g.endBatch();
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);
  TEST_ASSERT_EQUAL(0, g.currentMoveIndex());

  obs.callCount = 0;

  // "last" pattern: redo all inside a batch → one observer call
  g.beginBatch();
  while (g.canRedo()) g.redoMove();
  g.endBatch();
  TEST_ASSERT_EQUAL_INT(1, obs.callCount);
  TEST_ASSERT_EQUAL(3, g.currentMoveIndex());
}

// ---------------------------------------------------------------------------
// History integration (ChessGame owns history, records moves via makeMove)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Navigation facade: currentMoveIndex, moveCount, getHistory
// ---------------------------------------------------------------------------

void test_game_move_count_empty(void) {
  setUpGame();
  TEST_ASSERT_EQUAL(0, gm.moveCount());
}

void test_game_move_count_after_moves(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  TEST_ASSERT_EQUAL(2, gm.moveCount());
}

void test_game_current_move_index_start(void) {
  setUpGame();
  // Before any moves: index 0 (start position)
  TEST_ASSERT_EQUAL(0, gm.currentMoveIndex());
}

void test_game_current_move_index_after_moves(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  TEST_ASSERT_EQUAL(1, gm.currentMoveIndex());
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  TEST_ASSERT_EQUAL(2, gm.currentMoveIndex());
}

void test_game_current_move_index_after_undo(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.undoMove();
  TEST_ASSERT_EQUAL(1, gm.currentMoveIndex());
  gm.undoMove();
  TEST_ASSERT_EQUAL(0, gm.currentMoveIndex());
}

void test_game_get_history_empty(void) {
  setUpGame();
  std::string moves[10];
  int count = gm.getHistory(moves, 10);
  TEST_ASSERT_EQUAL(0, count);
}

void test_game_get_history_basic(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.makeMove(7, 6, 5, 5);  // 2. Nf3

  std::string moves[10];
  int count = gm.getHistory(moves, 10);
  TEST_ASSERT_EQUAL(3, count);
  TEST_ASSERT_EQUAL_STRING("e2e4", moves[0].c_str());
  TEST_ASSERT_EQUAL_STRING("e7e5", moves[1].c_str());
  TEST_ASSERT_EQUAL_STRING("g1f3", moves[2].c_str());
}

void test_game_get_history_with_promotion(void) {
  setUpGame();
  // Set up a position where white can promote
  gm.loadFEN("8/4P3/8/8/8/8/4p3/4K2k w - - 0 1");
  gm.makeMove(1, 4, 0, 4, 'Q');  // e7e8q

  std::string moves[10];
  int count = gm.getHistory(moves, 10);
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL_STRING("e7e8q", moves[0].c_str());
}

void test_game_get_history_includes_all_after_undo(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.undoMove();              // cursor at move 1, but log still has 2 moves

  std::string moves[10];
  int count = gm.getHistory(moves, 10);
  TEST_ASSERT_EQUAL(2, count);
  TEST_ASSERT_EQUAL_STRING("e2e4", moves[0].c_str());
  TEST_ASSERT_EQUAL_STRING("e7e5", moves[1].c_str());
}

void test_game_get_history_max_limit(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.makeMove(7, 6, 5, 5);  // 2. Nf3

  std::string moves[2];
  int count = gm.getHistory(moves, 2);
  TEST_ASSERT_EQUAL(2, count);
  TEST_ASSERT_EQUAL_STRING("e2e4", moves[0].c_str());
  TEST_ASSERT_EQUAL_STRING("e7e5", moves[1].c_str());
}

void test_game_get_history_san_format(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.makeMove(7, 6, 5, 5);  // 2. Nf3

  std::string moves[10];
  int count = gm.getHistory(moves, 10, MoveFormat::SAN);
  TEST_ASSERT_EQUAL(3, count);
  TEST_ASSERT_EQUAL_STRING("e4", moves[0].c_str());
  TEST_ASSERT_EQUAL_STRING("e5", moves[1].c_str());
  TEST_ASSERT_EQUAL_STRING("Nf3", moves[2].c_str());
}

void test_game_get_history_lan_format(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // 1. e4
  gm.makeMove(1, 4, 3, 4);  // 1... e5
  gm.makeMove(7, 6, 5, 5);  // 2. Nf3

  std::string moves[10];
  int count = gm.getHistory(moves, 10, MoveFormat::LAN);
  TEST_ASSERT_EQUAL(3, count);
  TEST_ASSERT_EQUAL_STRING("e2-e4", moves[0].c_str());
  TEST_ASSERT_EQUAL_STRING("e7-e5", moves[1].c_str());
  TEST_ASSERT_EQUAL_STRING("Ng1-f3", moves[2].c_str());
}

// ---------------------------------------------------------------------------
// Lifecycle (gameOver / endGame owned by ChessGame)
// ---------------------------------------------------------------------------

void test_game_end_game_resignation(void) {
  setUpGame();
  gm.endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL('b', gm.winnerColor());
}

void test_game_end_game_timeout(void) {
  setUpGame();
  gm.endGame(GameResult::TIMEOUT, 'w');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::TIMEOUT, gm.gameResult());
  TEST_ASSERT_EQUAL('w', gm.winnerColor());
}

void test_game_end_game_aborted(void) {
  setUpGame();
  gm.endGame(GameResult::ABORTED, ' ');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::ABORTED, gm.gameResult());
  TEST_ASSERT_EQUAL(' ', gm.winnerColor());
}

void test_game_end_game_draw_agreement(void) {
  setUpGame();
  gm.endGame(GameResult::DRAW_AGREEMENT, ' ');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::DRAW_AGREEMENT, gm.gameResult());
  TEST_ASSERT_EQUAL(' ', gm.winnerColor());
}

void test_game_end_game_double_call(void) {
  setUpGame();
  gm.endGame(GameResult::RESIGNATION, 'w');
  gm.endGame(GameResult::TIMEOUT, 'b');
  // First call wins
  TEST_ASSERT_EQUAL(GameResult::RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL('w', gm.winnerColor());
}

void test_game_move_after_game_over_rejected(void) {
  setUpGame();
  gm.endGame(GameResult::RESIGNATION, 'b');
  MoveResult r = gm.makeMove(6, 4, 4, 4);  // e2e4
  TEST_ASSERT_FALSE(r.valid);
}

void test_game_end_game_preserves_fen(void) {
  setUpGame();
  gm.makeMove(6, 4, 4, 4);  // e2e4
  std::string fenBefore = gm.getFen();
  gm.endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), gm.getFen().c_str());
}

void test_game_load_fen_resets_game_over(void) {
  setUpGame();
  gm.endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_TRUE(gm.isGameOver());
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_FALSE(gm.isGameOver());
}

void test_game_new_game_resets_game_over(void) {
  setUpGame();
  gm.endGame(GameResult::RESIGNATION, 'w');
  TEST_ASSERT_TRUE(gm.isGameOver());
  gm.newGame();
  TEST_ASSERT_FALSE(gm.isGameOver());
}

void test_game_checkmate_sets_game_over(void) {
  setUpGame();
  // Scholar's mate
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 4, 3, 4);  // e5
  gm.makeMove(7, 5, 4, 2);  // Bc4
  gm.makeMove(1, 0, 2, 0);  // a6
  gm.makeMove(7, 3, 3, 7);  // Qh5
  gm.makeMove(1, 1, 2, 1);  // b6
  MoveResult r = gm.makeMove(3, 7, 1, 5);  // Qxf7#
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::CHECKMATE, gm.gameResult());
  TEST_ASSERT_EQUAL('w', gm.winnerColor());
}

void test_game_stalemate_sets_game_over(void) {
  setUpGame();
  gm.loadFEN("k7/8/2K5/8/8/8/8/1Q6 w - - 0 1");
  MoveResult r = gm.makeMove(7, 1, 2, 1);  // Qb1-b6 stalemates black
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::STALEMATE, gm.gameResult());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_game_insufficient_material_sets_game_over(void) {
  setUpGame();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3);  // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::DRAW_INSUFFICIENT, gm.gameResult());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_game_fifty_move_sets_game_over(void) {
  setUpGame();
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 99 50");
  MoveResult r = gm.makeMove(7, 0, 7, 1);  // Ra1-b1 (clock hits 100)
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL(GameResult::DRAW_50, gm.gameResult());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_game_undo_clears_game_over(void) {
  setUpGame();
  // Scholar's mate
  gm.makeMove(6, 4, 4, 4);
  gm.makeMove(1, 4, 3, 4);
  gm.makeMove(7, 5, 4, 2);
  gm.makeMove(1, 0, 2, 0);
  gm.makeMove(7, 3, 3, 7);
  gm.makeMove(1, 1, 2, 1);
  gm.makeMove(3, 7, 1, 5);  // Qxf7#
  TEST_ASSERT_TRUE(gm.isGameOver());
  gm.undoMove();
  TEST_ASSERT_FALSE(gm.isGameOver());
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_game_tests() {
  needsDefaultKings = false;

  // Threefold repetition
  RUN_TEST(test_game_threefold_repetition);
  RUN_TEST(test_game_threefold_different_castling_rights);

  // isDraw
  RUN_TEST(test_game_is_draw_insufficient_material);
  RUN_TEST(test_game_is_draw_fifty_move);
  RUN_TEST(test_game_is_draw_false);

  // isThreefoldRepetition
  RUN_TEST(test_game_is_threefold_repetition);
  RUN_TEST(test_game_is_threefold_repetition_false);

  // Observer notification / batching
  RUN_TEST(test_game_observer_fires_on_move);
  RUN_TEST(test_game_observer_fires_on_new_game);
  RUN_TEST(test_game_batch_suppresses_observer);
  RUN_TEST(test_game_no_observer_no_crash);
  RUN_TEST(test_game_nested_batch);
  RUN_TEST(test_game_end_batch_without_begin);
  RUN_TEST(test_game_batch_first_last_single_observer);

  // History integration
  RUN_TEST(test_game_history_records_moves);
  RUN_TEST(test_game_history_correct_fields);
  RUN_TEST(test_game_history_capture_recorded);
  RUN_TEST(test_game_history_en_passant_recorded);
  RUN_TEST(test_game_history_castling_recorded);
  RUN_TEST(test_game_history_promotion_recorded);
  RUN_TEST(test_game_history_check_recorded);
  RUN_TEST(test_game_history_new_game_clears);
  RUN_TEST(test_game_history_load_fen_clears);
  RUN_TEST(test_game_history_prevstate_saved);
  RUN_TEST(test_game_history_invalid_move_not_recorded);
  RUN_TEST(test_game_history_last_move_after_sequence);

  // Navigation facade
  RUN_TEST(test_game_move_count_empty);
  RUN_TEST(test_game_move_count_after_moves);
  RUN_TEST(test_game_current_move_index_start);
  RUN_TEST(test_game_current_move_index_after_moves);
  RUN_TEST(test_game_current_move_index_after_undo);
  RUN_TEST(test_game_get_history_empty);
  RUN_TEST(test_game_get_history_basic);
  RUN_TEST(test_game_get_history_with_promotion);
  RUN_TEST(test_game_get_history_includes_all_after_undo);
  RUN_TEST(test_game_get_history_max_limit);
  RUN_TEST(test_game_get_history_san_format);
  RUN_TEST(test_game_get_history_lan_format);

  // Lifecycle (gameOver / endGame)
  RUN_TEST(test_game_end_game_resignation);
  RUN_TEST(test_game_end_game_timeout);
  RUN_TEST(test_game_end_game_aborted);
  RUN_TEST(test_game_end_game_draw_agreement);
  RUN_TEST(test_game_end_game_double_call);
  RUN_TEST(test_game_move_after_game_over_rejected);
  RUN_TEST(test_game_end_game_preserves_fen);
  RUN_TEST(test_game_load_fen_resets_game_over);
  RUN_TEST(test_game_new_game_resets_game_over);
  RUN_TEST(test_game_checkmate_sets_game_over);
  RUN_TEST(test_game_stalemate_sets_game_over);
  RUN_TEST(test_game_insufficient_material_sets_game_over);
  RUN_TEST(test_game_fifty_move_sets_game_over);
  RUN_TEST(test_game_undo_clears_game_over);
}
