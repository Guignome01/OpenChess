#include <unity.h>

#include "../../lib/core/src/chess_evaluation.h"
#include "../test_helpers.h"

extern ChessBitboard::BitboardSet bb;
extern Piece mailbox[64];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// Material evaluation
// ---------------------------------------------------------------------------

void test_evaluation_initial_is_zero(void) {
  setupInitialBoard(bb, mailbox);
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_EQUAL_INT(0, eval);
}

void test_evaluation_white_up_queen(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_QUEEN, "d1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_TRUE(eval > 800);  // 900 material - small PST penalty for queen on d1
}

void test_evaluation_equal_material(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_ROOK, "a1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::B_ROOK, "a8");
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_EQUAL_INT(0, eval);  // symmetric placement → zero
}

// ---------------------------------------------------------------------------
// Piece color helpers
// ---------------------------------------------------------------------------

void test_getPieceColor(void) {
  TEST_ASSERT_ENUM_EQ(Color::WHITE, ChessPiece::pieceColor(Piece::W_KING));
  TEST_ASSERT_ENUM_EQ(Color::WHITE, ChessPiece::pieceColor(Piece::W_PAWN));
  TEST_ASSERT_ENUM_EQ(Color::BLACK, ChessPiece::pieceColor(Piece::B_KING));
  TEST_ASSERT_ENUM_EQ(Color::BLACK, ChessPiece::pieceColor(Piece::B_PAWN));
  // pieceColor on NONE is undefined behavior — only check valid pieces
}

void test_isWhitePiece(void) {
  TEST_ASSERT_TRUE(ChessPiece::isWhite(Piece::W_KING));
  TEST_ASSERT_TRUE(ChessPiece::isWhite(Piece::W_QUEEN));
  TEST_ASSERT_FALSE(ChessPiece::isWhite(Piece::B_KING));
  TEST_ASSERT_FALSE(ChessPiece::isWhite(Piece::NONE));
}

void test_isBlackPiece(void) {
  TEST_ASSERT_TRUE(ChessPiece::isBlack(Piece::B_KING));
  TEST_ASSERT_TRUE(ChessPiece::isBlack(Piece::B_QUEEN));
  TEST_ASSERT_FALSE(ChessPiece::isBlack(Piece::W_KING));
  TEST_ASSERT_FALSE(ChessPiece::isBlack(Piece::NONE));
}

void test_colorName(void) {
  TEST_ASSERT_EQUAL_STRING("White", ChessPiece::colorName(Color::WHITE));
  TEST_ASSERT_EQUAL_STRING("Black", ChessPiece::colorName(Color::BLACK));
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
  setupInitialBoard(bb, mailbox);
  PositionState flags{0x0F, -1, -1};
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(bb, mailbox, Color::WHITE, flags));
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(bb, mailbox, Color::BLACK, flags));
}

void test_no_legal_moves_stalemate(void) {
  // Same stalemate position from check_mechanics
  placePiece(bb, mailbox, Piece::B_KING, "a8");
  placePiece(bb, mailbox, Piece::W_QUEEN, "b6");
  placePiece(bb, mailbox, Piece::W_KING, "c6");
  PositionState flags{0x00, -1, -1, 0, 1};
  TEST_ASSERT_FALSE(ChessRules::hasAnyLegalMove(bb, mailbox, Color::BLACK, flags));
}

// ---------------------------------------------------------------------------
// Additional evaluation tests
// ---------------------------------------------------------------------------

void test_evaluation_black_advantage(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::B_QUEEN, "d8"); // black queen, no white queen
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_TRUE(eval < 0); // negative = black advantage
}

void test_evaluation_empty_board(void) {
  // board is already cleared by setUp
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_EQUAL_INT(0, eval);
}

// ---------------------------------------------------------------------------
// Pawn structure evaluation
// ---------------------------------------------------------------------------

void test_eval_pawn_structure_symmetry(void) {
  // Symmetric pawn structure → evaluation must be 0.
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "d2");
  placePiece(bb, mailbox, Piece::W_PAWN, "e2");
  placePiece(bb, mailbox, Piece::W_PAWN, "f2");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::B_PAWN, "d7");
  placePiece(bb, mailbox, Piece::B_PAWN, "e7");
  placePiece(bb, mailbox, Piece::B_PAWN, "f7");
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_EQUAL_INT(0, eval);
}

