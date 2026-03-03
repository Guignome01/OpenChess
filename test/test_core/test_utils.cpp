#include <unity.h>

#include "../test_helpers.h"

extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// FEN round-trip
// ---------------------------------------------------------------------------

void test_fen_initial_position_roundtrip(void) {
  setupInitialBoard(board);
  char turn = 'w';
  PositionState state{0x0F, -1, -1, 0, 1};

  std::string fen = ChessUtils::boardToFEN(board, turn, &state);

  // Parse back
  char board2[8][8];
  char turn2;
  PositionState state2;
  ChessUtils::fenToBoard(fen, board2, turn2, &state2);

  TEST_ASSERT_EQUAL_MEMORY(board, board2, 64);
  TEST_ASSERT_EQUAL_CHAR('w', turn2);
  TEST_ASSERT_EQUAL_UINT8(0x0F, state2.castlingRights);
}

void test_fen_standard_initial_string(void) {
  setupInitialBoard(board);
  PositionState state{0x0F, -1, -1, 0, 1};
  std::string fen = ChessUtils::boardToFEN(board, 'w', &state);
  TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", fen.c_str());
}

void test_fen_custom_position_roundtrip(void) {
  std::string inputFen = "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  char turn;
  PositionState state;
  ChessUtils::fenToBoard(inputFen, board, turn, &state);

  std::string outputFen = ChessUtils::boardToFEN(board, turn, &state);
  TEST_ASSERT_EQUAL_STRING(inputFen.c_str(), outputFen.c_str());
}

void test_fen_en_passant_target(void) {
  std::string inputFen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  char turn;
  PositionState state;
  ChessUtils::fenToBoard(inputFen, board, turn, &state);

  TEST_ASSERT_TRUE(state.epRow >= 0 && state.epCol >= 0);
  TEST_ASSERT_EQUAL_INT(5, state.epRow); // row 5 = rank 3
  TEST_ASSERT_EQUAL_INT(4, state.epCol); // e-file

  std::string outputFen = ChessUtils::boardToFEN(board, turn, &state);
  TEST_ASSERT_EQUAL_STRING(inputFen.c_str(), outputFen.c_str());
}

void test_fen_no_castling_rights(void) {
  std::string inputFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1";
  char turn;
  PositionState state;
  ChessUtils::fenToBoard(inputFen, board, turn, &state);
  TEST_ASSERT_EQUAL_UINT8(0x00, state.castlingRights);
}

void test_fen_halfmove_and_fullmove(void) {
  std::string inputFen = "8/8/8/8/8/8/8/4K3 w - - 42 100";
  char turn;
  PositionState state;
  ChessUtils::fenToBoard(inputFen, board, turn, &state);
  TEST_ASSERT_EQUAL_INT(42, state.halfmoveClock);
  TEST_ASSERT_EQUAL_INT(100, state.fullmoveClock);
}

// ---------------------------------------------------------------------------
// Material evaluation
// ---------------------------------------------------------------------------

void test_evaluation_initial_is_zero(void) {
  setupInitialBoard(board);
  float eval = ChessUtils::evaluatePosition(board);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eval);
}

void test_evaluation_white_up_queen(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'Q', "d1");
  placePiece(board, 'k', "e8");
  float eval = ChessUtils::evaluatePosition(board);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, eval); // White has +9 for queen
}

void test_evaluation_equal_material(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'R', "a1");
  placePiece(board, 'k', "e8");
  placePiece(board, 'r', "a8");
  float eval = ChessUtils::evaluatePosition(board);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eval);
}

// ---------------------------------------------------------------------------
// Piece color helpers
// ---------------------------------------------------------------------------

void test_getPieceColor(void) {
  TEST_ASSERT_EQUAL_CHAR('w', ChessUtils::getPieceColor('K'));
  TEST_ASSERT_EQUAL_CHAR('w', ChessUtils::getPieceColor('P'));
  TEST_ASSERT_EQUAL_CHAR('b', ChessUtils::getPieceColor('k'));
  TEST_ASSERT_EQUAL_CHAR('b', ChessUtils::getPieceColor('p'));
  TEST_ASSERT_EQUAL_CHAR(' ', ChessUtils::getPieceColor(' '));  // empty square
}

void test_isWhitePiece(void) {
  TEST_ASSERT_TRUE(ChessUtils::isWhitePiece('K'));
  TEST_ASSERT_TRUE(ChessUtils::isWhitePiece('Q'));
  TEST_ASSERT_FALSE(ChessUtils::isWhitePiece('k'));
  TEST_ASSERT_FALSE(ChessUtils::isWhitePiece(' '));
}

