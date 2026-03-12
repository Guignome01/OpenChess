#include <unity.h>

#include <chess_hash.h>

// ---------------------------------------------------------------------------
// Key generation spot-check (constexpr PRNG matches known values)
// ---------------------------------------------------------------------------

void test_chess_hash_keys_pieces_first(void) {
  TEST_ASSERT_EQUAL_HEX64(0xF2C49D843D3F949FULL, ChessHash::KEYS.pieces[0][0]);
  TEST_ASSERT_EQUAL_HEX64(0x78FFE750EDADAAE9ULL, ChessHash::KEYS.pieces[1][0]);
  TEST_ASSERT_EQUAL_HEX64(0x44122009B38CC8AAULL, ChessHash::KEYS.pieces[11][0]);
}

void test_chess_hash_keys_pieces_last(void) {
  TEST_ASSERT_EQUAL_HEX64(0xBD8F540EF5A1C22DULL, ChessHash::KEYS.pieces[0][63]);
  TEST_ASSERT_EQUAL_HEX64(0x03F97B6B42D3E608ULL, ChessHash::KEYS.pieces[11][63]);
}

void test_chess_hash_keys_castling(void) {
  TEST_ASSERT_EQUAL_HEX64(0x5AE383CADBE6C3C4ULL, ChessHash::KEYS.castling[0]);
  TEST_ASSERT_EQUAL_HEX64(0xC2428255C16465F3ULL, ChessHash::KEYS.castling[15]);
}

void test_chess_hash_keys_en_passant(void) {
  TEST_ASSERT_EQUAL_HEX64(0xC737548EDCB1B1F8ULL, ChessHash::KEYS.enPassant[0]);
  TEST_ASSERT_EQUAL_HEX64(0x6BE449C10216D94EULL, ChessHash::KEYS.enPassant[7]);
}

void test_chess_hash_keys_side_to_move(void) {
  TEST_ASSERT_EQUAL_HEX64(0x41B86C4A1075677CULL, ChessHash::KEYS.sideToMove);
}

// ---------------------------------------------------------------------------
// pieceIndex lookup
// ---------------------------------------------------------------------------

void test_chess_hash_piece_index_white(void) {
  TEST_ASSERT_EQUAL(0, ChessHash::pieceIndex('P'));
  TEST_ASSERT_EQUAL(1, ChessHash::pieceIndex('N'));
  TEST_ASSERT_EQUAL(2, ChessHash::pieceIndex('B'));
  TEST_ASSERT_EQUAL(3, ChessHash::pieceIndex('R'));
  TEST_ASSERT_EQUAL(4, ChessHash::pieceIndex('Q'));
  TEST_ASSERT_EQUAL(5, ChessHash::pieceIndex('K'));
}

void test_chess_hash_piece_index_black(void) {
  TEST_ASSERT_EQUAL(6, ChessHash::pieceIndex('p'));
  TEST_ASSERT_EQUAL(7, ChessHash::pieceIndex('n'));
  TEST_ASSERT_EQUAL(8, ChessHash::pieceIndex('b'));
  TEST_ASSERT_EQUAL(9, ChessHash::pieceIndex('r'));
  TEST_ASSERT_EQUAL(10, ChessHash::pieceIndex('q'));
  TEST_ASSERT_EQUAL(11, ChessHash::pieceIndex('k'));
}

void test_chess_hash_piece_index_invalid(void) {
  TEST_ASSERT_EQUAL(-1, ChessHash::pieceIndex(' '));
  TEST_ASSERT_EQUAL(-1, ChessHash::pieceIndex('x'));
  TEST_ASSERT_EQUAL(-1, ChessHash::pieceIndex('0'));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_hash_tests() {
  // Key generation
  RUN_TEST(test_chess_hash_keys_pieces_first);
  RUN_TEST(test_chess_hash_keys_pieces_last);
  RUN_TEST(test_chess_hash_keys_castling);
  RUN_TEST(test_chess_hash_keys_en_passant);
  RUN_TEST(test_chess_hash_keys_side_to_move);

  // pieceIndex
  RUN_TEST(test_chess_hash_piece_index_white);
  RUN_TEST(test_chess_hash_piece_index_black);
  RUN_TEST(test_chess_hash_piece_index_invalid);
}
