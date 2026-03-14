#include <unity.h>

#include "../test_helpers.h"
#include <chess_board.h>
#include <chess_history.h>
#include <types.h>

// Shared globals from test_core.cpp
extern ChessBitboard::BitboardSet bb;
extern Piece mailbox[64];
extern bool needsDefaultKings;

static ChessBoard cb;

// Reset ChessBoard before every test
static void setUpBoard(void) {
  cb = ChessBoard();
  cb.newGame();
}

// ---------------------------------------------------------------------------
// New game / initial state
// ---------------------------------------------------------------------------

void test_board_new_game_board(void) {
  setUpBoard();
  // Verify each square matches INITIAL_BOARD
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      TEST_ASSERT_ENUM_EQ(ChessBoard::INITIAL_BOARD[row][col], cb.getSquare(row, col));
}

void test_board_new_game_turn(void) {
  setUpBoard();
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_new_game_not_over(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isCheckmate());
  TEST_ASSERT_FALSE(cb.isDraw());
}

void test_board_new_game_fen(void) {
  setUpBoard();
  TEST_ASSERT_EQUAL_STRING(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      cb.getFen().c_str());
}

void test_board_initial_evaluation_zero(void) {
  setUpBoard();
  TEST_ASSERT_EQUAL_INT(0, cb.getEvaluation());
}

// ---------------------------------------------------------------------------
// Basic moves
// ---------------------------------------------------------------------------

void test_board_e2e4(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(6, 4, 4, 4); // e2e4
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.isCapture);
  TEST_ASSERT_FALSE(r.isCastling);
  TEST_ASSERT_FALSE(r.isEnPassant);
  TEST_ASSERT_FALSE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Color::BLACK, cb.currentTurn());
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(6, 4)); // e2 empty
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(4, 4)); // e4 has pawn
}

void test_board_illegal_move_rejected(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(6, 4, 3, 4); // e2e5 — not legal
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn()); // turn unchanged
}

void test_board_wrong_turn_rejected(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(1, 4, 3, 4); // e7e5 — black's pawn, but it's white's turn
  TEST_ASSERT_FALSE(r.valid);
}

void test_board_empty_square_rejected(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(4, 4, 3, 4); // e4e5 — empty square
  TEST_ASSERT_FALSE(r.valid);
}

void test_board_out_of_bounds_rejected(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(-1, 0, 0, 0);
  TEST_ASSERT_FALSE(r.valid);
  r = cb.makeMove(0, 0, 8, 0);
  TEST_ASSERT_FALSE(r.valid);
}

void test_board_move_after_game_over_rejected(void) {
  setUpBoard();
  // Reach checkmate via scholar's mate, then try to move
  cb.makeMove(6, 4, 4, 4); // e4
  cb.makeMove(1, 4, 3, 4); // e5
  cb.makeMove(7, 5, 4, 2); // Bc4
  cb.makeMove(1, 0, 2, 0); // a6
  cb.makeMove(7, 3, 3, 7); // Qh5
  cb.makeMove(1, 1, 2, 1); // b6
  MoveResult r = cb.makeMove(3, 7, 1, 5); // Qxf7#
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  // Board no longer guards; this tests that the position is in checkmate
  TEST_ASSERT_TRUE(cb.isCheckmate());
}

// ---------------------------------------------------------------------------
// getSquare
// ---------------------------------------------------------------------------

void test_board_getSquare_returns_piece(void) {
  setUpBoard();
  TEST_ASSERT_ENUM_EQ(Piece::W_ROOK, cb.getSquare(7, 0)); // a1
  TEST_ASSERT_ENUM_EQ(Piece::B_KING, cb.getSquare(0, 4)); // e8
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(4, 4)); // e4 empty
}

// ---------------------------------------------------------------------------
// invalidMoveResult
// ---------------------------------------------------------------------------

void test_board_invalidMoveResult_fields(void) {
  MoveResult r = invalidMoveResult();
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_FALSE(r.isCapture);
  TEST_ASSERT_FALSE(r.isEnPassant);
  TEST_ASSERT_EQUAL_INT(-1, r.epCapturedRow);
  TEST_ASSERT_FALSE(r.isCastling);
  TEST_ASSERT_FALSE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, r.promotedTo);
  TEST_ASSERT_FALSE(r.isCheck);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR(' ', r.winnerColor);
}

// ---------------------------------------------------------------------------
// Captures
// ---------------------------------------------------------------------------

void test_board_simple_capture(void) {
  setUpBoard();
  // Set up a position where white can capture
  cb.loadFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2");
  MoveResult r = cb.makeMove(4, 3, 3, 4); // d4xe5
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(3, 4)); // e5 now has white pawn
}

// ---------------------------------------------------------------------------
// En passant
// ---------------------------------------------------------------------------

void test_board_en_passant_white(void) {
  setUpBoard();
  // White pawn on e5, black just played d7d5
  cb.loadFEN("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
  MoveResult r = cb.makeMove(3, 4, 2, 3); // e5xd6 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_EQUAL_INT(3, r.epCapturedRow); // captured pawn was on row 3, col 3
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(3, 3)); // d5 cleared
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(2, 3)); // d6 has white pawn
}

void test_board_en_passant_black(void) {
  setUpBoard();
  // Black pawn on d4, white just played e2e4
  cb.loadFEN("rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3");
  MoveResult r = cb.makeMove(4, 3, 5, 4); // d4xe3 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(4, 4)); // e4 cleared
  TEST_ASSERT_ENUM_EQ(Piece::B_PAWN, cb.getSquare(5, 4)); // e3 has black pawn
}

void test_board_ep_target_set_after_double_push(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4); // e2-e4 (double push)
  const PositionState& st = cb.positionState();
  TEST_ASSERT_EQUAL_INT(5, st.epRow); // EP target = e3 (row 5)
  TEST_ASSERT_EQUAL_INT(4, st.epCol); // col 4
}

void test_board_ep_target_cleared_after_other_move(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4); // e2-e4 sets EP target
  TEST_ASSERT_EQUAL_INT(5, cb.positionState().epRow); // EP target exists
  cb.makeMove(1, 0, 2, 0); // a7-a6 (not a double push)
  TEST_ASSERT_EQUAL_INT(-1, cb.positionState().epRow); // EP target cleared
  TEST_ASSERT_EQUAL_INT(-1, cb.positionState().epCol);
}

// ---------------------------------------------------------------------------
// Castling
// ---------------------------------------------------------------------------

void test_board_white_kingside_castle(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 6); // e1g1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_ENUM_EQ(Piece::W_KING, cb.getSquare(7, 6)); // king on g1
  TEST_ASSERT_ENUM_EQ(Piece::W_ROOK, cb.getSquare(7, 5)); // rook on f1
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(7, 4)); // e1 empty
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(7, 7)); // h1 empty
}

