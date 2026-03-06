#include <unity.h>

#include "../test_helpers.h"
#include <chess_board.h>
#include <chess_history.h>
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
  TEST_ASSERT_FALSE(gm.isCheckmate());
  TEST_ASSERT_FALSE(gm.isDraw());
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
  // Reach checkmate via scholar's mate, then try to move
  gm.makeMove(6, 4, 4, 4); // e4
  gm.makeMove(1, 4, 3, 4); // e5
  gm.makeMove(7, 5, 4, 2); // Bc4
  gm.makeMove(1, 0, 2, 0); // a6
  gm.makeMove(7, 3, 3, 7); // Qh5
  gm.makeMove(1, 1, 2, 1); // b6
  MoveResult r = gm.makeMove(3, 7, 1, 5); // Qxf7#
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  // Board no longer guards; this tests that the position is in checkmate
  TEST_ASSERT_TRUE(gm.isCheckmate());
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
  TEST_ASSERT_TRUE(gm.isCheckmate());
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
  TEST_ASSERT_TRUE(gm.isStalemate());
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
  TEST_ASSERT_TRUE(gm.isFiftyMoves());
  TEST_ASSERT_TRUE(gm.isDraw());
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
  TEST_ASSERT_TRUE(gm.isInsufficientMaterial());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_manager_insufficient_material_kb_vs_k(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_manager_insufficient_material_kn_vs_k(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K1N1 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_manager_insufficient_material_kb_vs_kb_same_color(void) {
  setUpManager();
  // Both bishops on light squares (c1=dark, d1 would be light, f1=light)
  // c8 is light square (row 0 + col 2 = even), f1 is light square (row 7 + col 5 = even)
  gm.loadFEN("2b1k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(gm.isDraw());
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
  TEST_ASSERT_FALSE(gm.isInsufficientMaterial());
}

void test_manager_sufficient_material_knn(void) {
  setUpManager();
  // K+N+N vs K — sufficient material (checkmate is possible)
  gm.loadFEN("4k3/8/8/8/8/8/8/2N1KN2 w - - 0 1");
  MoveResult r = gm.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isCheckmate());
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
  // Reach checkmate first
  gm.makeMove(6, 4, 4, 4); // e4
  gm.makeMove(1, 4, 3, 4); // e5
  gm.makeMove(7, 5, 4, 2); // Bc4
  gm.makeMove(1, 0, 2, 0); // a6
  gm.makeMove(7, 3, 3, 7); // Qh5
  gm.makeMove(1, 1, 2, 1); // b6
  MoveResult r = gm.makeMove(3, 7, 1, 5); // Qxf7#
  TEST_ASSERT_TRUE(gm.isCheckmate());
  // loadFEN resets position to non-terminal
  gm.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_FALSE(gm.isCheckmate());
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
// Compact move encoding
// ---------------------------------------------------------------------------

void test_encodeMove_roundtrip(void) {
  uint16_t encoded = ChessHistory::encodeMove(6, 4, 4, 4, ' '); // e2e4
  int fr, fc, tr, tc;
  char promo;
  ChessHistory::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(6, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(4, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR(' ', promo);
}

void test_encodeMove_with_promotion(void) {
  uint16_t encoded = ChessHistory::encodeMove(1, 4, 0, 4, 'q'); // e7e8q
  int fr, fc, tr, tc;
  char promo;
  ChessHistory::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(1, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(0, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR('q', promo);
}

void test_encodeMove_corner_squares(void) {
  // a8 (0,0) -> h1 (7,7) with knight promotion
  uint16_t encoded = ChessHistory::encodeMove(0, 0, 7, 7, 'n');
  int fr, fc, tr, tc;
  char promo;
  ChessHistory::decodeMove(encoded, fr, fc, tr, tc, promo);
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
  // Making a move changes FEN, but non-terminal positions preserve structure
  TEST_ASSERT_EQUAL_STRING(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      fenBefore.c_str());
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
// findPiece
// ---------------------------------------------------------------------------

void test_manager_find_piece_kings_initial(void) {
  setUpManager();
  int positions[2][2];
  int count = gm.findPiece('K', 'w', positions, 2);
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_INT(7, positions[0][0]); // row 7 = rank 1
  TEST_ASSERT_EQUAL_INT(4, positions[0][1]); // col 4 = file e
}

void test_manager_find_piece_black_pawns_initial(void) {
  setUpManager();
  int positions[8][2];
  int count = gm.findPiece('P', 'b', positions, 8);
  TEST_ASSERT_EQUAL_INT(8, count);
  // All should be on row 1 (rank 7)
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_INT(1, positions[i][0]);
  }
}

void test_manager_find_piece_not_found(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  int positions[8][2];
  int count = gm.findPiece('Q', 'w', positions, 8);
  TEST_ASSERT_EQUAL_INT(0, count);
}

void test_manager_find_piece_multiple_bishops(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/2B1B3/8/4K3 w - - 0 1");
  int positions[4][2];
  int count = gm.findPiece('B', 'w', positions, 4);
  TEST_ASSERT_EQUAL_INT(2, count);
}

void test_manager_find_piece_max_limit(void) {
  setUpManager();
  // 8 white pawns, but limit to 3
  int positions[3][2];
  int count = gm.findPiece('P', 'w', positions, 3);
  TEST_ASSERT_EQUAL_INT(3, count);
}

// ---------------------------------------------------------------------------
// inCheck (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_manager_in_check_true(void) {
  setUpManager();
  // White king in check from black queen
  gm.loadFEN("4k3/8/8/8/8/8/8/3qK3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.inCheck());
}

void test_manager_in_check_false(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.inCheck());
}

// ---------------------------------------------------------------------------
// isCheckmate (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_manager_is_checkmate_true(void) {
  setUpManager();
  // Back rank mate: white king on h1, black rook delivers mate on a1
  gm.loadFEN("6k1/8/8/8/8/8/5PPP/r5K1 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isCheckmate());
}

void test_manager_is_checkmate_false(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.isCheckmate());
}

// ---------------------------------------------------------------------------
// isStalemate (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_manager_is_stalemate_true(void) {
  setUpManager();
  // Stalemate: black king on a8, white queen on b6, white king on c8 — black to move
  gm.loadFEN("k7/8/1Q6/8/8/8/8/2K5 b - - 0 1");
  TEST_ASSERT_TRUE(gm.isStalemate());
}

void test_manager_is_stalemate_false(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.isStalemate());
}

// ---------------------------------------------------------------------------
// isInsufficientMaterial (public)
// ---------------------------------------------------------------------------

void test_manager_is_insufficient_material_true(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isInsufficientMaterial());
}

void test_manager_is_insufficient_material_false(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.isInsufficientMaterial());
}

// ---------------------------------------------------------------------------
// isAttacked
// ---------------------------------------------------------------------------

void test_manager_is_attacked_by_white(void) {
  setUpManager();
  // White pawn on e4 attacks d5 and f5
  gm.loadFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isAttacked(3, 3, 'w'));  // d5 attacked by white
  TEST_ASSERT_TRUE(gm.isAttacked(3, 5, 'w'));  // f5 attacked by white
  TEST_ASSERT_FALSE(gm.isAttacked(3, 4, 'w')); // e5 not attacked by white pawn
}

