#include <unity.h>

#include "../test_helpers.h"

extern ChessBitboard::BitboardSet bb;
extern Piece mailbox[64];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// isCheck
// ---------------------------------------------------------------------------

void test_king_not_in_check_initial(void) {
  setupInitialBoard(bb, mailbox);
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, Color::WHITE));
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, Color::BLACK));
}

void test_king_in_check_by_rook(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // rook on same file
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_king_in_check_by_bishop(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_BISHOP, "h4"); // bishop on diagonal
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_king_in_check_by_knight(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KNIGHT, "f3"); // knight checks
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_king_in_check_by_pawn(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e4");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5"); // black pawn attacks e4 from d5
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_king_in_check_by_queen(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_QUEEN, "e8"); // queen on same file
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_king_not_in_check_blocked(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8");
  placePiece(bb, mailbox, Piece::W_PAWN, "e2"); // own pawn blocks rook
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, Color::WHITE));
}

void test_black_king_in_check(void) {
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::W_ROOK, "e1"); // white rook attacks
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::BLACK));
}

// ---------------------------------------------------------------------------
// isCheckmate
// ---------------------------------------------------------------------------

void test_back_rank_mate(void) {
  // Classic back-rank mate: Black king on g8, pawns on f7/g7/h7, White rook on e8
  placePiece(bb, mailbox, Piece::B_KING, "g8");
  placePiece(bb, mailbox, Piece::B_PAWN, "f7");
  placePiece(bb, mailbox, Piece::B_PAWN, "g7");
  placePiece(bb, mailbox, Piece::B_PAWN, "h7");
  placePiece(bb, mailbox, Piece::W_ROOK, "e8"); // delivers mate
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_TRUE(ChessRules::isCheckmate(bb, mailbox, Color::BLACK, flags));
}

void test_scholars_mate(void) {
  // Scholar's mate position: White queen on f7 delivers checkmate
  PositionState state;
  Color turn;
  ChessFEN::fenToBoard("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b - - 0 1", bb, mailbox, turn, &state);
  TEST_ASSERT_TRUE(ChessRules::isCheckmate(bb, mailbox, Color::BLACK, state));
}

void test_not_checkmate_can_block(void) {
  // King in check but can block
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_ROOK, "d1"); // own rook can block/interpose
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // attacking rook
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(bb, mailbox, Color::WHITE, flags));
}

void test_not_checkmate_can_escape(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // rook checks
  // King can escape to d1, d2, f1, f2
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(bb, mailbox, Color::WHITE, flags));
}

void test_not_checkmate_can_capture_attacker(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e2"); // rook checks from e2
  // King can capture the rook (assuming no support)
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::isCheckmate(bb, mailbox, Color::WHITE, flags));
}

// ---------------------------------------------------------------------------
// isStalemate
// ---------------------------------------------------------------------------

void test_stalemate_king_only(void) {
  // Classic stalemate: Black king on a8, White queen on b6, White king on c6
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_QUEEN, "b6");
  placePiece(bb, mailbox, Piece::W_KING, "c6");
  PositionState flags{0x00, -1, -1, 0, 1};
  // Black to move — king has no legal moves, not in check
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, Color::BLACK));
  TEST_ASSERT_TRUE(ChessRules::isStalemate(bb, mailbox, Color::BLACK, flags));
}

void test_not_stalemate_has_move(void) {
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_KING, "c6");
  PositionState flags{0x00, -1, -1, 0, 1};
  // Black king can move to b8, b7, a7
  TEST_ASSERT_FALSE(ChessRules::isStalemate(bb, mailbox, Color::BLACK, flags));
}

// ---------------------------------------------------------------------------
// Move legality (can't move into check)
// ---------------------------------------------------------------------------

void test_king_cannot_move_into_check(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "f8"); // rook controls f-file
  int r, c;
  sq("e1", r, c);
  int tr, tc;
  sq("f1", tr, tc);
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::isValidMove(bb, mailbox, r, c, tr, tc, flags));
}

void test_pinned_piece_cannot_move(void) {
  // Bishop pinned to its own king
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_BISHOP, "e2"); // bishop on same file, between king and rook
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // enemy rook pins bishop
  int r, c;
  sq("e2", r, c);
  int tr, tc;
  sq("d3", tr, tc);
  PositionState flags{0x00, -1, -1, 0, 1};
  // Moving bishop exposes king to check → illegal
  TEST_ASSERT_FALSE(ChessRules::isValidMove(bb, mailbox, r, c, tr, tc, flags));
}

void test_pinned_piece_can_move_along_pin(void) {
  // Rook pinned along file — can move along that file
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_ROOK, "e4"); // own rook on same file
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // enemy rook pins
  int r, c;
  sq("e4", r, c);
  int tr, tc;
  sq("e8", tr, tc); // capture the pinning rook
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_TRUE(ChessRules::isValidMove(bb, mailbox, r, c, tr, tc, flags));
}

