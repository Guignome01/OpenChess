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

// ---------------------------------------------------------------------------
// Additional UCI edge cases
// ---------------------------------------------------------------------------

void test_parseUCIMove_invalid_rank(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("e9e4", fr, fc, tr, tc, promo));
}

void test_parseUCIMove_empty_string(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_FALSE(ChessCodec::parseUCIMove("", fr, fc, tr, tc, promo));
}

void test_parseUCIMove_uppercase_promotion(void) {
  int fr, fc, tr, tc;
  char promo;
  TEST_ASSERT_TRUE(ChessCodec::parseUCIMove("e7e8Q", fr, fc, tr, tc, promo));
  TEST_ASSERT_EQUAL_CHAR('Q', promo); // uppercase preserved (not normalized)
}

void test_toUCIMove_all_promo_chars(void) {
  TEST_ASSERT_EQUAL_STRING("e7e8r", ChessCodec::toUCIMove(1, 4, 0, 4, 'r').c_str());
  TEST_ASSERT_EQUAL_STRING("e7e8b", ChessCodec::toUCIMove(1, 4, 0, 4, 'b').c_str());
  TEST_ASSERT_EQUAL_STRING("e7e8n", ChessCodec::toUCIMove(1, 4, 0, 4, 'n').c_str());
}

// ---------------------------------------------------------------------------
// Compact encode/decode: rook & bishop promotions
// ---------------------------------------------------------------------------

void test_encodeMove_rook_promotion(void) {
  uint16_t encoded = ChessCodec::encodeMove(1, 4, 0, 4, 'r');
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(1, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(0, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR('r', promo);
}

void test_encodeMove_bishop_promotion(void) {
  uint16_t encoded = ChessCodec::encodeMove(1, 4, 0, 4, 'b');
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(encoded, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_INT(1, fr);
  TEST_ASSERT_EQUAL_INT(4, fc);
  TEST_ASSERT_EQUAL_INT(0, tr);
  TEST_ASSERT_EQUAL_INT(4, tc);
  TEST_ASSERT_EQUAL_CHAR('b', promo);
}

// ---------------------------------------------------------------------------
// Castling rights: roundtrip all 16 bitmask values
// ---------------------------------------------------------------------------

void test_castling_rights_roundtrip_all_16(void) {
  for (uint8_t i = 0; i <= 0x0F; i++) {
    std::string str = ChessCodec::castlingRightsToString(i);
    uint8_t parsed = ChessCodec::castlingRightsFromString(str);
    TEST_ASSERT_EQUAL_UINT8(i, parsed);
  }
}

void test_castling_rights_from_string_invalid(void) {
  TEST_ASSERT_EQUAL_UINT8(0x00, ChessCodec::castlingRightsFromString("XYZ"));
}

// ---------------------------------------------------------------------------
// On-disk format stability (values must never change)
// ---------------------------------------------------------------------------

void test_game_result_pinned_values(void) {
  // GameResult enum values are stored in binary game headers (FORMAT_VERSION 1).
  // Changing any value breaks backward compatibility with saved games.
  TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(GameResult::IN_PROGRESS));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(GameResult::CHECKMATE));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(GameResult::STALEMATE));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(GameResult::DRAW_50));
  TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(GameResult::DRAW_3FOLD));
  TEST_ASSERT_EQUAL_UINT8(5, static_cast<uint8_t>(GameResult::RESIGNATION));
  TEST_ASSERT_EQUAL_UINT8(6, static_cast<uint8_t>(GameResult::DRAW_INSUFFICIENT));
  TEST_ASSERT_EQUAL_UINT8(7, static_cast<uint8_t>(GameResult::DRAW_AGREEMENT));
  TEST_ASSERT_EQUAL_UINT8(8, static_cast<uint8_t>(GameResult::TIMEOUT));
  TEST_ASSERT_EQUAL_UINT8(9, static_cast<uint8_t>(GameResult::ABORTED));
}

void test_game_mode_pinned_values(void) {
  // GameModeId enum values are stored in binary game headers.
  TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(GameModeId::NONE));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(GameModeId::CHESS_MOVES));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(GameModeId::BOT));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(GameModeId::LICHESS));
}

void test_fen_marker_no_collision(void) {
  // FEN_MARKER (0xFFFF) must not be a valid encoded move.
  // encodeMove packs (from << 10 | to << 4 | promo).
  // Max from=63 (0x3F), max to=63, max promo=4 → 0xFFF4.
  // FEN_MARKER=0xFFFF can never be produced by encodeMove.
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, FEN_MARKER);

  // Verify all corner cases of encodeMove are below FEN_MARKER
  uint16_t maxEncoded = ChessCodec::encodeMove(7, 7, 7, 7, 'n'); // max coords + max promo
  TEST_ASSERT_TRUE(maxEncoded < FEN_MARKER);
}

void test_game_header_size(void) {
  // GameHeader is a packed struct stored directly on flash.
  // Size must remain exactly 16 bytes for format compatibility.
  TEST_ASSERT_EQUAL(16, (int)sizeof(GameHeader));
}

void register_chess_codec_tests() {
  needsDefaultKings = false;

  // UCI
  RUN_TEST(test_toUCIMove_basic);
  RUN_TEST(test_toUCIMove_with_promotion);
  RUN_TEST(test_toUCIMove_all_promo_chars);
  RUN_TEST(test_parseUCIMove_basic);
  RUN_TEST(test_parseUCIMove_with_promotion);
  RUN_TEST(test_parseUCIMove_invalid_too_short);
  RUN_TEST(test_parseUCIMove_invalid_same_square);
  RUN_TEST(test_parseUCIMove_invalid_file);
  RUN_TEST(test_parseUCIMove_invalid_promotion);
  RUN_TEST(test_parseUCIMove_invalid_rank);
  RUN_TEST(test_parseUCIMove_empty_string);
  RUN_TEST(test_parseUCIMove_uppercase_promotion);

  // Castling rights strings
  RUN_TEST(test_castling_rights_to_string_all);
  RUN_TEST(test_castling_rights_to_string_none);
  RUN_TEST(test_castling_rights_to_string_partial);
  RUN_TEST(test_castling_rights_from_string);
  RUN_TEST(test_castling_rights_roundtrip_all_16);
  RUN_TEST(test_castling_rights_from_string_invalid);

  // Compact encode/decode
  RUN_TEST(test_encodeMove_rook_promotion);
  RUN_TEST(test_encodeMove_bishop_promotion);

  // On-disk format stability
  RUN_TEST(test_game_result_pinned_values);
  RUN_TEST(test_game_mode_pinned_values);
  RUN_TEST(test_fen_marker_no_collision);
  RUN_TEST(test_game_header_size);
}