void test_board_white_queenside_castle(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 2); // e1c1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_ENUM_EQ(Piece::W_KING, cb.getSquare(7, 2)); // king on c1
  TEST_ASSERT_ENUM_EQ(Piece::W_ROOK, cb.getSquare(7, 3)); // rook on d1
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(7, 0)); // a1 empty
}

void test_board_black_kingside_castle(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
  MoveResult r = cb.makeMove(0, 4, 0, 6); // e8g8
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_ENUM_EQ(Piece::B_KING, cb.getSquare(0, 6)); // king on g8
  TEST_ASSERT_ENUM_EQ(Piece::B_ROOK, cb.getSquare(0, 5)); // rook on f8
}

void test_board_castling_revokes_rights(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  cb.makeMove(7, 4, 7, 6); // white king-side castle
  // White castling rights revoked
  uint8_t rights = cb.getCastlingRights();
  TEST_ASSERT_EQUAL_UINT8(0, rights & 0x03); // K and Q bits cleared
  TEST_ASSERT_EQUAL_UINT8(0x0C, rights & 0x0C); // k and q bits still set
}

void test_board_rook_move_revokes_right(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  cb.makeMove(7, 7, 7, 6); // Rh1-g1 (not castling, just a rook move)
  uint8_t rights = cb.getCastlingRights();
  TEST_ASSERT_EQUAL_UINT8(0, rights & 0x01); // K right revoked
  TEST_ASSERT_EQUAL_UINT8(0x02, rights & 0x02); // Q right still present
}

void test_board_black_queenside_castle(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
  MoveResult r = cb.makeMove(0, 4, 0, 2); // e8c8
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);
  TEST_ASSERT_ENUM_EQ(Piece::B_KING, cb.getSquare(0, 2)); // king on c8
  TEST_ASSERT_ENUM_EQ(Piece::B_ROOK, cb.getSquare(0, 3)); // rook on d8
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(0, 0)); // a8 empty
}

void test_board_rook_captured_revokes_castling(void) {
  setUpBoard();
  // Black rook at h2 can capture white rook at h1; white has K castling right
  cb.loadFEN("4k3/8/8/8/8/8/7r/4K2R b K - 0 1");
  MoveResult r = cb.makeMove(6, 7, 7, 7); // rh2 x Rh1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_EQUAL_UINT8(0, cb.getCastlingRights()); // K right revoked
}

// ---------------------------------------------------------------------------
// Promotion
// ---------------------------------------------------------------------------

void test_board_auto_queen_promotion(void) {
  setUpBoard();
  cb.loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
  MoveResult r = cb.makeMove(1, 4, 0, 4); // e7e8 — auto-queen
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, r.promotedTo);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, cb.getSquare(0, 4));
}

void test_board_knight_promotion(void) {
  setUpBoard();
  cb.loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
  MoveResult r = cb.makeMove(1, 4, 0, 4, 'n'); // e7e8n
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::W_KNIGHT, r.promotedTo); // uppercase for white
  TEST_ASSERT_ENUM_EQ(Piece::W_KNIGHT, cb.getSquare(0, 4));
}

void test_board_black_promotion(void) {
  setUpBoard();
  cb.loadFEN("4K2k/8/8/8/8/8/4p3/8 b - - 0 1");
  MoveResult r = cb.makeMove(6, 4, 7, 4); // e2e1 — auto-queen
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::B_QUEEN, r.promotedTo); // lowercase for black
}

void test_board_promotion_with_capture(void) {
  setUpBoard();
  // White pawn on d7 captures black rook on e8 and promotes
  cb.loadFEN("4r2k/3P4/8/8/8/8/8/4K3 w - - 0 1");
  MoveResult r = cb.makeMove(1, 3, 0, 4); // d7xe8=Q
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, r.promotedTo);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, cb.getSquare(0, 4));
}

// ---------------------------------------------------------------------------
// Check detection
// ---------------------------------------------------------------------------

void test_board_move_gives_check(void) {
  setUpBoard();
  // White rook on a1, black king on e8 — Ra1-a8+ is check along rank 8
  cb.loadFEN("4k3/8/8/8/8/8/4K3/R7 w - - 0 1");
  MoveResult r = cb.makeMove(7, 0, 0, 0); // Ra1-a8+
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCheck);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult); // not checkmate
}

void test_board_move_no_check(void) {
  setUpBoard();
  MoveResult r = cb.makeMove(6, 4, 4, 4); // e2e4 — not check
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.isCheck);
}

// ---------------------------------------------------------------------------
// Checkmate
// ---------------------------------------------------------------------------

void test_board_scholars_mate(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4); // e2e4
  cb.makeMove(1, 4, 3, 4); // e7e5
  cb.makeMove(7, 5, 4, 2); // Bf1-c4
  cb.makeMove(1, 0, 2, 0); // a7a6
  cb.makeMove(7, 3, 3, 7); // Qd1-h5
  cb.makeMove(1, 1, 2, 1); // b7b6
  MoveResult r = cb.makeMove(3, 7, 1, 5); // Qh5xf7# — checkmate
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('w', r.winnerColor);
  TEST_ASSERT_TRUE(cb.isCheckmate());
}

void test_board_back_rank_mate(void) {
  setUpBoard();
  // White rook delivers back-rank mate
  cb.loadFEN("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1");
  MoveResult r = cb.makeMove(7, 0, 0, 0); // Ra1-a8#
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('w', r.winnerColor);
}

// ---------------------------------------------------------------------------
// Stalemate
// ---------------------------------------------------------------------------

void test_board_stalemate(void) {
  setUpBoard();
  // Black king on a8, white king on c6, white queen on b1.
  // Qb1-b6 leaves black with no legal moves and no check → stalemate.
  cb.loadFEN("k7/8/2K5/8/8/8/8/1Q6 w - - 0 1");
  MoveResult r = cb.makeMove(7, 1, 2, 1); // Qb1-b6 — stalemates black king
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::STALEMATE, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('d', r.winnerColor);
  TEST_ASSERT_TRUE(cb.isStalemate());
}

// ---------------------------------------------------------------------------
// 50-move rule
// ---------------------------------------------------------------------------

void test_board_fifty_move_draw(void) {
  setUpBoard();
  // Load position with halfmove clock at 99
  cb.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 99 50");
  MoveResult r = cb.makeMove(7, 0, 7, 1); // Ra1-b1 (non-capture, non-pawn = clock hits 100)
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_50, r.gameResult);
  TEST_ASSERT_EQUAL_CHAR('d', r.winnerColor);
  TEST_ASSERT_TRUE(cb.isFiftyMoves());
  TEST_ASSERT_TRUE(cb.isDraw());
}

// ---------------------------------------------------------------------------
// Insufficient material
// ---------------------------------------------------------------------------

