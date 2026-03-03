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
}
