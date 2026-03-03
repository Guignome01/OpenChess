#include <unity.h>

#include "../test_helpers.h"
#include <codec.h>
#include <board.h>
#include <types.h>

// Shared globals from test_core.cpp
extern char board[8][8];
extern bool needsDefaultKings;

static ChessBoard gm;

// Reset ChessBoard before every test
static void setUpManager(void) {
  gm = ChessBoard();
  gm.newGame();
}

// ---------------------------------------------------------------------------
// New game / initial state
// ---------------------------------------------------------------------------

void test_manager_new_game_board(void) {
  setUpManager();
  const char (&b)[8][8] = gm.getBoard();
  TEST_ASSERT_EQUAL_MEMORY(ChessBoard::INITIAL_BOARD, b, 64);
}

void test_manager_new_game_turn(void) {
  setUpManager();
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_manager_new_game_not_over(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.isGameOver());
  TEST_ASSERT_EQUAL_UINT8(RESULT_IN_PROGRESS, gm.gameResult());
}

void test_manager_new_game_fen(void) {
  setUpManager();
  TEST_ASSERT_EQUAL_STRING(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      gm.getFen().c_str());
}

void test_manager_initial_evaluation_zero(void) {
  setUpManager();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, gm.getEvaluation());
}

// ---------------------------------------------------------------------------
// Basic moves
// ---------------------------------------------------------------------------

void test_manager_e2e4(void) {
  setUpManager();
  MoveResult r = gm.makeMove(6, 4, 4, 4); // e2e4
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.isCapture);
  TEST_ASSERT_FALSE(r.isCastling);
  TEST_ASSERT_FALSE(r.isEnPassant);
  TEST_ASSERT_FALSE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('b', gm.currentTurn());
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[6][4]); // e2 empty
  TEST_ASSERT_EQUAL_CHAR('P', gm.getBoard()[4][4]); // e4 has pawn
}

void test_manager_illegal_move_rejected(void) {
  setUpManager();
  MoveResult r = gm.makeMove(6, 4, 3, 4); // e2e5 — not legal
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn()); // turn unchanged
}

void test_manager_wrong_turn_rejected(void) {
  setUpManager();
  MoveResult r = gm.makeMove(1, 4, 3, 4); // e7e5 — black's pawn, but it's white's turn
  TEST_ASSERT_FALSE(r.valid);
}

void test_manager_empty_square_rejected(void) {
  setUpManager();
  MoveResult r = gm.makeMove(4, 4, 3, 4); // e4e5 — empty square
  TEST_ASSERT_FALSE(r.valid);
}

void test_manager_out_of_bounds_rejected(void) {
  setUpManager();
  MoveResult r = gm.makeMove(-1, 0, 0, 0);
  TEST_ASSERT_FALSE(r.valid);
  r = gm.makeMove(0, 0, 8, 0);
  TEST_ASSERT_FALSE(r.valid);
}

void test_manager_move_after_game_over_rejected(void) {
  setUpManager();
  gm.endGame(RESULT_RESIGNATION, 'b');
  MoveResult r = gm.makeMove(6, 4, 4, 4);
  TEST_ASSERT_FALSE(r.valid);
}

// ---------------------------------------------------------------------------
// Captures
// ---------------------------------------------------------------------------

void test_manager_simple_capture(void) {
  setUpManager();
  // Set up a position where white can capture
  gm.loadFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2");
  MoveResult r = gm.makeMove(4, 3, 3, 4); // d4xe5
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_EQUAL_CHAR('P', gm.getBoard()[3][4]); // e5 now has white pawn
}

// ---------------------------------------------------------------------------
// En passant
// ---------------------------------------------------------------------------

void test_manager_en_passant_white(void) {
  setUpManager();
  // White pawn on e5, black just played d7d5
  gm.loadFEN("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
  MoveResult r = gm.makeMove(3, 4, 2, 3); // e5xd6 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_EQUAL_INT(3, r.epCapturedRow); // captured pawn was on row 3, col 3
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[3][3]); // d5 cleared
  TEST_ASSERT_EQUAL_CHAR('P', gm.getBoard()[2][3]); // d6 has white pawn
}

void test_manager_en_passant_black(void) {
  setUpManager();
  // Black pawn on d4, white just played e2e4
  gm.loadFEN("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3");
  MoveResult r = gm.makeMove(4, 3, 5, 4); // d4xe3 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[4][4]); // e4 cleared
  TEST_ASSERT_EQUAL_CHAR('p', gm.getBoard()[5][4]); // e3 has black pawn
}

// ---------------------------------------------------------------------------
// Castling
// ---------------------------------------------------------------------------

void test_manager_white_kingside_castle(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 6); // e1g1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_EQUAL_CHAR('K', gm.getBoard()[7][6]); // king on g1
  TEST_ASSERT_EQUAL_CHAR('R', gm.getBoard()[7][5]); // rook on f1
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[7][4]); // e1 empty
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[7][7]); // h1 empty
}