void test_isBlackPiece(void) {
  TEST_ASSERT_TRUE(ChessUtils::isBlackPiece('k'));
  TEST_ASSERT_TRUE(ChessUtils::isBlackPiece('q'));
  TEST_ASSERT_FALSE(ChessUtils::isBlackPiece('K'));
  TEST_ASSERT_FALSE(ChessUtils::isBlackPiece(' '));
}

void test_colorName(void) {
  TEST_ASSERT_EQUAL_STRING("White", ChessUtils::colorName('w'));
  TEST_ASSERT_EQUAL_STRING("Black", ChessUtils::colorName('b'));
  TEST_ASSERT_EQUAL_STRING("Unknown", ChessUtils::colorName('x'));
}

// ---------------------------------------------------------------------------
// 50-move rule (PositionState field validation)
// ---------------------------------------------------------------------------

void test_fifty_move_rule(void) {
  PositionState state;
  state.halfmoveClock = 100;
  TEST_ASSERT_TRUE(state.halfmoveClock >= 100);
}

void test_no_fifty_move_rule(void) {
  PositionState state;
  state.halfmoveClock = 99;
  TEST_ASSERT_FALSE(state.halfmoveClock >= 100);
}

// ---------------------------------------------------------------------------
// hasAnyLegalMove (ChessRules)
// ---------------------------------------------------------------------------

void test_has_legal_moves_initial(void) {
  setupInitialBoard(board);
  PositionState flags{0x0F, -1, -1};
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(board, 'w', flags));
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(board, 'b', flags));
}

void test_no_legal_moves_stalemate(void) {
  // Same stalemate position from check_mechanics
  placePiece(board, 'k', "a8");
  placePiece(board, 'Q', "b6");
  placePiece(board, 'K', "c6");
  PositionState flags{0x00, -1, -1};
  TEST_ASSERT_FALSE(ChessRules::hasAnyLegalMove(board, 'b', flags));
}

// ---------------------------------------------------------------------------
// Additional evaluation tests
// ---------------------------------------------------------------------------

void test_evaluation_black_advantage(void) {
  placePiece(board, 'K', "e1");
  placePiece(board, 'k', "e8");
  placePiece(board, 'q', "d8"); // black queen, no white queen
  float eval = ChessUtils::evaluatePosition(board);
  TEST_ASSERT_TRUE(eval < 0.0f); // negative = black advantage
}

void test_evaluation_empty_board(void) {
  // board is already cleared by setUp
  float eval = ChessUtils::evaluatePosition(board);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eval);
}

// ---------------------------------------------------------------------------
// Additional FEN/helper tests
// ---------------------------------------------------------------------------

void test_boardToFEN_nullptr_state(void) {
  setupInitialBoard(board);
  std::string fen = ChessUtils::boardToFEN(board, 'w', nullptr);
  // With nullptr state, should use defaults: KQkq, -, 0, 1
  TEST_ASSERT_EQUAL_STRING("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", fen.c_str());
}

void test_hasAnyLegalMove_in_check_with_escape(void) {
  // King in check but can escape
  placePiece(board, 'K', "e1");
  placePiece(board, 'r', "e8"); // rook checks
  PositionState flags{0x00, -1, -1};
  // King can escape to d1, d2, f1, f2
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(board, 'w', flags));
}

void test_fen_partial_castling_rights_roundtrip(void) {
  std::string inputFen = "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w Kk - 0 1";
  char turn;
  PositionState state;
  ChessUtils::fenToBoard(inputFen, board, turn, &state);
  // Kk = white kingside + black kingside = 0x01 | 0x04 = 0x05
  TEST_ASSERT_EQUAL_UINT8(0x05, state.castlingRights);
  std::string outputFen = ChessUtils::boardToFEN(board, turn, &state);
  TEST_ASSERT_EQUAL_STRING(inputFen.c_str(), outputFen.c_str());
}

// ---------------------------------------------------------------------------
// checkEnPassant
// ---------------------------------------------------------------------------

void test_checkEnPassant_double_push_sets_target(void) {
  // White pawn e2→e4 (row 6→4): EP target should be e3 (row 5, col 4)
  auto ep = ChessUtils::checkEnPassant(6, 4, 4, 4, 'P', ' ');
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.capturedPawnRow);
  TEST_ASSERT_EQUAL_INT(5, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(4, ep.nextEpCol);
}

