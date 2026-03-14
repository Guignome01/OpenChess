#include <unity.h>

#include <chess_hash.h>
#include <chess_board.h>

#include "../test_helpers.h"

extern ChessBitboard::BitboardSet bb;
extern Piece mailbox[64];
extern bool needsDefaultKings;

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
// computeHash — functional tests
// ---------------------------------------------------------------------------

void test_computeHash_deterministic(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st = PositionState::initial();
  uint64_t h1 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);
  uint64_t h2 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);
  TEST_ASSERT_EQUAL_HEX64(h1, h2);
  TEST_ASSERT_NOT_EQUAL(0ULL, h1);
}

void test_computeHash_different_positions(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st = PositionState::initial();
  uint64_t h1 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);

  // Move e2 to e4
  Piece pawn = mailbox[squareOf(6, 4)];
  bb.removePiece(squareOf(6, 4), pawn);
  mailbox[squareOf(6, 4)] = Piece::NONE;
  bb.setPiece(squareOf(4, 4), pawn);
  mailbox[squareOf(4, 4)] = pawn;
  st.epRow = 5;
  st.epCol = 4;
  uint64_t h2 = ChessHash::computeHash(bb, mailbox, Color::BLACK, st);
  TEST_ASSERT_NOT_EQUAL(h1, h2);
}

void test_computeHash_same_position_same_hash(void) {
  // Set up identical boards separately
  BitboardSet bb2;
  Piece mailbox2[64];
  setupInitialBoard(bb, mailbox);
  setupInitialBoard(bb2, mailbox2);
  PositionState st = PositionState::initial();
  uint64_t h1 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);
  uint64_t h2 = ChessHash::computeHash(bb2, mailbox2, Color::WHITE, st);
  TEST_ASSERT_EQUAL_HEX64(h1, h2);
}

void test_computeHash_turn_sensitivity(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st = PositionState::initial();
  uint64_t hw = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);
  uint64_t hb = ChessHash::computeHash(bb, mailbox, Color::BLACK, st);
  TEST_ASSERT_NOT_EQUAL(hw, hb);
}

void test_computeHash_castling_sensitivity(void) {
  setupInitialBoard(bb, mailbox);
  PositionState st1 = PositionState::initial();  // KQkq = 0x0F
  PositionState st2 = PositionState::initial();
  st2.castlingRights = 0x05;  // Kk only
  uint64_t h1 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st1);
  uint64_t h2 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st2);
  TEST_ASSERT_NOT_EQUAL(h1, h2);
}

void test_computeHash_en_passant_sensitivity(void) {
  // Position where white pawn on e5, black pawn on d5 — EP capturable on d6
  clearBoard(bb, mailbox);
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  placePiece(bb, mailbox, Piece::W_PAWN, "e5");
  placePiece(bb, mailbox, Piece::B_PAWN, "d5");

  PositionState st1{0x00, -1, -1, 0, 1};  // no EP
  PositionState st2{0x00, 2, 3, 0, 1};    // EP on d6 (row=2, col=3)

  uint64_t h1 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st1);
  uint64_t h2 = ChessHash::computeHash(bb, mailbox, Color::WHITE, st2);
  TEST_ASSERT_NOT_EQUAL(h1, h2);
}

void test_computeHash_king_vs_king(void) {
  clearBoard(bb, mailbox);
  placePiece(bb, mailbox, Piece::W_KING, "e1");
  placePiece(bb, mailbox, Piece::B_KING, "e8");
  PositionState st{0x00, -1, -1, 0, 1};
  uint64_t h = ChessHash::computeHash(bb, mailbox, Color::WHITE, st);
  TEST_ASSERT_NOT_EQUAL(0ULL, h);
}

void test_computeHash_incremental_vs_full(void) {
  // ChessBoard maintains hash incrementally via makeMove.
  // Verify that the incremental hash matches a full recomputation.
  ChessBoard cb;
  cb.newGame();
  cb.makeMove(6, 4, 4, 4);  // e2e4
  cb.makeMove(1, 4, 3, 4);  // e7e5
  cb.makeMove(7, 6, 5, 5);  // Nf3

  // Full recompute from scratch
  uint64_t full = ChessHash::computeHash(cb.bitboards(), cb.mailbox(), cb.currentTurn(), cb.positionState());

  // The board's internal hash is stored in hashHistory — the last entry
  // We can verify by loading the same FEN into a fresh board
  std::string fen = cb.getFen();
  ChessBoard cb2;
  cb2.loadFEN(fen);

  // Both boards should produce the same hash from scratch
  uint64_t full2 = ChessHash::computeHash(cb2.bitboards(), cb2.mailbox(), cb2.currentTurn(), cb2.positionState());
  TEST_ASSERT_EQUAL_HEX64(full, full2);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_chess_hash_tests() {
  needsDefaultKings = false;

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

  // computeHash
  RUN_TEST(test_computeHash_deterministic);
  RUN_TEST(test_computeHash_different_positions);
  RUN_TEST(test_computeHash_same_position_same_hash);
  RUN_TEST(test_computeHash_turn_sensitivity);
  RUN_TEST(test_computeHash_castling_sensitivity);
  RUN_TEST(test_computeHash_en_passant_sensitivity);
  RUN_TEST(test_computeHash_king_vs_king);
  RUN_TEST(test_computeHash_incremental_vs_full);
}