void test_manager_white_queenside_castle(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 2); // e1c1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_EQUAL_CHAR('K', gm.getBoard()[7][2]); // king on c1
  TEST_ASSERT_EQUAL_CHAR('R', gm.getBoard()[7][3]); // rook on d1
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[7][0]); // a1 empty
}

void test_manager_black_kingside_castle(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
  MoveResult r = gm.makeMove(0, 4, 0, 6); // e8g8
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_EQUAL_CHAR('k', gm.getBoard()[0][6]); // king on g8
  TEST_ASSERT_EQUAL_CHAR('r', gm.getBoard()[0][5]); // rook on f8
}

void test_manager_castling_revokes_rights(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  gm.makeMove(7, 4, 7, 6); // white king-side castle
  // White castling rights revoked
  uint8_t rights = gm.getCastlingRights();
  TEST_ASSERT_EQUAL_UINT8(0, rights & 0x03); // K and Q bits cleared
  TEST_ASSERT_EQUAL_UINT8(0x0C, rights & 0x0C); // k and q bits still set
}

void test_manager_rook_move_revokes_right(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  gm.makeMove(7, 7, 7, 6); // Rh1-g1 (not castling, just a rook move)
  uint8_t rights = gm.getCastlingRights();
  TEST_ASSERT_EQUAL_UINT8(0, rights & 0x01); // K right revoked
  TEST_ASSERT_EQUAL_UINT8(0x02, rights & 0x02); // Q right still present
}

// ---------------------------------------------------------------------------
// Promotion
// ---------------------------------------------------------------------------

void test_manager_auto_queen_promotion(void) {
  setUpManager();
  gm.loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
  MoveResult r = gm.makeMove(1, 4, 0, 4); // e7e8 — auto-queen
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('Q', r.promotedTo);
  TEST_ASSERT_EQUAL_CHAR('Q', gm.getBoard()[0][4]);
}

void test_manager_knight_promotion(void) {
  setUpManager();
  gm.loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
  MoveResult r = gm.makeMove(1, 4, 0, 4, 'n'); // e7e8n
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('N', r.promotedTo); // uppercase for white
  TEST_ASSERT_EQUAL_CHAR('N', gm.getBoard()[0][4]);
}

void test_manager_black_promotion(void) {
  setUpManager();
  gm.loadFEN("4K2k/8/8/8/8/8/4p3/8 b - - 0 1");
  MoveResult r = gm.makeMove(6, 4, 7, 4); // e2e1 — auto-queen
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('q', r.promotedTo); // lowercase for black
}

// ---------------------------------------------------------------------------
// Check detection
// ---------------------------------------------------------------------------

void test_manager_move_gives_check(void) {
  setUpManager();
  // White rook on a1, black king on e8 — Ra1-a8+ is check along rank 8
  gm.loadFEN("4k3/8/8/8/8/8/4K3/R7 w - - 0 1");
  MoveResult r = gm.makeMove(7, 0, 0, 0); // Ra1-a8+
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCheck);
  TEST_ASSERT_EQUAL_UINT8(RESULT_IN_PROGRESS, r.gameResult); // not checkmate
}

void test_manager_move_no_check(void) {
  setUpManager();
  MoveResult r = gm.makeMove(6, 4, 4, 4); // e2e4 — not check
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.isCheck);
}

// ---------------------------------------------------------------------------
// Checkmate
// ---------------------------------------------------------------------------

