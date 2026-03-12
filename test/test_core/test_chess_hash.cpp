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
  TEST_ASSERT_EQUAL(0, ChessPiece::pieceZobristIndex(Piece::W_PAWN));
  TEST_ASSERT_EQUAL(1, ChessPiece::pieceZobristIndex(Piece::W_KNIGHT));
  TEST_ASSERT_EQUAL(2, ChessPiece::pieceZobristIndex(Piece::W_BISHOP));
  TEST_ASSERT_EQUAL(3, ChessPiece::pieceZobristIndex(Piece::W_ROOK));
  TEST_ASSERT_EQUAL(4, ChessPiece::pieceZobristIndex(Piece::W_QUEEN));
  TEST_ASSERT_EQUAL(5, ChessPiece::pieceZobristIndex(Piece::W_KING));
}

void test_chess_hash_piece_index_black(void) {
  TEST_ASSERT_EQUAL(6, ChessPiece::pieceZobristIndex(Piece::B_PAWN));
  TEST_ASSERT_EQUAL(7, ChessPiece::pieceZobristIndex(Piece::B_KNIGHT));
  TEST_ASSERT_EQUAL(8, ChessPiece::pieceZobristIndex(Piece::B_BISHOP));
  TEST_ASSERT_EQUAL(9, ChessPiece::pieceZobristIndex(Piece::B_ROOK));
  TEST_ASSERT_EQUAL(10, ChessPiece::pieceZobristIndex(Piece::B_QUEEN));
  TEST_ASSERT_EQUAL(11, ChessPiece::pieceZobristIndex(Piece::B_KING));
}

void test_chess_hash_piece_index_invalid(void) {
  TEST_ASSERT_EQUAL(-1, ChessPiece::pieceZobristIndex(Piece::NONE));
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