// ---------------------------------------------------------------------------
// hasLegalEnPassantCapture (direct tests)
// ---------------------------------------------------------------------------

void test_hasLegalEnPassantCapture_true(void) {
  // White pawn on e5, black pawn on d5, EP target d6
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_PAWN, "e5");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");
  int epR, epC;
  sq("d6", epR, epC);
  PositionState flags{0x00, epR, epC, 0, 1};
  TEST_ASSERT_TRUE(ChessRules::hasLegalEnPassantCapture(bb, mailbox, Color::WHITE, flags));
}

void test_hasLegalEnPassantCapture_false_no_target(void) {
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_PAWN, "e5");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");
  PositionState flags{0x00, -1, -1, 0, 1}; // no EP target
  TEST_ASSERT_FALSE(ChessRules::hasLegalEnPassantCapture(bb, mailbox, Color::WHITE, flags));
}

// ---------------------------------------------------------------------------
// isSquareUnderAttack (direct tests)
// ---------------------------------------------------------------------------

void test_isSquareUnderAttack_by_pawn(void) {
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");
  int r, c;
  sq("e4", r, c);
  // e4 is attacked by black pawn from d5 (defending color = white → attacker = black)
  TEST_ASSERT_TRUE(ChessRules::isSquareUnderAttack(bb, r, c, Color::WHITE));
}

void test_isSquareUnderAttack_by_knight(void) {
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::B_KNIGHT, "f3");
  int r, c;
  sq("e1", r, c);
  TEST_ASSERT_TRUE(ChessRules::isSquareUnderAttack(bb, r, c, Color::WHITE));
}

void test_isSquareUnderAttack_not_attacked(void) {
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::B_KNIGHT, "f3");
  int r, c;
  sq("a4", r, c);
  TEST_ASSERT_FALSE(ChessRules::isSquareUnderAttack(bb, r, c, Color::WHITE));
}

// ---------------------------------------------------------------------------
// Advanced check/mate scenarios
// ---------------------------------------------------------------------------

void test_discovered_check(void) {
  // White bishop on c1 blocks white rook on a1 from checking black king on h1.
  // Move the bishop away to reveal the rook check.
  placePiece(bb, mailbox, Piece::B_KING, "h1");
  placePiece(bb, mailbox, Piece::W_KING, "a8");
  placePiece(bb, mailbox, Piece::W_ROOK, "a1"); // rook on a1
  placePiece(bb, mailbox, Piece::W_BISHOP, "d1"); // bishop blocks rank 1
  // After bishop moves to e2 (off rank 1), rook gives check along rank 1
  // Verify the bishop CAN move (it would reveal check on opponent's king)
  int r, c;
  sq("d1", r, c);
  int tr, tc;
  sq("e2", tr, tc);
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_TRUE(ChessRules::isValidMove(bb, mailbox, r, c, tr, tc, flags));
}

void test_double_check_only_king_can_move(void) {
  // Black king in double check from white rook and bishop.
  // Only king moves should be legal — no blocks or captures by other pieces.
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::W_ROOK, "e1"); // rook checks along e-file
  placePiece(bb, mailbox, Piece::W_BISHOP, "b5"); // bishop checks along b5-e8 diagonal
  placePiece(bb, mailbox, Piece::B_KNIGHT, "d6"); // black knight could theoretically block/capture

  PositionState flags{0x00, -1, -1, 0, 1};
  // King is in check
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::BLACK));
  // Knight on d6 cannot resolve double check (even though it attacks both e4 and b5)
  MoveList moves;
  int r, c;
  sq("d6", r, c);
  ChessRules::getPossibleMoves(bb, mailbox, r, c, flags, moves);
  TEST_ASSERT_EQUAL_INT(0, moves.count);
}

void test_smothered_mate(void) {
  // Philidor's smothered mate: Kh8, Rg8, g7/h7 pawns, white Nf7#
  placePiece(bb, mailbox, Piece::B_KING, "h8");
  placePiece(bb, mailbox, Piece::B_ROOK, "g8"); // own rook blocks g8
  placePiece(bb, mailbox, Piece::B_PAWN, "g7");
  placePiece(bb, mailbox, Piece::B_PAWN, "h7");
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  placePiece(bb, mailbox, Piece::W_KNIGHT, "f7"); // knight checks h8, blocks via g8/g7/h7
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, Color::BLACK));
  TEST_ASSERT_TRUE(ChessRules::isCheckmate(bb, mailbox, Color::BLACK, flags));
}

