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
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, gm.gameResult());
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
  gm.endGame(GameResult::RESIGNATION, 'b');
  MoveResult r = gm.makeMove(6, 4, 4, 4);
  TEST_ASSERT_FALSE(r.valid);
}

// ---------------------------------------------------------------------------
// getSquare
// ---------------------------------------------------------------------------

void test_manager_getSquare_returns_piece(void) {
  setUpManager();
  TEST_ASSERT_EQUAL_CHAR('R', gm.getSquare(7, 0)); // a1
  TEST_ASSERT_EQUAL_CHAR('k', gm.getSquare(0, 4)); // e8
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(4, 4)); // e4 empty
}

// ---------------------------------------------------------------------------
// invalidMoveResult
// ---------------------------------------------------------------------------

void test_manager_invalidMoveResult_fields(void) {
  MoveResult r = invalidMoveResult();
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_FALSE(r.isCapture);
  TEST_ASSERT_FALSE(r.isEnPassant);
  TEST_ASSERT_EQUAL_INT(-1, r.epCapturedRow);
  TEST_ASSERT_FALSE(r.isCastling);
  TEST_ASSERT_FALSE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR(' ', r.promotedTo);
  TEST_ASSERT_FALSE(r.isCheck);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR(' ', r.winnerColor);
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

void test_manager_ep_target_set_after_double_push(void) {
  setUpManager();
  gm.makeMove(6, 4, 4, 4); // e2-e4 (double push)
  const PositionState& st = gm.positionState();
  TEST_ASSERT_EQUAL_INT(5, st.epRow); // EP target = e3 (row 5)
  TEST_ASSERT_EQUAL_INT(4, st.epCol); // col 4
}

void test_manager_ep_target_cleared_after_other_move(void) {
  setUpManager();
  gm.makeMove(6, 4, 4, 4); // e2-e4 sets EP target
  TEST_ASSERT_EQUAL_INT(5, gm.positionState().epRow); // EP target exists
  gm.makeMove(1, 0, 2, 0); // a7-a6 (not a double push)
  TEST_ASSERT_EQUAL_INT(-1, gm.positionState().epRow); // EP target cleared
  TEST_ASSERT_EQUAL_INT(-1, gm.positionState().epCol);
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

void test_manager_black_queenside_castle(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
  MoveResult r = gm.makeMove(0, 4, 0, 2); // e8c8
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_EQUAL_CHAR('k', gm.getBoard()[0][2]); // king on c8
  TEST_ASSERT_EQUAL_CHAR('r', gm.getBoard()[0][3]); // rook on d8
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getBoard()[0][0]); // a8 empty
}

void test_manager_rook_captured_revokes_castling(void) {
  setUpManager();
  // Black rook at h2 can capture white rook at h1; white has K castling right
  gm.loadFEN("4k3/8/8/8/8/8/7r/4K2R b K - 0 1");
  MoveResult r = gm.makeMove(6, 7, 7, 7); // rh2 x Rh1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_EQUAL_UINT8(0, gm.getCastlingRights()); // K right revoked
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

void test_manager_promotion_with_capture(void) {
  setUpManager();
  // White pawn on d7 captures black rook on e8 and promotes
  gm.loadFEN("4r2k/3P4/8/8/8/8/8/4K3 w - - 0 1");
  MoveResult r = gm.makeMove(1, 3, 0, 4); // d7xe8=Q
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('Q', r.promotedTo);
  TEST_ASSERT_EQUAL_CHAR('Q', gm.getBoard()[0][4]);
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
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult); // not checkmate
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
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('w', r.winnerColor);
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, gm.gameResult());
}

void test_manager_back_rank_mate(void) {
  setUpManager();
  // White rook delivers back-rank mate
  gm.loadFEN("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1");
  MoveResult r = gm.makeMove(7, 0, 0, 0); // Ra1-a8#
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
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
  TEST_ASSERT_ENUM_EQ(GameResult::STALEMATE, r.gameResult);
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
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_50, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('d', r.winnerColor);
}

// ---------------------------------------------------------------------------
// Threefold repetition
// ---------------------------------------------------------------------------