void test_eval_passed_pawn_bonus(void) {
  // Lone white pawn on e5 — passed (no black pawns) and isolated.
  // Without pawn structure: 100 (material) + 20 (PST) = 120 (king PSTs cancel).
  // Pawn structure adds: +25 (passed, rank bonus) -15 (isolated) = +10 net.
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "e5");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int eval = ChessEval::evaluatePosition(bb);
  TEST_ASSERT_TRUE(eval > 120);
}

void test_eval_doubled_pawns_worse(void) {
  // Position A: two white pawns on separate adjacent files (d4, e4).
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "d4");
  placePiece(bb, mailbox, Piece::W_PAWN, "e4");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int separate = ChessEval::evaluatePosition(bb);

  // Position B: doubled pawns on the e-file (e3, e4).
  clearBoard(bb, mailbox);
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "e3");
  placePiece(bb, mailbox, Piece::W_PAWN, "e4");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int doubled = ChessEval::evaluatePosition(bb);

  TEST_ASSERT_TRUE(separate > doubled);
}

void test_eval_isolated_pawns_worse(void) {
  // Position A: two white pawns on adjacent files (d4, e4) — connected.
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "d4");
  placePiece(bb, mailbox, Piece::W_PAWN, "e4");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int connected = ChessEval::evaluatePosition(bb);

  // Position B: two white pawns on distant files (a4, h4) — both isolated.
  clearBoard(bb, mailbox);
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_PAWN, "a4");
  placePiece(bb, mailbox, Piece::W_PAWN, "h4");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  int isolated = ChessEval::evaluatePosition(bb);

  TEST_ASSERT_TRUE(connected > isolated);
}

// ---------------------------------------------------------------------------
// Additional FEN/helper tests
// ---------------------------------------------------------------------------

void test_hasAnyLegalMove_in_check_with_escape(void) {
  // King in check but can escape
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_ROOK, "e8"); // rook checks
  PositionState flags{0x00, -1, -1, 0, 1};
  // King can escape to d1, d2, f1, f2
  TEST_ASSERT_TRUE(ChessRules::hasAnyLegalMove(bb, mailbox, Color::WHITE, flags));
}

// ---------------------------------------------------------------------------
// checkEnPassant
// ---------------------------------------------------------------------------

void test_checkEnPassant_double_push_sets_target(void) {
  // White pawn e2→e4 (row 6→4): EP target should be e3 (row 5, col 4)
  auto ep = ChessUtils::checkEnPassant(6, 4, 4, 4, Piece::W_PAWN, Piece::NONE);
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.capturedPawnRow);
  TEST_ASSERT_EQUAL_INT(5, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(4, ep.nextEpCol);
}

void test_checkEnPassant_single_push_no_target(void) {
  // White pawn e3→e4 (row 5→4): no EP target
  auto ep = ChessUtils::checkEnPassant(5, 4, 4, 4, Piece::W_PAWN, Piece::NONE);
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpCol);
}

void test_checkEnPassant_capture_detected(void) {
  // White pawn diagonal capture to empty square = EP capture
  auto ep = ChessUtils::checkEnPassant(3, 4, 2, 5, Piece::W_PAWN, Piece::NONE);
  TEST_ASSERT_TRUE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(3, ep.capturedPawnRow); // captured pawn is on same row as from
}

void test_checkEnPassant_normal_capture_not_ep(void) {
  // White pawn diagonal capture to occupied square = normal capture, not EP
  auto ep = ChessUtils::checkEnPassant(3, 4, 2, 5, Piece::W_PAWN, Piece::B_PAWN);
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.capturedPawnRow);
}

void test_checkEnPassant_black_double_push(void) {
  // Black pawn d7→d5 (row 1→3): EP target should be d6 (row 2, col 3)
  auto ep = ChessUtils::checkEnPassant(1, 3, 3, 3, Piece::B_PAWN, Piece::NONE);
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(2, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(3, ep.nextEpCol);
}

void test_checkEnPassant_non_pawn_no_effect(void) {
  // Knight move — should never set EP target or capture
  auto ep = ChessUtils::checkEnPassant(7, 1, 5, 2, Piece::W_KNIGHT, Piece::NONE);
  TEST_ASSERT_FALSE(ep.isCapture);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpRow);
  TEST_ASSERT_EQUAL_INT(-1, ep.nextEpCol);
}

// ---------------------------------------------------------------------------
// checkCastling
// ---------------------------------------------------------------------------