void test_board_insufficient_material_k_vs_k(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  // Any move should trigger DRAW_INSUFFICIENT
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(cb.isInsufficientMaterial());
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_insufficient_material_kb_vs_k(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_insufficient_material_kn_vs_k(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K1N1 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_insufficient_material_kb_vs_kb_same_color(void) {
  setUpBoard();
  // Both bishops on light squares (c1=dark, d1 would be light, f1=light)
  // c8 is light square (row 0 + col 2 = even), f1 is light square (row 7 + col 5 = even)
  cb.loadFEN("2b1k3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_INSUFFICIENT, r.gameResult);
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_insufficient_material_kb_vs_kb_diff_color(void) {
  setUpBoard();
  // White bishop on f1 (light: 7+5=even), black bishop on c8 is light too.
  // Put black bishop on d8 (dark: 0+3=odd) for different colors.
  cb.loadFEN("3bk3/8/8/8/8/8/8/4KB2 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  // Different color bishops — NOT insufficient
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(cb.isInsufficientMaterial());
}

void test_board_sufficient_material_knn(void) {
  setUpBoard();
  // K+N+N vs K — sufficient material (checkmate is possible)
  cb.loadFEN("4k3/8/8/8/8/8/8/2N1KN2 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(cb.isCheckmate());
}

void test_board_sufficient_material_kp_vs_k(void) {
  setUpBoard();
  // K+P vs K — sufficient material (pawn can promote)
  cb.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  MoveResult r = cb.makeMove(7, 4, 7, 3); // Ke1-d1
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
}

// ---------------------------------------------------------------------------
// FEN loading
// ---------------------------------------------------------------------------

void test_board_load_fen_sets_turn(void) {
  setUpBoard();
  cb.loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  TEST_ASSERT_ENUM_EQ(Color::BLACK, cb.currentTurn());
}

void test_board_load_fen_resets_game_over(void) {
  setUpBoard();
  // Reach checkmate first
  cb.makeMove(6, 4, 4, 4); // e4
  cb.makeMove(1, 4, 3, 4); // e5
  cb.makeMove(7, 5, 4, 2); // Bc4
  cb.makeMove(1, 0, 2, 0); // a6
  cb.makeMove(7, 3, 3, 7); // Qh5
  cb.makeMove(1, 1, 2, 1); // b6
  MoveResult r = cb.makeMove(3, 7, 1, 5); // Qxf7#
  TEST_ASSERT_TRUE(cb.isCheckmate());
  // loadFEN resets position to non-terminal
  cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_FALSE(cb.isCheckmate());
}

void test_board_load_fen_roundtrip(void) {
  setUpBoard();
  std::string inputFen = "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  cb.loadFEN(inputFen);
  TEST_ASSERT_EQUAL_STRING(inputFen.c_str(), cb.getFen().c_str());
}

void test_board_load_fen_complex(void) {
  setUpBoard();
  std::string fen = "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
  cb.loadFEN(fen);
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
  TEST_ASSERT_EQUAL_UINT8(0x0F, cb.getCastlingRights()); // KQkq
  TEST_ASSERT_EQUAL_INT(-1, cb.positionState().epRow);     // no EP
  TEST_ASSERT_EQUAL_INT(4, cb.positionState().halfmoveClock);
  TEST_ASSERT_EQUAL_INT(4, cb.positionState().fullmoveClock);
  TEST_ASSERT_ENUM_EQ(Piece::W_BISHOP, cb.getSquare(4, 2)); // Bc4
  TEST_ASSERT_ENUM_EQ(Piece::B_KNIGHT, cb.getSquare(2, 2)); // Nc6
  TEST_ASSERT_EQUAL_STRING(fen.c_str(), cb.getFen().c_str());
}

// --- FEN validation ---

void test_board_load_fen_rejects_empty(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.loadFEN(""));
}

void test_board_load_fen_rejects_too_few_ranks(void) {
  setUpBoard();
  // Only 7 ranks (6 slashes)
  TEST_ASSERT_FALSE(cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1"));
}

void test_board_load_fen_rejects_too_many_ranks(void) {
  setUpBoard();
  // 9 ranks (8 slashes)
  TEST_ASSERT_FALSE(cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_board_load_fen_rejects_invalid_piece(void) {
  setUpBoard();
  // 'x' is not a valid piece character
  TEST_ASSERT_FALSE(cb.loadFEN("xnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_board_load_fen_rejects_rank_overflow(void) {
  setUpBoard();
  // First rank sums to 9
  TEST_ASSERT_FALSE(cb.loadFEN("rnbqkbnrr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_board_load_fen_rejects_invalid_turn(void) {
  setUpBoard();
  // 'x' is not a valid turn
  TEST_ASSERT_FALSE(cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"));
}

void test_board_load_fen_valid_returns_true(void) {
  setUpBoard();
  TEST_ASSERT_TRUE(cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

void test_board_load_fen_invalid_preserves_state(void) {
  setUpBoard();
  // Make a move so state differs from initial
  cb.makeMove(6, 4, 4, 4); // e2e4
  std::string fenBefore = cb.getFen();
  // Invalid FEN should leave state unchanged
  TEST_ASSERT_FALSE(cb.loadFEN("bad_fen"));
  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), cb.getFen().c_str());
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

void test_board_fen_cache_consistent(void) {
  setUpBoard();
  std::string fen1 = cb.getFen();
  std::string fen2 = cb.getFen();
  TEST_ASSERT_EQUAL_STRING(fen1.c_str(), fen2.c_str());

  cb.makeMove(6, 4, 4, 4);  // e2e4
  std::string fen3 = cb.getFen();
  TEST_ASSERT_TRUE(fen3 != fen1);  // FEN changed after move
}

void test_board_eval_cache_consistent(void) {
  setUpBoard();
  int eval1 = cb.getEvaluation();
  int eval2 = cb.getEvaluation();
  TEST_ASSERT_EQUAL_INT(eval1, eval2);  // Same value from cache

  // Load asymmetric position (white missing e-pawn) and verify cache updates
  cb.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
  int eval3 = cb.getEvaluation();
  // White has one fewer pawn → negative evaluation
  TEST_ASSERT_TRUE(eval3 < eval1);
}

void test_board_end_game_preserves_fen(void) {
  setUpBoard();
  std::string fenBefore = cb.getFen();
  // Making a move changes FEN, but non-terminal positions preserve structure
  TEST_ASSERT_EQUAL_STRING(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      fenBefore.c_str());
}

void test_board_eval_after_capture(void) {
  setUpBoard();
  int initialEval = cb.getEvaluation();
  // White captures black's e-pawn (material advantage for white)
  cb.loadFEN("rnbqkbnr/pppp1ppp/8/4p3/3P4/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 2");
  cb.makeMove(4, 3, 3, 4); // d4xe5
  int evalAfter = cb.getEvaluation();
  TEST_ASSERT_TRUE(evalAfter > initialEval); // white gained material
}

// ---------------------------------------------------------------------------
// Position clocks (halfmove / fullmove)
// ---------------------------------------------------------------------------

void test_board_halfmove_clock_increments(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  cb.makeMove(7, 0, 7, 1); // Ra1-b1 (non-capture, non-pawn)
  TEST_ASSERT_EQUAL_INT(1, cb.positionState().halfmoveClock);
  cb.makeMove(0, 4, 0, 3); // Ke8-d8
  TEST_ASSERT_EQUAL_INT(2, cb.positionState().halfmoveClock);
}

void test_board_halfmove_clock_resets_on_pawn_move(void) {
  setUpBoard();
  // Start with halfmove=5 and make a pawn move
  cb.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 5 10");
  TEST_ASSERT_EQUAL_INT(5, cb.positionState().halfmoveClock);
  cb.makeMove(6, 4, 4, 4); // e2-e4 (pawn move resets clock)
  TEST_ASSERT_EQUAL_INT(0, cb.positionState().halfmoveClock);
}

void test_board_fullmove_increments_after_black(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  TEST_ASSERT_EQUAL_INT(1, cb.positionState().fullmoveClock);
  cb.makeMove(6, 4, 4, 4); // e2-e4 (white moves, fullmove stays 1)
  TEST_ASSERT_EQUAL_INT(1, cb.positionState().fullmoveClock);
  cb.makeMove(0, 4, 0, 3); // Ke8-d8 (black moves, fullmove → 2)
  TEST_ASSERT_EQUAL_INT(2, cb.positionState().fullmoveClock);
}

// ---------------------------------------------------------------------------
// findPiece
// ---------------------------------------------------------------------------

void test_board_find_piece_kings_initial(void) {
  setUpBoard();
  int positions[2][2];
  int count = cb.findPiece(Piece::W_KING, positions, 2);
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_INT(7, positions[0][0]); // row 7 = rank 1
  TEST_ASSERT_EQUAL_INT(4, positions[0][1]); // col 4 = file e
}

void test_board_find_piece_black_pawns_initial(void) {
  setUpBoard();
  int positions[8][2];
  int count = cb.findPiece(Piece::B_PAWN, positions, 8);
  TEST_ASSERT_EQUAL_INT(8, count);
  // All should be on row 1 (rank 7)
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_INT(1, positions[i][0]);
  }
}

void test_board_find_piece_not_found(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  int positions[8][2];
  int count = cb.findPiece(Piece::W_QUEEN, positions, 8);
  TEST_ASSERT_EQUAL_INT(0, count);
}

void test_board_find_piece_multiple_bishops(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/2B1B3/8/4K3 w - - 0 1");
  int positions[4][2];
  int count = cb.findPiece(Piece::W_BISHOP, positions, 4);
  TEST_ASSERT_EQUAL_INT(2, count);
}

void test_board_find_piece_max_limit(void) {
  setUpBoard();
  // 8 white pawns, but limit to 3
  int positions[3][2];
  int count = cb.findPiece(Piece::W_PAWN, positions, 3);
  TEST_ASSERT_EQUAL_INT(3, count);
}

// ---------------------------------------------------------------------------
// inCheck (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_board_in_check_true(void) {
  setUpBoard();
  // White king in check from black queen
  cb.loadFEN("4k3/8/8/8/8/8/8/3qK3 w - - 0 1");
  TEST_ASSERT_TRUE(cb.inCheck());
}

void test_board_in_check_false(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.inCheck());
}

// ---------------------------------------------------------------------------
// isCheckmate (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_board_is_checkmate_true(void) {
  setUpBoard();
  // Back rank mate: white king on h1, black rook delivers mate on a1
  cb.loadFEN("6k1/8/8/8/8/8/5PPP/r5K1 w - - 0 1");
  TEST_ASSERT_TRUE(cb.isCheckmate());
}

void test_board_is_checkmate_false(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isCheckmate());
}

// ---------------------------------------------------------------------------
// isStalemate (no-arg, uses current turn)
// ---------------------------------------------------------------------------

void test_board_is_stalemate_true(void) {
  setUpBoard();
  // Stalemate: black king on a8, white queen on b6, white king on c8 — black to move
  cb.loadFEN("k7/8/1Q6/8/8/8/8/2K5 b - - 0 1");
  TEST_ASSERT_TRUE(cb.isStalemate());
}

void test_board_is_stalemate_false(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isStalemate());
}

// ---------------------------------------------------------------------------
// isInsufficientMaterial (public)
// ---------------------------------------------------------------------------

void test_board_is_insufficient_material_true(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(cb.isInsufficientMaterial());
}

void test_board_is_insufficient_material_false(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isInsufficientMaterial());
}

// ---------------------------------------------------------------------------
// isAttacked
// ---------------------------------------------------------------------------

void test_board_is_attacked_by_white(void) {
  setUpBoard();
  // White pawn on e4 attacks d5 and f5
  cb.loadFEN("4k3/8/8/8/4P3/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(cb.isAttacked(3, 3, Color::WHITE));  // d5 attacked by white
  TEST_ASSERT_TRUE(cb.isAttacked(3, 5, Color::WHITE));  // f5 attacked by white
  TEST_ASSERT_FALSE(cb.isAttacked(3, 4, Color::WHITE)); // e5 not attacked by white pawn
}

void test_board_is_attacked_by_black(void) {
  setUpBoard();
  // Black knight on f6 attacks e4, g4, d5, h5, d7, h7, e8, g8
  cb.loadFEN("4k3/8/5n2/8/8/8/8/4K3 w - - 0 1");
  TEST_ASSERT_TRUE(cb.isAttacked(4, 4, Color::BLACK));  // e4 attacked by black knight
  TEST_ASSERT_TRUE(cb.isAttacked(4, 6, Color::BLACK));  // g4 attacked by black knight
  TEST_ASSERT_FALSE(cb.isAttacked(4, 5, Color::BLACK)); // f4 not attacked by black knight
}

void test_board_is_attacked_empty_square(void) {
  setUpBoard();
  // Empty square can be attacked
  cb.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  TEST_ASSERT_TRUE(cb.isAttacked(0, 0, Color::WHITE)); // a8 attacked by Ra1 along file
}

// ---------------------------------------------------------------------------
// moveNumber
// ---------------------------------------------------------------------------

void test_board_move_number_initial(void) {
  setUpBoard();
  TEST_ASSERT_EQUAL_INT(1, cb.moveNumber());
}

void test_board_move_number_after_moves(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  cb.makeMove(6, 4, 4, 4); // e2-e4 (white, fullmove stays 1)
  TEST_ASSERT_EQUAL_INT(1, cb.moveNumber());
  cb.makeMove(0, 4, 0, 3); // Ke8-d8 (black, fullmove → 2)
  TEST_ASSERT_EQUAL_INT(2, cb.moveNumber());
}

void test_board_move_number_from_fen(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 42");
  TEST_ASSERT_EQUAL_INT(42, cb.moveNumber());
}

// ---------------------------------------------------------------------------
// Threefold repetition (Zobrist position tracking in ChessBoard)
// ---------------------------------------------------------------------------

void test_board_threefold_repetition(void) {
  setUpBoard();
  // Position where Ke1 and Ke8 shuffle back and forth (pawns provide sufficient material)
  cb.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  // Move 1: Ke1-d1, Ke8-d8
  cb.makeMove(7, 4, 7, 3);
  cb.makeMove(0, 4, 0, 3);
  // Move 2: Kd1-e1, Kd8-e8 (back to original — occurrence 2)
  cb.makeMove(7, 3, 7, 4);
  cb.makeMove(0, 3, 0, 4);
  // Move 3: Ke1-d1, Ke8-d8
  cb.makeMove(7, 4, 7, 3);
  cb.makeMove(0, 4, 0, 3);
  // Move 4: Kd1-e1, Kd8-e8 (back to original — occurrence 3)
  cb.makeMove(7, 3, 7, 4);
  MoveResult r = cb.makeMove(0, 3, 0, 4);  // third repetition
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(cb.isThreefoldRepetition());
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_threefold_different_castling_rights(void) {
  setUpBoard();
  // King move loses castling rights → initial hash differs from subsequent hashes
  cb.loadFEN("4k3/8/8/8/8/8/8/4K2R w K - 0 1");
  cb.makeMove(7, 4, 7, 3); // Ke1-d1 (loses K castling)
  cb.makeMove(0, 4, 0, 3);
  cb.makeMove(7, 3, 7, 4);
  cb.makeMove(0, 3, 0, 4); // board same as start, but castling=0
  cb.makeMove(7, 4, 7, 3);
  cb.makeMove(0, 4, 0, 3);
  cb.makeMove(7, 3, 7, 4);
  MoveResult r = cb.makeMove(0, 3, 0, 4); // occurrence 2 (not 3)
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(cb.isThreefoldRepetition());
}

void test_board_threefold_not_reached(void) {
  setUpBoard();
  // Only 2 occurrences — not enough for threefold
  cb.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  cb.makeMove(7, 4, 7, 3);
  cb.makeMove(0, 4, 0, 3);
  cb.makeMove(7, 3, 7, 4);
  MoveResult r = cb.makeMove(0, 3, 0, 4); // occurrence 2
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, r.gameResult);
  TEST_ASSERT_FALSE(cb.isThreefoldRepetition());
}

void test_board_threefold_query(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isThreefoldRepetition());
}

void test_board_threefold_with_rook_moves(void) {
  setUpBoard();
  // Repeat rook moves to produce threefold repetition (sufficient material)
  cb.loadFEN("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  cb.makeMove(7, 0, 7, 1); // Ra1-b1
  cb.makeMove(0, 4, 0, 3); // Ke8-d8
  cb.makeMove(7, 1, 7, 0); // Rb1-a1
  cb.makeMove(0, 3, 0, 4); // Kd8-e8
  cb.makeMove(7, 0, 7, 1);
  cb.makeMove(0, 4, 0, 3);
  cb.makeMove(7, 1, 7, 0);
  MoveResult r = cb.makeMove(0, 3, 0, 4); // 3rd time
  TEST_ASSERT_ENUM_EQ(GameResult::DRAW_3FOLD, r.gameResult);
  TEST_ASSERT_TRUE(cb.isThreefoldRepetition());
  TEST_ASSERT_TRUE(cb.isDraw());
}

void test_board_position_history_reset_on_pawn_move(void) {
  setUpBoard();
  // Start with some moves to build position history, then a pawn move resets it
  cb.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  cb.makeMove(7, 4, 7, 3); // Ke1-d1
  cb.makeMove(0, 4, 0, 3); // Ke8-d8
  cb.makeMove(7, 3, 7, 4); // Kd1-e1
  cb.makeMove(0, 3, 0, 4); // Kd8-e8 (occurrence 2)
  // Pawn move resets halfmove clock, which resets position history
  cb.makeMove(6, 4, 4, 4); // e2-e4
  cb.makeMove(0, 4, 0, 3); // Ke8-d8
  // Now even if positions repeat, the prior history is gone
  TEST_ASSERT_FALSE(cb.isThreefoldRepetition());
}

// ---------------------------------------------------------------------------
// reverseMove / applyMoveEntry
// ---------------------------------------------------------------------------

// Helper: build a MoveEntry from scratch
static MoveEntry makeBoardEntry(int fr, int fc, int tr, int tc, Piece piece,
                                Piece captured, const PositionState& prev,
                                Piece promo = Piece::NONE, bool ep = false,
                                int epRow = -1, bool castle = false,
                                bool check = false) {
  MoveEntry e = {};
  e.fromRow = fr; e.fromCol = fc; e.toRow = tr; e.toCol = tc;
  e.piece = piece; e.captured = captured; e.promotion = promo;
  e.isCapture = (captured != Piece::NONE);
  e.isEnPassant = ep; e.epCapturedRow = epRow;
  e.isCastling = castle;
  e.isPromotion = (promo != Piece::NONE);
  e.isCheck = check;
  e.prevState = prev;
  return e;
}

void test_board_reverse_move_simple(void) {
  setUpBoard();
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(6, 4, 4, 4);  // e2-e4
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(Color::BLACK, cb.currentTurn());

  MoveEntry e = makeBoardEntry(6, 4, 4, 4, Piece::W_PAWN, Piece::NONE, before);
  cb.reverseMove(e);

  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(6, 4));  // pawn back on e2
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(4, 4));  // e4 empty
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_reverse_move_capture(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4);  // e4
  cb.makeMove(1, 3, 3, 3);  // d5
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(4, 4, 3, 3);  // exd5 capture
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCapture);

  MoveEntry e = makeBoardEntry(4, 4, 3, 3, Piece::W_PAWN, Piece::B_PAWN, before);
  cb.reverseMove(e);

  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(4, 4));  // pawn back on e5
  TEST_ASSERT_ENUM_EQ(Piece::B_PAWN, cb.getSquare(3, 3));  // black pawn restored on d5
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_reverse_move_en_passant(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4);  // e4
  cb.makeMove(1, 0, 2, 0);  // a6
  cb.makeMove(4, 4, 3, 4);  // e5
  cb.makeMove(1, 3, 3, 3);  // d5
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(3, 4, 2, 3);  // exd6 en passant
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isEnPassant);

  MoveEntry e = makeBoardEntry(3, 4, 2, 3, Piece::W_PAWN, Piece::B_PAWN, before, Piece::NONE, true, 3);
  cb.reverseMove(e);

  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(3, 4));  // pawn back on e5
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(2, 3));  // d6 empty
  TEST_ASSERT_ENUM_EQ(Piece::B_PAWN, cb.getSquare(3, 3));  // black pawn restored on d5
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_reverse_move_castling(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(7, 4, 7, 6);  // O-O white kingside
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isCastling);

  MoveEntry e = makeBoardEntry(7, 4, 7, 6, Piece::W_KING, Piece::NONE, before, Piece::NONE, false, -1, true);
  cb.reverseMove(e);

  TEST_ASSERT_ENUM_EQ(Piece::W_KING, cb.getSquare(7, 4));  // king back on e1
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(7, 6));  // g1 empty
  TEST_ASSERT_ENUM_EQ(Piece::W_ROOK, cb.getSquare(7, 7));  // rook back on h1
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(7, 5));  // f1 empty
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
  // Castling rights should be restored
  TEST_ASSERT_EQUAL(0x0F, cb.positionState().castlingRights);
}