void test_manager_is_attacked_by_black(void) {
  setUpManager();
  // Black knight on f6 attacks e4, g4, d5, h5, d7, h7, e8, g8
  gm.loadFEN("4k3/8/5n2/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isAttacked(4, 4, 'b'));  // e4 attacked by black knight
  TEST_ASSERT_TRUE(gm.isAttacked(4, 6, 'b'));  // g4 attacked by black knight
  TEST_ASSERT_FALSE(gm.isAttacked(4, 5, 'b')); // f4 not attacked by black knight
}

void test_manager_is_attacked_empty_square(void) {
  setUpManager();
  // Empty square can be attacked
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  TEST_ASSERT_TRUE(gm.isAttacked(0, 0, 'w')); // a8 attacked by Ra1 along file
}

// ---------------------------------------------------------------------------
// moveNumber
// ---------------------------------------------------------------------------

void test_manager_move_number_initial(void) {
  setUpManager();
  TEST_ASSERT_EQUAL_INT(1, gm.moveNumber());
}

void test_manager_move_number_after_moves(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  gm.makeMove(6, 4, 4, 4); // e2-e4 (white, fullmove stays 1)
  TEST_ASSERT_EQUAL_INT(1, gm.moveNumber());
  gm.makeMove(0, 4, 0, 3); // Ke8-d8 (black, fullmove → 2)
  TEST_ASSERT_EQUAL_INT(2, gm.moveNumber());
}

void test_manager_move_number_from_fen(void) {
  setUpManager();
  gm.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 42");
  TEST_ASSERT_EQUAL_INT(42, gm.moveNumber());
}

// ---------------------------------------------------------------------------
// Threefold repetition (Zobrist position tracking in ChessBoard)
// ---------------------------------------------------------------------------

void test_board_threefold_repetition(void) {
  setUpManager();
  // Position where Ke1 and Ke8 shuffle back and forth (pawns provide sufficient material)
  gm.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  // Move 1: Ke1-d1, Ke8-d8
  gm.makeMove(7, 4, 7, 3);
  gm.makeMove(0, 4, 0, 3);
  // Move 2: Kd1-e1, Kd8-e8 (back to original — occurrence 2)
  gm.makeMove(7, 3, 7, 4);
  gm.makeMove(0, 3, 0, 4);
  // Move 3: Ke1-d1, Ke8-d8
  gm.makeMove(7, 4, 7, 3);
  gm.makeMove(0, 4, 0, 3);
  // Move 4: Kd1-e1, Kd8-e8 (back to original — occurrence 3)
  gm.makeMove(7, 3, 7, 4);
  MoveResult r = gm.makeMove(0, 3, 0, 4);  // third repetition
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(gm.isThreefoldRepetition());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_board_threefold_different_castling_rights(void) {
  setUpManager();
  // King move loses castling rights → initial hash differs from subsequent hashes
  gm.loadFEN("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
  gm.makeMove(7, 4, 7, 3); // Ke1-d1 (loses K castling)
  gm.makeMove(0, 4, 0, 3);
  gm.makeMove(7, 3, 7, 4);
  gm.makeMove(0, 3, 0, 4); // board same as start, but castling=0
  gm.makeMove(7, 4, 7, 3);
  gm.makeMove(0, 4, 0, 3);
  gm.makeMove(7, 3, 7, 4);
  MoveResult r = gm.makeMove(0, 3, 0, 4); // occurrence 2 (not 3)
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isThreefoldRepetition());
}

void test_board_threefold_not_reached(void) {
  setUpManager();
  // Only 2 occurrences — not enough for threefold
  gm.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  gm.makeMove(7, 4, 7, 3);
  gm.makeMove(0, 4, 0, 3);
  gm.makeMove(7, 3, 7, 4);
  MoveResult r = gm.makeMove(0, 3, 0, 4); // occurrence 2
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(gm.isThreefoldRepetition());
}

void test_board_threefold_query(void) {
  setUpManager();
  TEST_ASSERT_FALSE(gm.isThreefoldRepetition());
}

void test_board_threefold_with_rook_moves(void) {
  setUpManager();
  // Repeat rook moves to produce threefold repetition (sufficient material)
  gm.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  gm.makeMove(7, 0, 7, 1); // Ra1-b1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 1, 7, 0); // Rb1-a1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8
  gm.makeMove(7, 0, 7, 1);
  gm.makeMove(0, 4, 0, 3);
  gm.makeMove(7, 1, 7, 0);
  MoveResult r = gm.makeMove(0, 3, 0, 4); // 3rd time
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(gm.isThreefoldRepetition());
  TEST_ASSERT_TRUE(gm.isDraw());
}

void test_board_position_history_reset_on_pawn_move(void) {
  setUpManager();
  // Start with some moves to build position history, then a pawn move resets it
  gm.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  gm.makeMove(7, 4, 7, 3); // Ke1-d1
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  gm.makeMove(7, 3, 7, 4); // Kd1-e1
  gm.makeMove(0, 3, 0, 4); // Kd8-e8 (occurrence 2)
  // Pawn move resets halfmove clock, which resets position history
  gm.makeMove(6, 4, 4, 4); // e2-e4
  gm.makeMove(0, 4, 0, 3); // Ke8-d8
  // Now even if positions repeat, the prior history is gone
  TEST_ASSERT_FALSE(gm.isThreefoldRepetition());
}

// ---------------------------------------------------------------------------
// reverseMove / applyMoveEntry
// ---------------------------------------------------------------------------

// Helper: build a MoveEntry from scratch
static MoveEntry makeBoardEntry(int fr, int fc, int tr, int tc, char piece,
                                char captured, const PositionState& prev,
                                char promo = ' ', bool ep = false,
                                int epRow = -1, bool castle = false,
                                bool check = false) {
  MoveEntry e = {};
  e.fromRow = fr; e.fromCol = fc; e.toRow = tr; e.toCol = tc;
  e.piece = piece; e.captured = captured; e.promotion = promo;
  e.isCapture = (captured != ' ');
  e.isEnPassant = ep; e.epCapturedRow = epRow;
  e.isCastling = castle;
  e.isPromotion = (promo != ' ');
  e.isCheck = check;
  e.prevState = prev;
  return e;
}

void test_board_reverse_move_simple(void) {
  setUpManager();
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(6, 4, 4, 4);  // e2-e4
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_CHAR('b', gm.currentTurn());

  MoveEntry e = makeBoardEntry(6, 4, 4, 4, 'P', ' ', before);
  gm.reverseMove(e);

  TEST_ASSERT_EQUAL_CHAR('P', gm.getSquare(6, 4));  // pawn back on e2
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(4, 4));  // e4 empty
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_board_reverse_move_capture(void) {
  setUpManager();
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 3, 3, 3);  // d5
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(4, 4, 3, 3);  // exd5 capture
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);

  MoveEntry e = makeBoardEntry(4, 4, 3, 3, 'P', 'p', before);
  gm.reverseMove(e);

  TEST_ASSERT_EQUAL_CHAR('P', gm.getSquare(4, 4));  // pawn back on e5
  TEST_ASSERT_EQUAL_CHAR('p', gm.getSquare(3, 3));  // black pawn restored on d5
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_board_reverse_move_en_passant(void) {
  setUpManager();
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 0, 2, 0);  // a6
  gm.makeMove(4, 4, 3, 4);  // e5
  gm.makeMove(1, 3, 3, 3);  // d5
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(3, 4, 2, 3);  // exd6 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);

  MoveEntry e = makeBoardEntry(3, 4, 2, 3, 'P', 'p', before, ' ', true, 3);
  gm.reverseMove(e);

  TEST_ASSERT_EQUAL_CHAR('P', gm.getSquare(3, 4));  // pawn back on e5
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(2, 3));  // d6 empty
  TEST_ASSERT_EQUAL_CHAR('p', gm.getSquare(3, 3));  // black pawn restored on d5
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_board_reverse_move_castling(void) {
  setUpManager();
  gm.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(7, 4, 7, 6);  // O-O white kingside
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);

  MoveEntry e = makeBoardEntry(7, 4, 7, 6, 'K', ' ', before, ' ', false, -1, true);
  gm.reverseMove(e);

  TEST_ASSERT_EQUAL_CHAR('K', gm.getSquare(7, 4));  // king back on e1
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(7, 6));  // g1 empty
  TEST_ASSERT_EQUAL_CHAR('R', gm.getSquare(7, 7));  // rook back on h1
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(7, 5));  // f1 empty
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
  // Castling rights should be restored
  TEST_ASSERT_EQUAL(0x0F, gm.positionState().castlingRights);
}