void test_checkCastling_kingside(void) {
  auto c = ChessUtils::checkCastling(7, 4, 7, 6, Piece::W_KING);
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(7, c.rookFromCol);  // h-file
  TEST_ASSERT_EQUAL_INT(5, c.rookToCol);    // f-file
}

void test_checkCastling_queenside(void) {
  auto c = ChessUtils::checkCastling(7, 4, 7, 2, Piece::W_KING);
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(0, c.rookFromCol);  // a-file
  TEST_ASSERT_EQUAL_INT(3, c.rookToCol);    // d-file
}

void test_checkCastling_black_kingside(void) {
  auto c = ChessUtils::checkCastling(0, 4, 0, 6, Piece::B_KING);
  TEST_ASSERT_TRUE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(7, c.rookFromCol);
  TEST_ASSERT_EQUAL_INT(5, c.rookToCol);
}

void test_checkCastling_not_king(void) {
  // Rook moving 2 squares is not castling
  auto c = ChessUtils::checkCastling(7, 0, 7, 2, Piece::W_ROOK);
  TEST_ASSERT_FALSE(c.isCastling);
  TEST_ASSERT_EQUAL_INT(-1, c.rookFromCol);
  TEST_ASSERT_EQUAL_INT(-1, c.rookToCol);
}

void test_checkCastling_king_one_square(void) {
  // King moving one square is not castling
  auto c = ChessUtils::checkCastling(7, 4, 7, 5, Piece::W_KING);
  TEST_ASSERT_FALSE(c.isCastling);
}

// ---------------------------------------------------------------------------
// updateCastlingRights
// ---------------------------------------------------------------------------

void test_updateCastlingRights_white_king_moves(void) {
  uint8_t rights = 0x0F;  // KQkq
  rights = ChessUtils::updateCastlingRights(rights, 7, 4, 7, 5, Piece::W_KING, Piece::NONE);
  TEST_ASSERT_EQUAL_UINT8(0x0C, rights);  // kq only
}

void test_updateCastlingRights_black_king_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 0, 4, 0, 5, Piece::B_KING, Piece::NONE);
  TEST_ASSERT_EQUAL_UINT8(0x03, rights);  // KQ only
}

void test_updateCastlingRights_white_h_rook_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 7, 7, 5, 7, Piece::W_ROOK, Piece::NONE);
  TEST_ASSERT_EQUAL_UINT8(0x0E, rights);  // Qkq (lost K)
}

void test_updateCastlingRights_white_a_rook_moves(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 7, 0, 5, 0, Piece::W_ROOK, Piece::NONE);
  TEST_ASSERT_EQUAL_UINT8(0x0D, rights);  // Kkq (lost Q)
}

void test_updateCastlingRights_rook_captured(void) {
  uint8_t rights = 0x0F;
  // Black captures white h-rook
  rights = ChessUtils::updateCastlingRights(rights, 0, 7, 7, 7, Piece::B_ROOK, Piece::W_ROOK);
  // Lost K (rook captured on h1) + lost k (black rook moved from h8)
  TEST_ASSERT_EQUAL_UINT8(0x0A, rights);  // Qq
}

void test_updateCastlingRights_no_change_on_pawn_move(void) {
  uint8_t rights = 0x0F;
  rights = ChessUtils::updateCastlingRights(rights, 6, 4, 4, 4, Piece::W_PAWN, Piece::NONE);
  TEST_ASSERT_EQUAL_UINT8(0x0F, rights);  // unchanged
}

// ---------------------------------------------------------------------------
// opponentColor / squareName / PositionState::initial
// ---------------------------------------------------------------------------

void test_opponentColor_white(void) {
  TEST_ASSERT_EQUAL_STRING("Black", ChessPiece::colorName(~Color::WHITE));
}