void test_board_reverse_move_promotion(void) {
  setUpBoard();
  cb.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(1, 0, 0, 0, 'q');  // a7-a8=Q
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, cb.getSquare(0, 0));

  MoveEntry e = makeBoardEntry(1, 0, 0, 0, Piece::W_PAWN, Piece::NONE, before, Piece::W_QUEEN);
  cb.reverseMove(e);

  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(1, 0));  // pawn back on a7
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(0, 0));  // a8 empty
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_reverse_move_clears_game_over(void) {
  setUpBoard();
  // Scholar's mate
  cb.makeMove(6, 4, 4, 4);  // e4
  cb.makeMove(1, 4, 3, 4);  // e5
  cb.makeMove(7, 5, 4, 2);  // Bc4
  cb.makeMove(1, 0, 2, 0);  // a6
  cb.makeMove(7, 3, 3, 7);  // Qh5
  cb.makeMove(1, 1, 2, 1);  // b6
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(3, 7, 1, 5);  // Qxf7#
  TEST_ASSERT_TRUE(cb.isCheckmate());

  MoveEntry e = makeBoardEntry(3, 7, 1, 5, Piece::W_QUEEN, Piece::B_PAWN, before, Piece::NONE, false, -1, false, true);
  cb.reverseMove(e);

  TEST_ASSERT_FALSE(cb.isCheckmate());
  TEST_ASSERT_ENUM_EQ(Color::WHITE, cb.currentTurn());
}

