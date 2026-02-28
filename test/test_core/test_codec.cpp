#include <unity.h>

#include "../test_helpers.h"

extern char board[8][8];
extern bool needsDefaultKings;

// ---------------------------------------------------------------------------
// UCI move conversion
// ---------------------------------------------------------------------------

void test_toUCIMove_basic(void) {
  std::string uci = ChessCodec::toUCIMove(6, 4, 4, 4); // e2e4
  TEST_ASSERT_EQUAL_STRING("e2e4", uci.c_str());
}

void test_toUCIMove_with_promotion(void) {
  std::string uci = ChessCodec::toUCIMove(1, 4, 0, 4, 'q'); // e7e8q
  TEST_ASSERT_EQUAL_STRING("e7e8q", uci.c_str());
}

void test_parseUCIMove_basic(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_TRUE(ChessCodec::parseUCIMove("e2e4", fr, fc, tr, tc, promo));
  TEST_ASSERT_EQUAL_INT(6, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(4, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR(' ', promo);
}

void test_parseUCIMove_with_promotion(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_TRUE(ChessCodec::parseUCIMove("e7e8q", fr, fc, tr, tc, promo));
  TEST_ASSERT_EQUAL_INT(1, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(0, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR('q', promo);
}

void test_parseUCIMove_invalid_too_short(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("e2", fr, fc, tr, tc, promo));
}

void test_parseUCIMove_invalid_same_square(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("e2e2", fr, fc, tr, tc, promo));
}

void test_parseUCIMove_invalid_file(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("z2e4", fr, fc, tr, tc, promo));
}

void test_parseUCIMove_invalid_promotion(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("e7e8x", fr, fc, tr, tc, promo)); // x not valid
}

// ---------------------------------------------------------------------------
// Castling rights string conversion
// ---------------------------------------------------------------------------

void test_castling_rights_to_string_all(void) {
  TEST_ASSERT_EQUAL_STRING("KQkq", ChessCodec::castlingRightsToString(0x0F).c_str());
}

void test_castling_rights_to_string_none(void) {
  TEST_ASSERT_EQUAL_STRING("-", ChessCodec::castlingRightsToString(0x00).c_str());
}

void test_castling_rights_to_string_partial(void) {
  TEST_ASSERT_EQUAL_STRING("Kk", ChessCodec::castlingRightsToString(0x05).c_str());
}

void test_castling_rights_from_string(void) {
  TEST_ASSERT_EQUAL_UINT8(0x0F, ChessCodec::castlingRightsFromString("KQkq"));
  TEST_ASSERT_EQUAL_UINT8(0x00, ChessCodec::castlingRightsFromString("-"));
  TEST_ASSERT_EQUAL_UINT8(0x01, ChessCodec::castlingRightsFromString("K"));
}

void register_codec_tests() {
  needsDefaultKings = false;

  // UCI
  RUN_TEST(test_toUCIMove_basic);
  RUN_TEST(test_toUCIMove_with_promotion);
  RUN_TEST(test_parseUCIMove_basic);
  RUN_TEST(test_parseUCIMove_with_promotion);
  RUN_TEST(test_parseUCIMove_invalid_too_short);
  RUN_TEST(test_parseUCIMove_invalid_same_square);
  RUN_TEST(test_parseUCIMove_invalid_file);
  RUN_TEST(test_parseUCIMove_invalid_promotion);

  // Castling rights strings
  RUN_TEST(test_castling_rights_to_string_all);
  RUN_TEST(test_castling_rights_to_string_none);
  RUN_TEST(test_castling_rights_to_string_partial);
  RUN_TEST(test_castling_rights_from_string);
}