void test_opponentColor_black(void) {
  TEST_ASSERT_EQUAL_STRING("White", ChessPiece::colorName(~Color::BLACK));
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

// ---------------------------------------------------------------------------
// Castling rights string conversion
// ---------------------------------------------------------------------------

void test_castling_rights_to_string_all(void) {
  TEST_ASSERT_EQUAL_STRING("KQkq", ChessUtils::castlingRightsToString(0x0F).c_str());
}

void test_castling_rights_to_string_none(void) {
  TEST_ASSERT_EQUAL_STRING("-", ChessUtils::castlingRightsToString(0x00).c_str());
}

void test_castling_rights_to_string_partial(void) {
  TEST_ASSERT_EQUAL_STRING("Kk", ChessUtils::castlingRightsToString(0x05).c_str());
}

void test_castling_rights_from_string(void) {
  TEST_ASSERT_EQUAL_UINT8(0x0F, ChessUtils::castlingRightsFromString("KQkq"));
  TEST_ASSERT_EQUAL_UINT8(0x00, ChessUtils::castlingRightsFromString("-"));
  TEST_ASSERT_EQUAL_UINT8(0x01, ChessUtils::castlingRightsFromString("K"));
}

void test_castling_rights_roundtrip_all_16(void) {
  for (uint8_t i = 0; i <= 0x0F; i++) {
    std::string str = ChessUtils::castlingRightsToString(i);
    uint8_t parsed = ChessUtils::castlingRightsFromString(str);
    TEST_ASSERT_EQUAL_UINT8(i, parsed);
  }
}

void test_castling_rights_from_string_invalid(void) {
  TEST_ASSERT_EQUAL_UINT8(0x00, ChessUtils::castlingRightsFromString("XYZ"));
}

// ---------------------------------------------------------------------------
// pawnDirection
// ---------------------------------------------------------------------------

void test_pawnDirection_white(void) {
  TEST_ASSERT_EQUAL_INT(-1, ChessPiece::pawnDirection(Color::WHITE));
}

void test_pawnDirection_black(void) {
  TEST_ASSERT_EQUAL_INT(1, ChessPiece::pawnDirection(Color::BLACK));
}

// ---------------------------------------------------------------------------
// homeRow
// ---------------------------------------------------------------------------

void test_homeRow_white(void) {
  TEST_ASSERT_EQUAL_INT(7, ChessPiece::homeRow(Color::WHITE));
}

void test_homeRow_black(void) {
  TEST_ASSERT_EQUAL_INT(0, ChessPiece::homeRow(Color::BLACK));
}

// ---------------------------------------------------------------------------
// castlingCharToBit
// ---------------------------------------------------------------------------

void test_castlingCharToBit_all_four(void) {
  TEST_ASSERT_EQUAL_UINT8(0x01, ChessUtils::castlingCharToBit('K'));
  TEST_ASSERT_EQUAL_UINT8(0x02, ChessUtils::castlingCharToBit('Q'));
  TEST_ASSERT_EQUAL_UINT8(0x04, ChessUtils::castlingCharToBit('k'));
  TEST_ASSERT_EQUAL_UINT8(0x08, ChessUtils::castlingCharToBit('q'));
}

void test_castlingCharToBit_invalid(void) {
  TEST_ASSERT_EQUAL_UINT8(0, ChessUtils::castlingCharToBit('X'));
  TEST_ASSERT_EQUAL_UINT8(0, ChessUtils::castlingCharToBit(' '));
}

// ---------------------------------------------------------------------------
// hasCastlingRight
// ---------------------------------------------------------------------------

void test_hasCastlingRight_all_rights(void) {
  uint8_t all = 0x0F;
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(all, Color::WHITE, true));   // K
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(all, Color::WHITE, false));  // Q
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(all, Color::BLACK, true));   // k
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(all, Color::BLACK, false));  // q
}

void test_hasCastlingRight_no_rights(void) {
  uint8_t none = 0x00;
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(none, Color::WHITE, true));
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(none, Color::WHITE, false));
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(none, Color::BLACK, true));
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(none, Color::BLACK, false));
}

void test_hasCastlingRight_partial(void) {
  uint8_t wKbq = 0x09;  // K + q
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(wKbq, Color::WHITE, true));   // K set
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(wKbq, Color::WHITE, false)); // Q not set
  TEST_ASSERT_FALSE(ChessUtils::hasCastlingRight(wKbq, Color::BLACK, true));  // k not set
  TEST_ASSERT_TRUE(ChessUtils::hasCastlingRight(wKbq, Color::BLACK, false));  // q set
}

// ---------------------------------------------------------------------------
// Coordinate helpers: fileChar / rankChar / fileIndex / rankIndex
// ---------------------------------------------------------------------------