void test_manager_scholars_mate(void) {
  setUpManager();
  gm.makeMove(6, 4, 4, 4); // e2e4
  gm.makeMove(1, 4, 3, 4); // e7e5
  gm.makeMove(7, 5, 4, 2); // Bf1-c4
  gm.makeMove(1, 0, 2, 0); // a7a6
  gm.makeMove(7, 3, 3, 7); // Qd1-h5
  gm.makeMove(1, 1, 2, 1); // b7b6
  MoveResult r = gm.makeMove(3, 7, 1, 5); // Qh5xf7# — checkmate
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_UINT8(RESULT_CHECKMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('w', r.winnerColor);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL_UINT8(RESULT_CHECKMATE, gm.gameResult());
}

void test_manager_back_rank_mate(void) {
  setUpManager();
  // White rook delivers back-rank mate
  gm.loadFEN("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1");
  MoveResult r = gm.makeMove(7, 0, 0, 0); // Ra1-a8#
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_UINT8(RESULT_CHECKMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('w', r.winnerColor);
}

// ---------------------------------------------------------------------------
// Stalemate
// ---------------------------------------------------------------------------

void test_manager_stalemate(void) {
  setUpManager();
  // Black king on a8, white king on c6, white queen on b1.
  // Qb1-b6 leaves black with no legal moves and no check → stalemate.
  gm.loadFEN("k7/8/2K5/8/8/8/8/1Q6 w - - 0 1");
  MoveResult r = gm.makeMove(7, 1, 2, 1); // Qb1-b6 — stalemates black king
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_UINT8(RESULT_STALEMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('d', r.winnerColor);
  TEST_ASSERT_TRUE(gm.isGameOver());
}

// ---------------------------------------------------------------------------
// 50-move rule
// ---------------------------------------------------------------------------

void test_manager_fifty_move_draw(void) {
  setUpManager();
  // Load position with halfmove clock at 99
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 99 50");
  MoveResult r = gm.makeMove(7, 0, 7, 1); // Ra1-b1 (non-capture, non-pawn = clock hits 100)
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_UINT8(RESULT_DRAW_50, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('d', r.winnerColor);
}

// ---------------------------------------------------------------------------
// Threefold repetition
// ---------------------------------------------------------------------------

void test_manager_threefold_repetition(void) {
  setUpManager();
  // Position where Ke1 and Ke8 shuffle back and forth
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
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
  TEST_ASSERT_EQUAL_UINT8(RESULT_DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(gm.isGameOver());
}

// ---------------------------------------------------------------------------
// FEN loading
// ---------------------------------------------------------------------------

void test_manager_load_fen_sets_turn(void) {
  setUpManager();
  gm.loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  TEST_ASSERT_EQUAL_CHAR('b', gm.currentTurn());
}

void test_manager_load_fen_resets_game_over(void) {
  setUpManager();
  gm.endGame(RESULT_RESIGNATION, 'w');
  TEST_ASSERT_TRUE(gm.isGameOver());
  gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_FALSE(gm.isGameOver());
}

void test_manager_load_fen_roundtrip(void) {
  setUpManager();
  std::string inputFen = "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  gm.loadFEN(inputFen);
  TEST_ASSERT_EQUAL_STRING(inputFen.c_str(), gm.getFen().c_str());
}

// ---------------------------------------------------------------------------
// endGame
// ---------------------------------------------------------------------------

void test_manager_end_game_resignation(void) {
  setUpManager();
  gm.endGame(RESULT_RESIGNATION, 'b');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL_UINT8(RESULT_RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL_CHAR('b', gm.winnerColor());
}

void test_manager_end_game_timeout(void) {
  setUpManager();
  gm.endGame(RESULT_TIMEOUT, 'w');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL_UINT8(RESULT_TIMEOUT, gm.gameResult());
}

void test_manager_end_game_aborted(void) {
  setUpManager();
  gm.endGame(RESULT_ABORTED, ' ');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_EQUAL_UINT8(RESULT_ABORTED, gm.gameResult());
}

// ---------------------------------------------------------------------------
// Callback / batching
// ---------------------------------------------------------------------------

static int callbackCount;
static void countingCallback() { callbackCount++; }

void test_manager_callback_fires_on_move(void) {
  setUpManager();
  callbackCount = 0;
  gm.onStateChanged(countingCallback);
  gm.makeMove(6, 4, 4, 4); // e2e4
  TEST_ASSERT_EQUAL_INT(1, callbackCount);
}

void test_manager_callback_fires_on_new_game(void) {
  setUpManager();
  callbackCount = 0;
  gm.onStateChanged(countingCallback);
  gm.newGame();
  TEST_ASSERT_EQUAL_INT(1, callbackCount);
}

void test_manager_batch_suppresses_callbacks(void) {
  setUpManager();
  callbackCount = 0;
  gm.onStateChanged(countingCallback);
  gm.beginBatch();
  gm.makeMove(6, 4, 4, 4);
  gm.makeMove(1, 4, 3, 4);
  TEST_ASSERT_EQUAL_INT(0, callbackCount);
  gm.endBatch();
  TEST_ASSERT_EQUAL_INT(1, callbackCount); // single coalesced callback
}

void test_manager_no_callback_when_not_set(void) {
  setUpManager();
  // Just ensure no crash when no callback is set
  gm.makeMove(6, 4, 4, 4); // should not crash
  TEST_ASSERT_TRUE(true);
}

// ---------------------------------------------------------------------------
// Compact move encoding (ChessCodec)
// ---------------------------------------------------------------------------

void test_codec_encode_decode_roundtrip(void) {
  uint16_t encoded = ChessCodec::encodeMove(6, 4, 4, 4, ' '); // e2e4
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(6, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(4, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR(' ', promo);
}

void test_codec_encode_decode_with_promotion(void) {
  uint16_t encoded = ChessCodec::encodeMove(1, 4, 0, 4, 'q'); // e7e8q
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(1, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(0, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR('q', promo);
}

void test_codec_encode_decode_corner_squares(void) {
  // a8 (0,0) -> h1 (7,7) with knight promotion
  uint16_t encoded = ChessCodec::encodeMove(0, 0, 7, 7, 'n');
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(0, fr);
  TEST_ASSERT_EQUAL_INT(0, fc);
  TEST_ASSERT_EQUAL_INT(7, tr);
  TEST_ASSERT_EQUAL_INT(7, tc);
  TEST_ASSERT_EQUAL_CHAR('n', promo);
}

// ---------------------------------------------------------------------------
// FEN / evaluation cache
// ---------------------------------------------------------------------------

void test_manager_fen_cache_consistent(void) {
  setUpManager();
  std::string fen1 = gm.getFen();
  std::string fen2 = gm.getFen();
  TEST_ASSERT_EQUAL_STRING(fen1.c_str(), fen2.c_str());

  gm.makeMove(6, 4, 4, 4);  // e2e4
  std::string fen3 = gm.getFen();
  TEST_ASSERT_TRUE(fen3 != fen1);  // FEN changed after move
}

void test_manager_eval_cache_consistent(void) {
  setUpManager();
  float eval1 = gm.getEvaluation();
  float eval2 = gm.getEvaluation();
  TEST_ASSERT_FLOAT_WITHIN(0.001f, eval1, eval2);  // Same value from cache

  // Load asymmetric position (white missing e-pawn) and verify cache updates
  gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
  float eval3 = gm.getEvaluation();
  // White has one fewer pawn → negative evaluation
  TEST_ASSERT_TRUE(eval3 < eval1);
}

void test_manager_end_game_preserves_fen(void) {
  setUpManager();
  std::string fenBefore = gm.getFen();
  gm.endGame(RESULT_RESIGNATION, 'b');
  std::string fenAfter = gm.getFen();
  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), fenAfter.c_str());
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_board_tests() {
  needsDefaultKings = false;

  // Initial state
  RUN_TEST(test_manager_new_game_board);
  RUN_TEST(test_manager_new_game_turn);
  RUN_TEST(test_manager_new_game_not_over);
  RUN_TEST(test_manager_new_game_fen);
  RUN_TEST(test_manager_initial_evaluation_zero);

  // Basic moves
  RUN_TEST(test_manager_e2e4);
  RUN_TEST(test_manager_illegal_move_rejected);
  RUN_TEST(test_manager_wrong_turn_rejected);
  RUN_TEST(test_manager_empty_square_rejected);
  RUN_TEST(test_manager_out_of_bounds_rejected);
  RUN_TEST(test_manager_move_after_game_over_rejected);

  // Captures
  RUN_TEST(test_manager_simple_capture);

  // En passant
  RUN_TEST(test_manager_en_passant_white);
  RUN_TEST(test_manager_en_passant_black);

  // Castling
  RUN_TEST(test_manager_white_kingside_castle);
  RUN_TEST(test_manager_white_queenside_castle);
  RUN_TEST(test_manager_black_kingside_castle);
  RUN_TEST(test_manager_castling_revokes_rights);
  RUN_TEST(test_manager_rook_move_revokes_right);

  // Promotion
  RUN_TEST(test_manager_auto_queen_promotion);
  RUN_TEST(test_manager_knight_promotion);
  RUN_TEST(test_manager_black_promotion);

  // Check
  RUN_TEST(test_manager_move_gives_check);
  RUN_TEST(test_manager_move_no_check);

  // Checkmate
  RUN_TEST(test_manager_scholars_mate);
  RUN_TEST(test_manager_back_rank_mate);

  // Stalemate
  RUN_TEST(test_manager_stalemate);

  // 50-move rule
  RUN_TEST(test_manager_fifty_move_draw);

  // Threefold repetition
  RUN_TEST(test_manager_threefold_repetition);

  // FEN loading
  RUN_TEST(test_manager_load_fen_sets_turn);
  RUN_TEST(test_manager_load_fen_resets_game_over);
  RUN_TEST(test_manager_load_fen_roundtrip);

  // endGame
  RUN_TEST(test_manager_end_game_resignation);
  RUN_TEST(test_manager_end_game_timeout);
  RUN_TEST(test_manager_end_game_aborted);

  // Callbacks
  RUN_TEST(test_manager_callback_fires_on_move);
  RUN_TEST(test_manager_callback_fires_on_new_game);
  RUN_TEST(test_manager_batch_suppresses_callbacks);
  RUN_TEST(test_manager_no_callback_when_not_set);

  // Codec
  RUN_TEST(test_codec_encode_decode_roundtrip);
  RUN_TEST(test_codec_encode_decode_with_promotion);
  RUN_TEST(test_codec_encode_decode_corner_squares);

  // FEN/eval cache
  RUN_TEST(test_manager_fen_cache_consistent);
  RUN_TEST(test_manager_eval_cache_consistent);
  RUN_TEST(test_manager_end_game_preserves_fen);
}