void test_board_reverse_move_promotion(void) {
  setUpManager();
  gm.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(1, 0, 0, 0, 'q');  // a7-a8=Q
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('Q', gm.getSquare(0, 0));

  MoveEntry e = makeBoardEntry(1, 0, 0, 0, 'P', ' ', before, 'Q');
  gm.reverseMove(e);

  TEST_ASSERT_EQUAL_CHAR('P', gm.getSquare(1, 0));  // pawn back on a7
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(0, 0));  // a8 empty
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_board_reverse_move_clears_game_over(void) {
  setUpManager();
  // Scholar's mate
  gm.makeMove(6, 4, 4, 4);  // e4
  gm.makeMove(1, 4, 3, 4);  // e5
  gm.makeMove(7, 5, 4, 2);  // Bc4
  gm.makeMove(1, 0, 2, 0);  // a6
  gm.makeMove(7, 3, 3, 7);  // Qh5
  gm.makeMove(1, 1, 2, 1);  // b6
  PositionState before = gm.positionState();
  MoveResult r = gm.makeMove(3, 7, 1, 5);  // Qxf7#
  TEST_ASSERT_TRUE(gm.isCheckmate());

  MoveEntry e = makeBoardEntry(3, 7, 1, 5, 'Q', 'p', before, ' ', false, -1, false, true);
  gm.reverseMove(e);

  TEST_ASSERT_FALSE(gm.isCheckmate());
  TEST_ASSERT_EQUAL_CHAR('w', gm.currentTurn());
}