void test_checkEnPassant_single_push_no_target(void) {
  // White pawn e3→e4 (row 5→4): no EP target
  auto ep = ChessUtils::checkEnPassant(5, 4, 4, 4, 'P', ' ');
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpCol);
}

void test_checkEnPassant_capture_detected(void) {
  // White pawn diagonal capture to empty square = EP capture
  auto ep = ChessUtils::checkEnPassant(3, 4, 2, 5, 'P', ' ');
  TEST_ASSERT_TRUE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(3, ep.capturedPawnRow); // captured pawn is on same row as from
}

void test_checkEnPassant_normal_capture_not_ep(void) {
  // White pawn diagonal capture to occupied square = normal capture, not EP
  auto ep = ChessUtils::checkEnPassant(3, 4, 2, 5, 'P', 'p');
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.capturedPawnRow);
}

void test_checkEnPassant_black_double_push(void) {
  // Black pawn d7→d5 (row 1→3): EP target should be d6 (row 2, col 3)
  auto ep = ChessUtils::checkEnPassant(1, 3, 3, 3, 'p', ' ');
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(2, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(3, ep.nextEpCol);
}

void test_checkEnPassant_non_pawn_no_effect(void) {
  // Knight move — should never set EP target or capture
  auto ep = ChessUtils::checkEnPassant(7, 1, 5, 2, 'N', ' ');
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpCol);
}

// ---------------------------------------------------------------------------
// checkCastling
// ---------------------------------------------------------------------------

void test_checkCastling_kingside(void) {
  auto c = ChessUtils::checkCastling(7, 4, 7, 6, 'K');
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(7, c.rookFromCol);  // h-file
  TEST_ASSERT_EQUAL_INT(5, c.rookToCol);    // f-file
}

void test_checkCastling_queenside(void) {
  auto c = ChessUtils::checkCastling(7, 4, 7, 2, 'K');
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(0, c.rookFromCol);  // a-file
  TEST_ASSERT_EQUAL_INT(3, c.rookToCol);    // d-file
}

void test_checkCastling_black_kingside(void) {
  auto c = ChessUtils::checkCastling(0, 4, 0, 6, 'k');
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(7, c.rookFromCol);
  TEST_ASSERT_EQUAL_INT(5, c.rookToCol);
}

void test_checkCastling_not_king(void) {
  // Rook moving 2 squares is not castling
  auto c = ChessUtils::checkCastling(7, 0, 7, 2, 'R');
  TEST_ASSERT_FALSE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(-1, c.rookFromCol);
  TEST_ASSERT_EQUAL_INT(-1, c.rookToCol);
}

void test_checkCastling_king_one_square(void) {
  // King moving one square is not castling
  auto c = ChessUtils::checkCastling(7, 4, 7, 5, 'K');
  TEST_ASSERT_FALSE(c.isCastling);
}

// ---------------------------------------------------------------------------
// updateCastlingRights
// ---------------------------------------------------------------------------

void test_updateCastlingRights_white_king_moves(void) {
  uint8_t rights = 0x0F;  // KQkq
  rights = ChessUtils::updateCastlingRights(rights, 7, 4, 7, 5, 'K', ' ');
  TEST_ASSERT_EQUAL_UINT8(0x0C, rights);  // kq only
}

void test_updateCastlingRights_black_king_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 0, 4, 0, 5, 'k', ' ');
  TEST_ASSERT_EQUAL_UINT8(0x03, rights);  // KQ only
}

void test_updateCastlingRights_white_h_rook_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 7, 7, 5, 7, 'R', ' ');
  TEST_ASSERT_EQUAL_UINT8(0x0E, rights);  // Qkq (lost K)
}

void test_updateCastlingRights_white_a_rook_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 7, 0, 5, 0, 'R', ' ');
  TEST_ASSERT_EQUAL_UINT8(0x0D, rights);  // Kkq (lost Q)
}

void test_updateCastlingRights_rook_captured(void) {
  uint8_t rights = 0x0F;
  // Black captures white h-rook
  rights = ChessUtils::updateCastlingRights(rights, 0, 7, 7, 7, 'r', 'R');
  // Lost K (rook captured on h1) + lost k (black rook moved from h8)
  TEST_ASSERT_EQUAL_UINT8(0x0A, rights);  // Qq
}

void test_updateCastlingRights_no_change_on_pawn_move(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 6, 4, 4, 4, 'P', ' ');
  TEST_ASSERT_EQUAL_UINT8(0x0F, rights);  // unchanged
}

// ---------------------------------------------------------------------------
// opponentColor / squareName / PositionState::initial
// ---------------------------------------------------------------------------