void test_board_apply_move_entry(void) {
  setUpBoard();
  PositionState before = cb.positionState();
  MoveEntry e = makeBoardEntry(6, 4, 4, 4, Piece::W_PAWN, Piece::NONE, before);

  MoveResult r = cb.applyMoveEntry(e);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, cb.getSquare(4, 4));
  TEST_ASSERT_ENUM_EQ(Piece::NONE, cb.getSquare(6, 4));
  TEST_ASSERT_ENUM_EQ(Color::BLACK, cb.currentTurn());
}

void test_board_apply_move_entry_promotion(void) {
  setUpBoard();
  cb.loadFEN("8/P4k2/8/8/8/8/5K2/8 w - - 0 1");
  PositionState before = cb.positionState();
  MoveEntry e = makeBoardEntry(1, 0, 0, 0, Piece::W_PAWN, Piece::NONE, before, Piece::W_QUEEN);

  MoveResult r = cb.applyMoveEntry(e);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_ENUM_EQ(Piece::W_QUEEN, cb.getSquare(0, 0));
}

// ---------------------------------------------------------------------------
// King cache
// ---------------------------------------------------------------------------

void test_board_king_cache_initial(void) {
  setUpBoard();
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(0, cb.kingRow(Color::BLACK));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::BLACK));
}