void test_board_apply_move_entry(void) {
  setUpManager();
  PositionState before = gm.positionState();
  MoveEntry e = makeBoardEntry(6, 4, 4, 4, 'P', ' ', before);

  MoveResult r = gm.applyMoveEntry(e);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_EQUAL_CHAR('P', gm.getSquare(4, 4));
  TEST_ASSERT_EQUAL_CHAR(' ', gm.getSquare(6, 4));
  TEST_ASSERT_EQUAL_CHAR('b', gm.currentTurn());
}

void test_board_apply_move_entry_promotion(void) {
  setUpManager();
  gm.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  PositionState before = gm.positionState();
  MoveEntry e = makeBoardEntry(1, 0, 0, 0, 'P', ' ', before, 'Q');

  MoveResult r = gm.applyMoveEntry(e);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('Q', gm.getSquare(0, 0));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_board_tests() {
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

  // Compact move encoding
  RUN_TEST(test_encodeMove_roundtrip);
  RUN_TEST(test_encodeMove_with_promotion);
  RUN_TEST(test_encodeMove_corner_squares);

  // FEN/eval cache
  RUN_TEST(test_manager_fen_cache_consistent);
  RUN_TEST(test_manager_eval_cache_consistent);
  RUN_TEST(test_manager_end_game_preserves_fen);
  RUN_TEST(test_manager_eval_after_capture);

  // Position clocks
  RUN_TEST(test_manager_halfmove_clock_increments);
  RUN_TEST(test_manager_halfmove_clock_resets_on_pawn_move);
  RUN_TEST(test_manager_fullmove_increments_after_black);

  // findPiece
  RUN_TEST(test_manager_find_piece_kings_initial);
  RUN_TEST(test_manager_find_piece_black_pawns_initial);
  RUN_TEST(test_manager_find_piece_not_found);
  RUN_TEST(test_manager_find_piece_multiple_bishops);
  RUN_TEST(test_manager_find_piece_max_limit);

  // inCheck (no-arg)
  RUN_TEST(test_manager_in_check_true);
  RUN_TEST(test_manager_in_check_false);

  // isCheckmate (no-arg)
  RUN_TEST(test_manager_is_checkmate_true);
  RUN_TEST(test_manager_is_checkmate_false);

  // isStalemate (no-arg)
  RUN_TEST(test_manager_is_stalemate_true);
  RUN_TEST(test_manager_is_stalemate_false);

  // isInsufficientMaterial (public)
  RUN_TEST(test_manager_is_insufficient_material_true);
  RUN_TEST(test_manager_is_insufficient_material_false);

  // isAttacked
  RUN_TEST(test_manager_is_attacked_by_white);
  RUN_TEST(test_manager_is_attacked_by_black);
  RUN_TEST(test_manager_is_attacked_empty_square);

  // moveNumber
  RUN_TEST(test_manager_move_number_initial);
  RUN_TEST(test_manager_move_number_after_moves);
  RUN_TEST(test_manager_move_number_from_fen);

  // Threefold repetition
  RUN_TEST(test_board_threefold_repetition);
  RUN_TEST(test_board_threefold_different_castling_rights);
  RUN_TEST(test_board_threefold_not_reached);
  RUN_TEST(test_board_threefold_query);
  RUN_TEST(test_board_threefold_with_rook_moves);
  RUN_TEST(test_board_position_history_reset_on_pawn_move);

  // reverseMove / applyMoveEntry
  RUN_TEST(test_board_reverse_move_simple);
  RUN_TEST(test_board_reverse_move_capture);
  RUN_TEST(test_board_reverse_move_en_passant);
  RUN_TEST(test_board_reverse_move_castling);
  RUN_TEST(test_board_reverse_move_promotion);
  RUN_TEST(test_board_reverse_move_clears_game_over);
  RUN_TEST(test_board_apply_move_entry);
  RUN_TEST(test_board_apply_move_entry_promotion);
}