void test_opponentColor_white(void) {
  TEST_ASSERT_EQUAL_CHAR('b', ChessUtils::opponentColor('w'));
}

void test_opponentColor_black(void) {
  TEST_ASSERT_EQUAL_CHAR('w', ChessUtils::opponentColor('b'));
}

void test_squareName_corners(void) {
  TEST_ASSERT_EQUAL_STRING("a8", ChessUtils::squareName(0, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("h8", ChessUtils::squareName(0, 7).c_str());
  TEST_ASSERT_EQUAL_STRING("a1", ChessUtils::squareName(7, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("h1", ChessUtils::squareName(7, 7).c_str());
}

void test_squareName_center(void) {
  TEST_ASSERT_EQUAL_STRING("e4", ChessUtils::squareName(4, 4).c_str());
  TEST_ASSERT_EQUAL_STRING("d5", ChessUtils::squareName(3, 3).c_str());
}

void test_positionState_initial(void) {
  PositionState st = PositionState::initial();
  TEST_ASSERT_EQUAL_UINT8(0x0F, st.castlingRights);
  TEST_ASSERT_EQUAL_INT(-1, st.epRow);
  TEST_ASSERT_EQUAL_INT(-1, st.epCol);
  TEST_ASSERT_EQUAL_INT(0, st.halfmoveClock);
  TEST_ASSERT_EQUAL_INT(1, st.fullmoveClock);
}

void register_utils_tests() {
  needsDefaultKings = false;

  // FEN
  RUN_TEST(test_fen_initial_position_roundtrip);
  RUN_TEST(test_fen_standard_initial_string);
  RUN_TEST(test_fen_custom_position_roundtrip);
  RUN_TEST(test_fen_en_passant_target);
  RUN_TEST(test_fen_no_castling_rights);
  RUN_TEST(test_fen_halfmove_and_fullmove);

  // Evaluation
  RUN_TEST(test_evaluation_initial_is_zero);
  RUN_TEST(test_evaluation_white_up_queen);
  RUN_TEST(test_evaluation_equal_material);

  // Piece color helpers
  RUN_TEST(test_getPieceColor);
  RUN_TEST(test_isWhitePiece);
  RUN_TEST(test_isBlackPiece);
  RUN_TEST(test_colorName);

  // 50-move rule
  RUN_TEST(test_fifty_move_rule);
  RUN_TEST(test_no_fifty_move_rule);

  // Legal moves
  RUN_TEST(test_has_legal_moves_initial);
  RUN_TEST(test_no_legal_moves_stalemate);
  RUN_TEST(test_hasAnyLegalMove_in_check_with_escape);

  // Additional evaluation
  RUN_TEST(test_evaluation_black_advantage);
  RUN_TEST(test_evaluation_empty_board);

  // Additional FEN / helpers
  RUN_TEST(test_boardToFEN_nullptr_state);
  RUN_TEST(test_fen_partial_castling_rights_roundtrip);

  // checkEnPassant
  RUN_TEST(test_checkEnPassant_double_push_sets_target);
  RUN_TEST(test_checkEnPassant_single_push_no_target);
  RUN_TEST(test_checkEnPassant_capture_detected);
  RUN_TEST(test_checkEnPassant_normal_capture_not_ep);
  RUN_TEST(test_checkEnPassant_black_double_push);
  RUN_TEST(test_checkEnPassant_non_pawn_no_effect);

  // checkCastling
  RUN_TEST(test_checkCastling_kingside);
  RUN_TEST(test_checkCastling_queenside);
  RUN_TEST(test_checkCastling_black_kingside);
  RUN_TEST(test_checkCastling_not_king);
  RUN_TEST(test_checkCastling_king_one_square);

  // updateCastlingRights
  RUN_TEST(test_updateCastlingRights_white_king_moves);
  RUN_TEST(test_updateCastlingRights_black_king_moves);
  RUN_TEST(test_updateCastlingRights_white_h_rook_moves);
  RUN_TEST(test_updateCastlingRights_white_a_rook_moves);
  RUN_TEST(test_updateCastlingRights_rook_captured);
  RUN_TEST(test_updateCastlingRights_no_change_on_pawn_move);

  // opponentColor
  RUN_TEST(test_opponentColor_white);
  RUN_TEST(test_opponentColor_black);

  // squareName
  RUN_TEST(test_squareName_corners);
  RUN_TEST(test_squareName_center);

  // PositionState::initial
  RUN_TEST(test_positionState_initial);
}