void test_stalemate_with_blocked_pawns(void) {
  // Black king on a8, black pawn on a7 blocked by white pawn on a6.
  // White king on c7 controls b8, b7, c8, d8, d7.
  // Black has no legal moves: king surrounded, pawn blocked.
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::B_PAWN, "a7");
  placePiece(bb, mailbox, Piece::W_PAWN, "a6"); // blocks the pawn
  placePiece(bb, mailbox, Piece::W_KING, "c7"); // controls b8, b7, c8, d8, d7
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, Color::BLACK));
  TEST_ASSERT_TRUE(ChessRules::isStalemate(bb, mailbox, Color::BLACK, flags));
}

void test_diagonal_pin(void) {
  // Black pawn on d4 pinned to black king on g7 by white bishop on a1
  placePiece(bb, mailbox, Piece::B_KING, "g7");
  placePiece(bb, mailbox, Piece::W_KING, "a8");
  placePiece(bb, mailbox, Piece::B_PAWN, "d4");
  placePiece(bb, mailbox, Piece::W_BISHOP, "a1"); // pins d4 to g7 along diagonal
  int r, c;
  sq("d4", r, c);
  PositionState flags{0x00, -1, -1, 0, 1};
  // Pawn should have 0 legal moves (pinned diagonally, can't move along pin)
  MoveList moves;
  ChessRules::getPossibleMoves(bb, mailbox, r, c, flags, moves);
  TEST_ASSERT_EQUAL_INT(0, moves.count);
}

// ---------------------------------------------------------------------------
// isCheck — 4-param overload (explicit king position)
// ---------------------------------------------------------------------------

void test_isCheck_explicit_king_position(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8");
  // Using explicit king position
  TEST_ASSERT_TRUE(ChessRules::isCheck(bb, squareOf(7, 4), Color::WHITE));
}

void test_isCheck_explicit_wrong_position(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8");
  // Pass wrong position — king is not really at d1, so check should not be detected there
  TEST_ASSERT_FALSE(ChessRules::isCheck(bb, squareOf(7, 3), Color::WHITE));
}

// ---------------------------------------------------------------------------
// isValidMove — direct tests
// ---------------------------------------------------------------------------

void test_isValidMove_basic_valid(void) {
  setupInitialBoard(bb, mailbox);
  PositionState flags = PositionState::initial();
  // e2e4 is valid
  TEST_ASSERT_TRUE(ChessRules::isValidMove(bb, mailbox, 6, 4, 4, 4, flags));
}

void test_isValidMove_illegal_destination(void) {
  setupInitialBoard(bb, mailbox);
  PositionState flags = PositionState::initial();
  // e2e5 (3 squares) is illegal for a pawn
  TEST_ASSERT_FALSE(ChessRules::isValidMove(bb, mailbox, 6, 4, 3, 4, flags));
}

void test_isValidMove_empty_source(void) {
  PositionState flags{0x00, -1, -1, 0, 1};
  placePiece(bb, mailbox, Piece::W_KING, "h1");
  placePiece(bb, mailbox, Piece::B_KING, "h8");
  // e4 is empty — moving from empty square is invalid
  TEST_ASSERT_FALSE(ChessRules::isValidMove(bb, mailbox, 4, 4, 3, 4, flags));
}

// ---------------------------------------------------------------------------
// isDraw — direct static tests
// ---------------------------------------------------------------------------

void test_rules_isDraw_insufficient(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  PositionState st{0x00, -1, -1, 0, 1};
  HashHistory hh{};
  TEST_ASSERT_TRUE(ChessRules::isDraw(bb, mailbox, Color::WHITE, st, hh));
}

void test_rules_isDraw_fifty_move(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::W_ROOK, "a1");  // sufficient material
  PositionState st{0x00, -1, -1, 100, 50};
  HashHistory hh{};
  TEST_ASSERT_TRUE(ChessRules::isDraw(bb, mailbox, Color::WHITE, st, hh));
}

void test_rules_isDraw_false(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st = PositionState::initial();
  HashHistory hh{};
  TEST_ASSERT_FALSE(ChessRules::isDraw(bb, mailbox, Color::WHITE, st, hh));
}

// ---------------------------------------------------------------------------
// isGameOver — direct static tests
// ---------------------------------------------------------------------------

void test_rules_isGameOver_checkmate(void) {
  // Back rank mate
  placePiece(bb, mailbox, Piece::B_KING, "g8");
  placePiece(bb, mailbox, Piece::B_PAWN, "f7");
  placePiece(bb, mailbox, Piece::B_PAWN, "g7");
  placePiece(bb, mailbox, Piece::B_PAWN, "h7");
  placePiece(bb, mailbox, Piece::W_ROOK, "e8");
  placePiece(bb, mailbox, Piece::W_KING, "a1");
  PositionState st{0x00, -1, -1, 0, 1};
  HashHistory hh{};
  char winner = ' ';
  GameResult result = ChessRules::isGameOver(bb, mailbox, Color::BLACK, st, hh, winner);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, result);
  TEST_ASSERT_EQUAL_CHAR('w', winner);
}