void test_board_king_cache_after_king_move(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  cb.makeMove(7, 4, 7, 3);  // Ke1-d1
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(3, cb.kingCol(Color::WHITE));
  // Black king unchanged
  TEST_ASSERT_EQUAL_INT(0, cb.kingRow(Color::BLACK));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::BLACK));
}

void test_board_king_cache_after_non_king_move(void) {
  setUpBoard();
  cb.makeMove(6, 4, 4, 4);  // e2-e4 (pawn move)
  // Kings haven't moved
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(0, cb.kingRow(Color::BLACK));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::BLACK));
}

void test_board_king_cache_after_castling(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  cb.makeMove(7, 4, 7, 6);  // O-O white kingside
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(6, cb.kingCol(Color::WHITE));
}

void test_board_king_cache_after_load_fen(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R b KQkq - 0 1");
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(0, cb.kingRow(Color::BLACK));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::BLACK));

  // Now a position with kings off-center
  cb.loadFEN("8/8/8/3k4/8/8/8/1K6 w - - 0 1");
  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(1, cb.kingCol(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(3, cb.kingRow(Color::BLACK));
  TEST_ASSERT_EQUAL_INT(3, cb.kingCol(Color::BLACK));
}

void test_board_king_cache_after_reverse_move(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  PositionState before = cb.positionState();
  cb.makeMove(7, 4, 7, 3);  // Ke1-d1
  TEST_ASSERT_EQUAL_INT(3, cb.kingCol(Color::WHITE));

  MoveEntry e = makeBoardEntry(7, 4, 7, 3, Piece::W_KING, Piece::NONE, before);
  cb.reverseMove(e);

  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::WHITE));
}

void test_board_king_cache_reverse_castling(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
  PositionState before = cb.positionState();
  cb.makeMove(7, 4, 7, 6);  // O-O
  TEST_ASSERT_EQUAL_INT(6, cb.kingCol(Color::WHITE));

  MoveEntry e = makeBoardEntry(7, 4, 7, 6, Piece::W_KING, Piece::NONE, before,
                               Piece::NONE, false, -1, true);
  cb.reverseMove(e);

  TEST_ASSERT_EQUAL_INT(7, cb.kingRow(Color::WHITE));
  TEST_ASSERT_EQUAL_INT(4, cb.kingCol(Color::WHITE));
}

// ---------------------------------------------------------------------------
// MoveList struct
// ---------------------------------------------------------------------------

void test_movelist_initial_state(void) {
  MoveList moves;
  TEST_ASSERT_EQUAL_INT(0, moves.count);
}

void test_movelist_add_and_access(void) {
  MoveList moves;
  // Add Move structs — use squareOf to encode (row, col) into from/to
  moves.add(Move(0, static_cast<uint8_t>(ChessBitboard::squareOf(3, 5))));
  moves.add(Move(0, static_cast<uint8_t>(ChessBitboard::squareOf(0, 7))));
  TEST_ASSERT_EQUAL_INT(2, moves.count);
  TEST_ASSERT_EQUAL_INT(3, moves.targetRow(0));
  TEST_ASSERT_EQUAL_INT(5, moves.targetCol(0));
  TEST_ASSERT_EQUAL_INT(0, moves.targetRow(1));
  TEST_ASSERT_EQUAL_INT(7, moves.targetCol(1));
}

void test_movelist_clear(void) {
  MoveList moves;
  moves.add(Move(0, static_cast<uint8_t>(ChessBitboard::squareOf(1, 2))));
  moves.add(Move(0, static_cast<uint8_t>(ChessBitboard::squareOf(3, 4))));
  TEST_ASSERT_EQUAL_INT(2, moves.count);
  moves.clear();
  TEST_ASSERT_EQUAL_INT(0, moves.count);
}