void test_fileChar_all_columns(void) {
  TEST_ASSERT_EQUAL_CHAR('a', ChessUtils::fileChar(0));
  TEST_ASSERT_EQUAL_CHAR('b', ChessUtils::fileChar(1));
  TEST_ASSERT_EQUAL_CHAR('c', ChessUtils::fileChar(2));
  TEST_ASSERT_EQUAL_CHAR('d', ChessUtils::fileChar(3));
  TEST_ASSERT_EQUAL_CHAR('e', ChessUtils::fileChar(4));
  TEST_ASSERT_EQUAL_CHAR('f', ChessUtils::fileChar(5));
  TEST_ASSERT_EQUAL_CHAR('g', ChessUtils::fileChar(6));
  TEST_ASSERT_EQUAL_CHAR('h', ChessUtils::fileChar(7));
}

void test_rankChar_all_rows(void) {
  TEST_ASSERT_EQUAL_CHAR('8', ChessUtils::rankChar(0));
  TEST_ASSERT_EQUAL_CHAR('7', ChessUtils::rankChar(1));
  TEST_ASSERT_EQUAL_CHAR('6', ChessUtils::rankChar(2));
  TEST_ASSERT_EQUAL_CHAR('5', ChessUtils::rankChar(3));
  TEST_ASSERT_EQUAL_CHAR('4', ChessUtils::rankChar(4));
  TEST_ASSERT_EQUAL_CHAR('3', ChessUtils::rankChar(5));
  TEST_ASSERT_EQUAL_CHAR('2', ChessUtils::rankChar(6));
  TEST_ASSERT_EQUAL_CHAR('1', ChessUtils::rankChar(7));
}

void test_fileIndex_all_files(void) {
  TEST_ASSERT_EQUAL_INT(0, ChessUtils::fileIndex('a'));
  TEST_ASSERT_EQUAL_INT(1, ChessUtils::fileIndex('b'));
  TEST_ASSERT_EQUAL_INT(4, ChessUtils::fileIndex('e'));
  TEST_ASSERT_EQUAL_INT(7, ChessUtils::fileIndex('h'));
}

void test_rankIndex_all_ranks(void) {
  TEST_ASSERT_EQUAL_INT(7, ChessUtils::rankIndex('1'));
  TEST_ASSERT_EQUAL_INT(6, ChessUtils::rankIndex('2'));
  TEST_ASSERT_EQUAL_INT(4, ChessUtils::rankIndex('4'));
  TEST_ASSERT_EQUAL_INT(0, ChessUtils::rankIndex('8'));
}

void test_coordinate_roundtrip(void) {
  for (int col = 0; col < 8; col++) {
    char f = ChessUtils::fileChar(col);
    TEST_ASSERT_EQUAL_INT(col, ChessUtils::fileIndex(f));
  }
  for (int row = 0; row < 8; row++) {
    char r = ChessUtils::rankChar(row);
    TEST_ASSERT_EQUAL_INT(row, ChessUtils::rankIndex(r));
  }
}

// ---------------------------------------------------------------------------
// isValidSquare
// ---------------------------------------------------------------------------

void test_isValidSquare_valid(void) {
  TEST_ASSERT_TRUE(ChessUtils::isValidSquare(0, 0));
  TEST_ASSERT_TRUE(ChessUtils::isValidSquare(0, 7));
  TEST_ASSERT_TRUE(ChessUtils::isValidSquare(7, 0));
  TEST_ASSERT_TRUE(ChessUtils::isValidSquare(7, 7));
  TEST_ASSERT_TRUE(ChessUtils::isValidSquare(3, 4));
}

void test_isValidSquare_invalid(void) {
  TEST_ASSERT_FALSE(ChessUtils::isValidSquare(-1, 0));
  TEST_ASSERT_FALSE(ChessUtils::isValidSquare(0, -1));
  TEST_ASSERT_FALSE(ChessUtils::isValidSquare(8, 0));
  TEST_ASSERT_FALSE(ChessUtils::isValidSquare(0, 8));
  TEST_ASSERT_FALSE(ChessUtils::isValidSquare(-1, -1));
}

// ---------------------------------------------------------------------------
// isValidPromotionChar
// ---------------------------------------------------------------------------