void test_rules_isGameOver_stalemate(void) {
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_QUEEN, "b6");
  placePiece(bb, mailbox, Piece::W_KING, "c6");
  PositionState st{0x00, -1, -1, 0, 1};
  HashHistory hh{};
  char winner = ' ';
  GameResult result = ChessRules::isGameOver(bb, mailbox, Color::BLACK, st, hh, winner);
  TEST_ASSERT_ENUM_EQ(GameResult::STALEMATE, result);
}

void test_rules_isGameOver_in_progress(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st = PositionState::initial();
  HashHistory hh{};
  char winner = ' ';
  GameResult result = ChessRules::isGameOver(bb, mailbox, Color::WHITE, st, hh, winner);
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, result);
}

void test_rules_isThreefoldRepetition_direct(void) {
  // Fabricate a HashHistory with 3 identical hashes
  HashHistory hh{};
  hh.keys[0] = 0xABCD;
  hh.keys[1] = 0x1234;
  hh.keys[2] = 0xABCD;
  hh.keys[3] = 0x5678;
  hh.keys[4] = 0xABCD;
  hh.count = 5;
  TEST_ASSERT_TRUE(ChessRules::isThreefoldRepetition(hh));
}

void test_rules_isThreefoldRepetition_not_reached(void) {
  HashHistory hh{};
  hh.keys[0] = 0xABCD;
  hh.keys[1] = 0x1234;
  hh.keys[2] = 0xABCD;
  hh.count = 3;
  TEST_ASSERT_FALSE(ChessRules::isThreefoldRepetition(hh));
}

void register_chess_rules_check_tests() {
  needsDefaultKings = false;

  // Check detection
  RUN_TEST(test_king_not_in_check_initial);
  RUN_TEST(test_king_in_check_by_rook);
  RUN_TEST(test_king_in_check_by_bishop);
  RUN_TEST(test_king_in_check_by_knight);
  RUN_TEST(test_king_in_check_by_pawn);
  RUN_TEST(test_king_in_check_by_queen);
  RUN_TEST(test_king_not_in_check_blocked);
  RUN_TEST(test_black_king_in_check);

  // Checkmate
  RUN_TEST(test_back_rank_mate);
  RUN_TEST(test_scholars_mate);
  RUN_TEST(test_not_checkmate_can_block);
  RUN_TEST(test_not_checkmate_can_escape);
  RUN_TEST(test_not_checkmate_can_capture_attacker);
  RUN_TEST(test_smothered_mate);

  // Stalemate
  RUN_TEST(test_stalemate_king_only);
  RUN_TEST(test_not_stalemate_has_move);
  RUN_TEST(test_stalemate_with_blocked_pawns);

  // Move legality
  RUN_TEST(test_king_cannot_move_into_check);
  RUN_TEST(test_pinned_piece_cannot_move);
  RUN_TEST(test_pinned_piece_can_move_along_pin);
  RUN_TEST(test_diagonal_pin);
  RUN_TEST(test_discovered_check);
  RUN_TEST(test_double_check_only_king_can_move);

  // En passant legality
  RUN_TEST(test_hasLegalEnPassantCapture_true);
  RUN_TEST(test_hasLegalEnPassantCapture_false_no_target);

  // Square attack detection
  RUN_TEST(test_isSquareUnderAttack_by_pawn);
  RUN_TEST(test_isSquareUnderAttack_by_knight);
  RUN_TEST(test_isSquareUnderAttack_not_attacked);

  // isCheck explicit king position
  RUN_TEST(test_isCheck_explicit_king_position);
  RUN_TEST(test_isCheck_explicit_wrong_position);

  // isValidMove direct
  RUN_TEST(test_isValidMove_basic_valid);
  RUN_TEST(test_isValidMove_illegal_destination);
  RUN_TEST(test_isValidMove_empty_source);

  // isDraw direct
  RUN_TEST(test_rules_isDraw_insufficient);
  RUN_TEST(test_rules_isDraw_fifty_move);
  RUN_TEST(test_rules_isDraw_false);

  // isGameOver direct
  RUN_TEST(test_rules_isGameOver_checkmate);
  RUN_TEST(test_rules_isGameOver_stalemate);
  RUN_TEST(test_rules_isGameOver_in_progress);

  // isThreefoldRepetition direct
  RUN_TEST(test_rules_isThreefoldRepetition_direct);
  RUN_TEST(test_rules_isThreefoldRepetition_not_reached);
}