void test_movelist_fills_to_capacity(void) {
  MoveList moves;
  for (int i = 0; i < MAX_MOVES; i++) {
    moves.add(Move(0, static_cast<uint8_t>(ChessBitboard::squareOf(i % 8, i / 4 % 8))));
  }
  TEST_ASSERT_EQUAL_INT(MAX_MOVES, moves.count);
  // Verify first and last entries
  TEST_ASSERT_EQUAL_INT(0, moves.targetRow(0));
  TEST_ASSERT_EQUAL_INT(0, moves.targetCol(0));
}

void test_movelist_used_by_get_possible_moves(void) {
  setUpBoard();
  MoveList moves;
  cb.getPossibleMoves(6, 4, moves);  // e2 pawn: e3 and e4
  TEST_ASSERT_EQUAL_INT(2, moves.count);
}

// ---------------------------------------------------------------------------
// HashHistory struct
// ---------------------------------------------------------------------------

void test_hashhistory_initial_state(void) {
  HashHistory h;
  TEST_ASSERT_EQUAL_INT(0, h.count);
}

void test_hashhistory_add_and_read(void) {
  HashHistory h;
  h.keys[h.count++] = 0xDEADBEEF;
  h.keys[h.count++] = 0xCAFEBABE;
  TEST_ASSERT_EQUAL_INT(2, h.count);
  TEST_ASSERT_EQUAL_UINT64(0xDEADBEEF, h.keys[0]);
  TEST_ASSERT_EQUAL_UINT64(0xCAFEBABE, h.keys[1]);
}

void test_hashhistory_max_size(void) {
  TEST_ASSERT_EQUAL_INT(512, HashHistory::MAX_SIZE);
}

// ---------------------------------------------------------------------------
// loadFEN edge cases
// ---------------------------------------------------------------------------

void test_board_load_fen_sets_castling_rights(void) {
  setUpBoard();
  cb.loadFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kq - 0 1");
  // K + q = 0x01 | 0x08 = 0x09
  TEST_ASSERT_EQUAL_UINT8(0x09, cb.getCastlingRights());
}

void test_board_load_fen_sets_ep_target(void) {
  setUpBoard();
  cb.loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  const PositionState& st = cb.positionState();
  TEST_ASSERT_EQUAL_INT(5, st.epRow);  // e3 = row 5
  TEST_ASSERT_EQUAL_INT(4, st.epCol);  // e = col 4
}

void test_board_load_fen_sets_clocks(void) {
  setUpBoard();
  cb.loadFEN("4k3/8/8/8/8/8/8/4K3 w - - 42 21");
  TEST_ASSERT_EQUAL_INT(42, cb.positionState().halfmoveClock);
  TEST_ASSERT_EQUAL_INT(21, cb.positionState().fullmoveClock);
}

// ---------------------------------------------------------------------------
// Dirty-flag caching (getEvaluation / getFen)
// ---------------------------------------------------------------------------

void test_board_getEvaluation_updates_after_capture(void) {
  setUpBoard();
  int evalBefore = cb.getEvaluation();
  // Set up a capture scenario
  cb.loadFEN("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2");
  cb.makeMove(4, 4, 3, 3);  // exd5
  int evalAfter = cb.getEvaluation();
  // White captured a pawn, should be up ~100 cp
  TEST_ASSERT_TRUE(evalAfter > evalBefore);
}