void test_isValidPromotionChar_valid(void) {
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('q'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('r'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('b'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('n'));
}

void test_isValidPromotionChar_uppercase(void) {
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('Q'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('R'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('B'));
  TEST_ASSERT_TRUE(ChessUtils::isValidPromotionChar('N'));
}

void test_isValidPromotionChar_invalid(void) {
  TEST_ASSERT_FALSE(ChessUtils::isValidPromotionChar('k'));
  TEST_ASSERT_FALSE(ChessUtils::isValidPromotionChar('p'));
  TEST_ASSERT_FALSE(ChessUtils::isValidPromotionChar('x'));
  TEST_ASSERT_FALSE(ChessUtils::isValidPromotionChar(' '));
}

// ---------------------------------------------------------------------------
// gameResultName
// ---------------------------------------------------------------------------

void test_gameResultName_all_values(void) {
  TEST_ASSERT_EQUAL_STRING("In progress", ChessUtils::gameResultName(GameResult::IN_PROGRESS));
  TEST_ASSERT_EQUAL_STRING("Checkmate", ChessUtils::gameResultName(GameResult::CHECKMATE));
  TEST_ASSERT_EQUAL_STRING("Stalemate", ChessUtils::gameResultName(GameResult::STALEMATE));
  TEST_ASSERT_EQUAL_STRING("Draw (50-move rule)", ChessUtils::gameResultName(GameResult::DRAW_50));
  TEST_ASSERT_EQUAL_STRING("Draw (threefold repetition)", ChessUtils::gameResultName(GameResult::DRAW_3FOLD));
  TEST_ASSERT_EQUAL_STRING("Resignation", ChessUtils::gameResultName(GameResult::RESIGNATION));
  TEST_ASSERT_EQUAL_STRING("Draw (insufficient material)", ChessUtils::gameResultName(GameResult::DRAW_INSUFFICIENT));
  TEST_ASSERT_EQUAL_STRING("Draw (agreement)", ChessUtils::gameResultName(GameResult::DRAW_AGREEMENT));
  TEST_ASSERT_EQUAL_STRING("Timeout", ChessUtils::gameResultName(GameResult::TIMEOUT));
  TEST_ASSERT_EQUAL_STRING("Aborted", ChessUtils::gameResultName(GameResult::ABORTED));
}

// ---------------------------------------------------------------------------
// boardToText
// ---------------------------------------------------------------------------

void test_boardToText_initial_position(void) {
  setupInitialBoard(bb, mailbox);
  std::string text = ChessUtils::boardToText(mailbox);
  TEST_ASSERT_TRUE(text.length() > 0);
  // Check that back ranks have expected piece chars
  TEST_ASSERT_TRUE(text.find('r') != std::string::npos);  // black rook
  TEST_ASSERT_TRUE(text.find('R') != std::string::npos);  // white rook
  TEST_ASSERT_TRUE(text.find('K') != std::string::npos);  // white king
  TEST_ASSERT_TRUE(text.find('k') != std::string::npos);  // black king
}

void test_boardToText_empty_board(void) {
  std::string text = ChessUtils::boardToText(mailbox);
  TEST_ASSERT_TRUE(text.length() > 0);
  // Empty board rows should contain only dots (no piece chars between rank labels)
  // Each rank line looks like "8 . . . . . . . .  8\n"
  for (int row = 0; row < 8; row++) {
    char rank = ChessUtils::rankChar(row);
    // Find the rank label at line start
    std::string prefix = std::string(1, rank) + " ";
    size_t pos = text.find(prefix);
    TEST_ASSERT_TRUE(pos != std::string::npos);
    // The 8 squares after the rank label should all be dots
    for (int col = 0; col < 8; col++) {
      TEST_ASSERT_EQUAL_CHAR('.', text[pos + 2 + col * 2]);
    }
  }
}

// ---------------------------------------------------------------------------
// applyBoardTransform
// ---------------------------------------------------------------------------

void test_applyBoardTransform_simple_move(void) {
  placePiece(bb, mailbox, Piece::W_PAWN, "e2");
  ChessUtils::EnPassantInfo ep{false, -1, -1, -1};
  ChessUtils::CastlingInfo castle{false, -1, -1};
  Piece captured = Piece::NONE;
  ChessUtils::applyBoardTransform(bb, mailbox, squareOf(6, 4), squareOf(4, 4), ep, castle, captured);
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, mailbox[squareOf(4, 4)]);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(6, 4)]);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, captured);
}

void test_applyBoardTransform_capture(void) {
  placePiece(bb, mailbox, Piece::W_PAWN, "e4");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");
  ChessUtils::EnPassantInfo ep{false, -1, -1, -1};
  ChessUtils::CastlingInfo castle{false, -1, -1};
  Piece captured = Piece::NONE;
  ChessUtils::applyBoardTransform(bb, mailbox, squareOf(4, 4), squareOf(3, 3), ep, castle, captured);
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, mailbox[squareOf(3, 3)]);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(4, 4)]);
  TEST_ASSERT_ENUM_EQ(Piece::B_PAWN, captured);
}