void test_manager_threefold_repetition(void) {
  setUpManager();
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

void test_manager_threefold_different_castling_rights(void) {
  setUpManager();
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
// Insufficient material
// ---------------------------------------------------------------------------

void test_manager_insufficient_material_k_vs_k(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  // Any move should trigger DRAW_INSUFFICIENT
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(gm.isGameOver());
}

void test_manager_insufficient_material_kb_vs_k(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
}

void test_manager_insufficient_material_kn_vs_k(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K1N1 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
}

void test_manager_insufficient_material_kb_vs_kb_same_color(void) {
  setUpManager();
  // Both bishops on light squares (c1=dark, d1 would be light, f1=light)
  // c8 is light square (row 0 + col 2 = even), f1 is light square (row 7 + col 5 = even)
  gm.loadFEN("2b1k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
}

void test_manager_insufficient_material_kb_vs_kb_diff_color(void) {
  setUpManager();
  // White bishop on f1 (light: 7+5=even), black bishop on c8 is light too.
  // Put black bishop on d8 (dark: 0+3=odd) for different colors.
  gm.loadFEN("3bk3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  // Different color bishops — NOT insufficient
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isGameOver());
}

void test_manager_sufficient_material_knn(void) {
  setUpManager();
  // K+N+N vs K — sufficient material (checkmate is possible)
  gm.loadFEN("4k3/8/8/8/8/8/8/2N1KN2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isGameOver());
}

void test_manager_sufficient_material_kp_vs_k(void) {
  setUpManager();
  // K+P vs K — sufficient material (pawn can promote)
  gm.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
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
  gm.endGame(GameResult::RESIGNATION, 'w');
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

void test_manager_load_fen_complex(void) {
  setUpManager();
  std::string fen = "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
  gm.loadFEN(fen);
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
  TEST_ASSERT_EQUAL_UINT8(0x0F, gm.getCastlingRights()); // KQkq
  TEST_ASSERT_EQUAL_INT(-1, gm.positionState().epRow);     // no EP
  TEST_ASSERT_EQUAL_INT(4, gm.positionState().halfmoveClock);
  TEST_ASSERT_EQUAL_INT(4, gm.positionState().fullmoveClock);
  TEST_ASSERT_EQUAL_CHAR('B', gm.getSquare(4, 2)); // Bc4
  TEST_ASSERT_EQUAL_CHAR('n', gm.getSquare(2, 2)); // Nc6
  TEST_ASSERT_EQUAL_STRING(fen.c_str(), gm.getFen().c_str());
}

// --- FEN validation ---

void test_manager_load_fen_rejects_empty(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.loadFEN(""));
}

void test_manager_load_fen_rejects_too_few_ranks(void) {
  setUpManager();
  // Only 7 ranks (6 slashes)
  TEST_ASSERT_FALSE(gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1"));
}

void test_manager_load_fen_rejects_too_many_ranks(void) {
  setUpManager();
  // 9 ranks (8 slashes)
  TEST_ASSERT_FALSE(gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_manager_load_fen_rejects_invalid_piece(void) {
  setUpManager();
  // 'x' is not a valid piece character
  TEST_ASSERT_FALSE(gm.loadFEN("xnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_manager_load_fen_rejects_rank_overflow(void) {
  setUpManager();
  // First rank sums to 9
  TEST_ASSERT_FALSE(gm.loadFEN("rnbqkbnrr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_manager_load_fen_rejects_invalid_turn(void) {
  setUpManager();
  // 'x' is not a valid turn
  TEST_ASSERT_FALSE(gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"));
}

void test_manager_load_fen_valid_returns_true(void) {
  setUpManager();
  TEST_ASSERT_TRUE(gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_manager_load_fen_invalid_preserves_state(void) {
  setUpManager();
  // Make a move so state differs from initial
  gm.makeMove(6, 4, 4, 4); // e2e4
  std::string fenBefore = gm.getFen();
  // Invalid FEN should leave state unchanged
  TEST_ASSERT_FALSE(gm.loadFEN("bad_fen"));
  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), gm.getFen().c_str());
}

// ---------------------------------------------------------------------------
// endGame
// ---------------------------------------------------------------------------

void test_manager_end_game_resignation(void) {
  setUpManager();
  gm.endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL_CHAR('b', gm.winnerColor());
}

void test_manager_end_game_timeout(void) {
  setUpManager();
  gm.endGame(GameResult::TIMEOUT, 'w');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::TIMEOUT, gm.gameResult());
}

void test_manager_end_game_aborted(void) {
  setUpManager();
  gm.endGame(GameResult::ABORTED, ' ');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::ABORTED, gm.gameResult());
}

void test_manager_end_game_draw_agreement(void) {
  setUpManager();
  gm.endGame(GameResult::DRAW_AGREEMENT, 'd');
  TEST_ASSERT_TRUE(gm.isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_AGREEMENT, gm.gameResult());
  TEST_ASSERT_EQUAL_CHAR('d', gm.winnerColor());
}

void test_manager_end_game_double_call(void) {
  setUpManager();
  gm.endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_ENUM_EQ(GameResult::RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL_CHAR('b', gm.winnerColor());
  // Second call should be a no-op — result preserved
  gm.endGame(GameResult::TIMEOUT, 'w');
  TEST_ASSERT_ENUM_EQ(GameResult::RESIGNATION, gm.gameResult());
  TEST_ASSERT_EQUAL_CHAR('b', gm.winnerColor());
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

void test_manager_nested_batch(void) {
  setUpManager();
  callbackCount = 0;
  gm.onStateChanged(countingCallback);
  gm.beginBatch();
  gm.beginBatch();  // depth 2
  gm.makeMove(6, 4, 4, 4);
  gm.endBatch();    // depth 1 — callback still suppressed
  TEST_ASSERT_EQUAL_INT(0, callbackCount);
  gm.endBatch();    // depth 0 — fires coalesced callback
  TEST_ASSERT_EQUAL_INT(1, callbackCount);
}

void test_manager_end_batch_without_begin(void) {
  setUpManager();
  callbackCount = 0;
  gm.onStateChanged(countingCallback);
  // endBatch with no beginBatch should not crash or fire callback
  gm.endBatch();
  TEST_ASSERT_EQUAL_INT(0, callbackCount);
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
  gm.endGame(GameResult::RESIGNATION, 'b');
  std::string fenAfter = gm.getFen();
  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), fenAfter.c_str());
}

void test_manager_eval_after_capture(void) {
  setUpManager();
  float initialEval = gm.getEvaluation();
  // White captures black's e-pawn (material advantage for white)
  gm.loadFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2");
  gm.makeMove(4, 3, 3, 4); // d4xe5
  float evalAfter = gm.getEvaluation();
  TEST_ASSERT_TRUE(evalAfter > initialEval); // white gained material
}

// ---------------------------------------------------------------------------
// Position clocks (halfmove / fullmove)
// ---------------------------------------------------------------------------

void test_manager_halfmove_clock_increments(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  gm.makeMove(7, 0, 7, 1); // Ra1-b1 (non-capture, non-pawn)
  TEST_ASSERT_EQUAL_INT(1, gm.positionState().halfmoveClock);
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  TEST_ASSERT_EQUAL_INT(2, gm.positionState().halfmoveClock);
}

void test_manager_halfmove_clock_resets_on_pawn_move(void) {
  setUpManager();
  // Start with halfmove=5 and make a pawn move
  gm.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 5 10");
  TEST_ASSERT_EQUAL_INT(5, gm.positionState().halfmoveClock);
  gm.makeMove(6, 4, 4, 4); // e2-e4 (pawn move resets clock)
  TEST_ASSERT_EQUAL_INT(0, gm.positionState().halfmoveClock);
}

void test_manager_fullmove_increments_after_black(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  TEST_ASSERT_EQUAL_INT(1, gm.positionState().fullmoveClock);
  gm.makeMove(6, 4, 4, 4); // e2-e4 (white moves, fullmove stays 1)
  TEST_ASSERT_EQUAL_INT(1, gm.positionState().fullmoveClock);
  gm.makeMove(0, 4, 0, 3); // Ke8-d8 (black moves, fullmove → 2)
  TEST_ASSERT_EQUAL_INT(2, gm.positionState().fullmoveClock);
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
  RUN_TEST(test_manager_getSquare_returns_piece);
  RUN_TEST(test_manager_invalidMoveResult_fields);

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
  RUN_TEST(test_manager_ep_target_set_after_double_push);
  RUN_TEST(test_manager_ep_target_cleared_after_other_move);

  // Castling
  RUN_TEST(test_manager_white_kingside_castle);
  RUN_TEST(test_manager_white_queenside_castle);
  RUN_TEST(test_manager_black_kingside_castle);
  RUN_TEST(test_manager_black_queenside_castle);
  RUN_TEST(test_manager_castling_revokes_rights);
  RUN_TEST(test_manager_rook_move_revokes_right);
  RUN_TEST(test_manager_rook_captured_revokes_castling);

  // Promotion
  RUN_TEST(test_manager_auto_queen_promotion);
  RUN_TEST(test_manager_knight_promotion);
  RUN_TEST(test_manager_black_promotion);
  RUN_TEST(test_manager_promotion_with_capture);

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
  RUN_TEST(test_manager_threefold_different_castling_rights);

  // Insufficient material
  RUN_TEST(test_manager_insufficient_material_k_vs_k);
  RUN_TEST(test_manager_insufficient_material_kb_vs_k);
  RUN_TEST(test_manager_insufficient_material_kn_vs_k);
  RUN_TEST(test_manager_insufficient_material_kb_vs_kb_same_color);
  RUN_TEST(test_manager_insufficient_material_kb_vs_kb_diff_color);
  RUN_TEST(test_manager_sufficient_material_knn);
  RUN_TEST(test_manager_sufficient_material_kp_vs_k);

  // FEN loading
  RUN_TEST(test_manager_load_fen_sets_turn);
  RUN_TEST(test_manager_load_fen_resets_game_over);
  RUN_TEST(test_manager_load_fen_roundtrip);
  RUN_TEST(test_manager_load_fen_complex);

  // FEN validation
  RUN_TEST(test_manager_load_fen_rejects_empty);
  RUN_TEST(test_manager_load_fen_rejects_too_few_ranks);
  RUN_TEST(test_manager_load_fen_rejects_too_many_ranks);
  RUN_TEST(test_manager_load_fen_rejects_invalid_piece);
  RUN_TEST(test_manager_load_fen_rejects_rank_overflow);
  RUN_TEST(test_manager_load_fen_rejects_invalid_turn);
  RUN_TEST(test_manager_load_fen_valid_returns_true);
  RUN_TEST(test_manager_load_fen_invalid_preserves_state);

  // endGame
  RUN_TEST(test_manager_end_game_resignation);
  RUN_TEST(test_manager_end_game_timeout);
  RUN_TEST(test_manager_end_game_aborted);
  RUN_TEST(test_manager_end_game_draw_agreement);
  RUN_TEST(test_manager_end_game_double_call);

  // Callbacks / batching
  RUN_TEST(test_manager_callback_fires_on_move);
  RUN_TEST(test_manager_callback_fires_on_new_game);
  RUN_TEST(test_manager_batch_suppresses_callbacks);
  RUN_TEST(test_manager_no_callback_when_not_set);
  RUN_TEST(test_manager_nested_batch);
  RUN_TEST(test_manager_end_batch_without_begin);

  // Codec
  RUN_TEST(test_codec_encode_decode_roundtrip);
  RUN_TEST(test_codec_encode_decode_with_promotion);
  RUN_TEST(test_codec_encode_decode_corner_squares);

  // FEN/eval cache
  RUN_TEST(test_manager_fen_cache_consistent);
  RUN_TEST(test_manager_eval_cache_consistent);
  RUN_TEST(test_manager_end_game_preserves_fen);
  RUN_TEST(test_manager_eval_after_capture);

  // Position clocks
  RUN_TEST(test_manager_halfmove_clock_increments);
  RUN_TEST(test_manager_halfmove_clock_resets_on_pawn_move);
  RUN_TEST(test_manager_fullmove_increments_after_black);
}