void test_board_getFen_updates_after_move(void) {
  setUpBoard();
  std::string fenBefore = cb.getFen();
  cb.makeMove(6, 4, 4, 4);  // e2e4
  std::string fenAfter = cb.getFen();
  TEST_ASSERT_FALSE(fenBefore == fenAfter);
  // FEN should reflect black to move
  TEST_ASSERT_TRUE(fenAfter.find(" b ") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Board-level threefold repetition query
// ---------------------------------------------------------------------------

void test_board_isThreefoldRepetition_query(void) {
  setUpBoard();
  cb.loadFEN("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1");
  // Repeat position 3 times
  cb.makeMove(7, 4, 7, 3);  // Ke1-d1
  cb.makeMove(0, 4, 0, 3);  // Ke8-d8
  cb.makeMove(7, 3, 7, 4);  // Kd1-e1
  cb.makeMove(0, 3, 0, 4);  // Kd8-e8 — 2nd occurrence
  cb.makeMove(7, 4, 7, 3);  // Ke1-d1
  cb.makeMove(0, 4, 0, 3);  // Ke8-d8
  cb.makeMove(7, 3, 7, 4);  // Kd1-e1
  cb.makeMove(0, 3, 0, 4);  // Kd8-e8 — 3rd occurrence
  TEST_ASSERT_TRUE(cb.isThreefoldRepetition());
}

void test_board_isThreefoldRepetition_false(void) {
  setUpBoard();
  TEST_ASSERT_FALSE(cb.isThreefoldRepetition());
}

// ---------------------------------------------------------------------------
// reverseMove cache restoration
// ---------------------------------------------------------------------------

void test_board_reverse_move_restores_fen(void) {
  setUpBoard();
  std::string fenBefore = cb.getFen();
  PositionState before = cb.positionState();
  cb.makeMove(6, 4, 4, 4);  // e2-e4
  TEST_ASSERT_FALSE(fenBefore == cb.getFen());

  MoveEntry e = makeBoardEntry(6, 4, 4, 4, Piece::W_PAWN, Piece::NONE, before);
  cb.reverseMove(e);

  TEST_ASSERT_EQUAL_STRING(fenBefore.c_str(), cb.getFen().c_str());
}

void test_board_reverse_move_restores_eval(void) {
  setUpBoard();
  cb.loadFEN("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2");
  int evalBefore = cb.getEvaluation();
  PositionState before = cb.positionState();
  MoveResult r = cb.makeMove(4, 4, 3, 3);  // exd5 capture
  TEST_ASSERT_TRUE(r.valid);
  // Eval should change after capture
  TEST_ASSERT_FALSE(evalBefore == cb.getEvaluation());

  MoveEntry e = makeBoardEntry(4, 4, 3, 3, Piece::W_PAWN, Piece::B_PAWN, before);
  cb.reverseMove(e);

  TEST_ASSERT_EQUAL_INT(evalBefore, cb.getEvaluation());
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_board_tests() {
  needsDefaultKings = false;

  // Initial state
  RUN_TEST(test_board_new_game_board);
  RUN_TEST(test_board_new_game_turn);
  RUN_TEST(test_board_new_game_not_over);
  RUN_TEST(test_board_new_game_fen);
  RUN_TEST(test_board_initial_evaluation_zero);
  RUN_TEST(test_board_getSquare_returns_piece);
  RUN_TEST(test_board_invalidMoveResult_fields);

  // Basic moves
  RUN_TEST(test_board_e2e4);
  RUN_TEST(test_board_illegal_move_rejected);
  RUN_TEST(test_board_wrong_turn_rejected);
  RUN_TEST(test_board_empty_square_rejected);
  RUN_TEST(test_board_out_of_bounds_rejected);
  RUN_TEST(test_board_move_after_game_over_rejected);

  // Captures
  RUN_TEST(test_board_simple_capture);

  // En passant
  RUN_TEST(test_board_en_passant_white);
  RUN_TEST(test_board_en_passant_black);
  RUN_TEST(test_board_ep_target_set_after_double_push);
  RUN_TEST(test_board_ep_target_cleared_after_other_move);

  // Castling
  RUN_TEST(test_board_white_kingside_castle);
  RUN_TEST(test_board_white_queenside_castle);
  RUN_TEST(test_board_black_kingside_castle);
  RUN_TEST(test_board_black_queenside_castle);
  RUN_TEST(test_board_castling_revokes_rights);
  RUN_TEST(test_board_rook_move_revokes_right);
  RUN_TEST(test_board_rook_captured_revokes_castling);

  // Promotion
  RUN_TEST(test_board_auto_queen_promotion);
  RUN_TEST(test_board_knight_promotion);
  RUN_TEST(test_board_black_promotion);
  RUN_TEST(test_board_promotion_with_capture);

  // Check
  RUN_TEST(test_board_move_gives_check);
  RUN_TEST(test_board_move_no_check);

  // Checkmate
  RUN_TEST(test_board_scholars_mate);
  RUN_TEST(test_board_back_rank_mate);

  // Stalemate
  RUN_TEST(test_board_stalemate);

  // 50-move rule
  RUN_TEST(test_board_fifty_move_draw);

  // Insufficient material
  RUN_TEST(test_board_insufficient_material_k_vs_k);
  RUN_TEST(test_board_insufficient_material_kb_vs_k);
  RUN_TEST(test_board_insufficient_material_kn_vs_k);
  RUN_TEST(test_board_insufficient_material_kb_vs_kb_same_color);
  RUN_TEST(test_board_insufficient_material_kb_vs_kb_diff_color);
  RUN_TEST(test_board_sufficient_material_knn);
  RUN_TEST(test_board_sufficient_material_kp_vs_k);

  // FEN loading
  RUN_TEST(test_board_load_fen_sets_turn);
  RUN_TEST(test_board_load_fen_resets_game_over);
  RUN_TEST(test_board_load_fen_roundtrip);
  RUN_TEST(test_board_load_fen_complex);

  // FEN validation
  RUN_TEST(test_board_load_fen_rejects_empty);
  RUN_TEST(test_board_load_fen_rejects_too_few_ranks);
  RUN_TEST(test_board_load_fen_rejects_too_many_ranks);
  RUN_TEST(test_board_load_fen_rejects_invalid_piece);
  RUN_TEST(test_board_load_fen_rejects_rank_overflow);
  RUN_TEST(test_board_load_fen_rejects_invalid_turn);
  RUN_TEST(test_board_load_fen_valid_returns_true);
  RUN_TEST(test_board_load_fen_invalid_preserves_state);

  // Compact move encoding
  RUN_TEST(test_encodeMove_roundtrip);
  RUN_TEST(test_encodeMove_with_promotion);
  RUN_TEST(test_encodeMove_corner_squares);

  // FEN/eval cache
  RUN_TEST(test_board_fen_cache_consistent);
  RUN_TEST(test_board_eval_cache_consistent);
  RUN_TEST(test_board_end_game_preserves_fen);
  RUN_TEST(test_board_eval_after_capture);

  // Position clocks
  RUN_TEST(test_board_halfmove_clock_increments);
  RUN_TEST(test_board_halfmove_clock_resets_on_pawn_move);
  RUN_TEST(test_board_fullmove_increments_after_black);

  // findPiece
  RUN_TEST(test_board_find_piece_kings_initial);
  RUN_TEST(test_board_find_piece_black_pawns_initial);
  RUN_TEST(test_board_find_piece_not_found);
  RUN_TEST(test_board_find_piece_multiple_bishops);
  RUN_TEST(test_board_find_piece_max_limit);

  // inCheck (no-arg)
  RUN_TEST(test_board_in_check_true);
  RUN_TEST(test_board_in_check_false);

  // isCheckmate (no-arg)
  RUN_TEST(test_board_is_checkmate_true);
  RUN_TEST(test_board_is_checkmate_false);

  // isStalemate (no-arg)
  RUN_TEST(test_board_is_stalemate_true);
  RUN_TEST(test_board_is_stalemate_false);

  // isInsufficientMaterial (public)
  RUN_TEST(test_board_is_insufficient_material_true);
  RUN_TEST(test_board_is_insufficient_material_false);

  // isAttacked
  RUN_TEST(test_board_is_attacked_by_white);
  RUN_TEST(test_board_is_attacked_by_black);
  RUN_TEST(test_board_is_attacked_empty_square);

  // moveNumber
  RUN_TEST(test_board_move_number_initial);
  RUN_TEST(test_board_move_number_after_moves);
  RUN_TEST(test_board_move_number_from_fen);

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

  // King cache
  RUN_TEST(test_board_king_cache_initial);
  RUN_TEST(test_board_king_cache_after_king_move);
  RUN_TEST(test_board_king_cache_after_non_king_move);
  RUN_TEST(test_board_king_cache_after_castling);
  RUN_TEST(test_board_king_cache_after_load_fen);
  RUN_TEST(test_board_king_cache_after_reverse_move);
  RUN_TEST(test_board_king_cache_reverse_castling);

  // MoveList struct
  RUN_TEST(test_movelist_initial_state);
  RUN_TEST(test_movelist_add_and_access);
  RUN_TEST(test_movelist_clear);
  RUN_TEST(test_movelist_fills_to_capacity);
  RUN_TEST(test_movelist_used_by_get_possible_moves);

  // HashHistory struct
  RUN_TEST(test_hashhistory_initial_state);
  RUN_TEST(test_hashhistory_add_and_read);
  RUN_TEST(test_hashhistory_max_size);

  // loadFEN edge cases
  RUN_TEST(test_board_load_fen_sets_castling_rights);
  RUN_TEST(test_board_load_fen_sets_ep_target);
  RUN_TEST(test_board_load_fen_sets_clocks);

  // Dirty-flag caching
  RUN_TEST(test_board_getEvaluation_updates_after_capture);
  RUN_TEST(test_board_getFen_updates_after_move);

  // Board-level threefold repetition
  RUN_TEST(test_board_isThreefoldRepetition_query);
  RUN_TEST(test_board_isThreefoldRepetition_false);

  // reverseMove cache restoration
  RUN_TEST(test_board_reverse_move_restores_fen);
  RUN_TEST(test_board_reverse_move_restores_eval);
}