void test_applyBoardTransform_en_passant(void) {
  placePiece(bb, mailbox, Piece::W_PAWN, "e5");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");  // captured pawn on row 3
  ChessUtils::EnPassantInfo ep{true, 3, -1, -1};  // EP capture, captured pawn on row 3
  ChessUtils::CastlingInfo castle{false, -1, -1};
  Piece captured = Piece::NONE;
  ChessUtils::applyBoardTransform(bb, mailbox, squareOf(3, 4), squareOf(2, 3), ep, castle, captured);
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, mailbox[squareOf(2, 3)]);  // pawn moved to d6
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(3, 4)]);     // source cleared
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(3, 3)]);     // captured pawn removed
}

void test_applyBoardTransform_castling(void) {
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::W_ROOK, "h1");
  ChessUtils::EnPassantInfo ep{false, -1, -1, -1};
  ChessUtils::CastlingInfo castle{true, 7, 5};  // kingside: rook h1→f1
  Piece captured = Piece::NONE;
  ChessUtils::applyBoardTransform(bb, mailbox, squareOf(7, 4), squareOf(7, 6), ep, castle, captured);
  TEST_ASSERT_ENUM_EQ(Piece::W_KING, mailbox[squareOf(7, 6)]);   // king on g1
  TEST_ASSERT_ENUM_EQ(Piece::W_ROOK, mailbox[squareOf(7, 5)]);   // rook on f1
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(7, 4)]);      // e1 cleared
  TEST_ASSERT_ENUM_EQ(Piece::NONE, mailbox[squareOf(7, 7)]);      // h1 cleared
}

void register_chess_utils_tests() {
  needsDefaultKings = false;

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

  // Pawn structure evaluation
  RUN_TEST(test_eval_pawn_structure_symmetry);
  RUN_TEST(test_eval_passed_pawn_bonus);
  RUN_TEST(test_eval_doubled_pawns_worse);
  RUN_TEST(test_eval_isolated_pawns_worse);

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

  // Castling rights strings
  RUN_TEST(test_castling_rights_to_string_all);
  RUN_TEST(test_castling_rights_to_string_none);
  RUN_TEST(test_castling_rights_to_string_partial);
  RUN_TEST(test_castling_rights_from_string);
  RUN_TEST(test_castling_rights_roundtrip_all_16);
  RUN_TEST(test_castling_rights_from_string_invalid);

  // pawnDirection
  RUN_TEST(test_pawnDirection_white);
  RUN_TEST(test_pawnDirection_black);

  // homeRow
  RUN_TEST(test_homeRow_white);
  RUN_TEST(test_homeRow_black);

  // castlingCharToBit
  RUN_TEST(test_castlingCharToBit_all_four);
  RUN_TEST(test_castlingCharToBit_invalid);

  // hasCastlingRight
  RUN_TEST(test_hasCastlingRight_all_rights);
  RUN_TEST(test_hasCastlingRight_no_rights);
  RUN_TEST(test_hasCastlingRight_partial);

  // Coordinate helpers
  RUN_TEST(test_fileChar_all_columns);
  RUN_TEST(test_rankChar_all_rows);
  RUN_TEST(test_fileIndex_all_files);
  RUN_TEST(test_rankIndex_all_ranks);
  RUN_TEST(test_coordinate_roundtrip);

  // isValidSquare
  RUN_TEST(test_isValidSquare_valid);
  RUN_TEST(test_isValidSquare_invalid);

  // isValidPromotionChar
  RUN_TEST(test_isValidPromotionChar_valid);
  RUN_TEST(test_isValidPromotionChar_uppercase);
  RUN_TEST(test_isValidPromotionChar_invalid);

  // gameResultName
  RUN_TEST(test_gameResultName_all_values);

  // boardToText
  RUN_TEST(test_boardToText_initial_position);
  RUN_TEST(test_boardToText_empty_board);

  // applyBoardTransform
  RUN_TEST(test_applyBoardTransform_simple_move);
  RUN_TEST(test_applyBoardTransform_capture);
  RUN_TEST(test_applyBoardTransform_en_passant);
  RUN_TEST(test_applyBoardTransform_castling);
}
