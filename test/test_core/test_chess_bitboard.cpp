#include <unity.h>

#include <chess_bitboard.h>
#include <chess_attacks.h>

#include "../test_helpers.h"

using namespace ChessBitboard;
using namespace ChessAttacks;

// ---------------------------------------------------------------------------
// Helper: build a Bitboard from a list of squares
// ---------------------------------------------------------------------------

static Bitboard bbFromSquares(const Square squares[], int count) {
  Bitboard bb = 0;
  for (int i = 0; i < count; ++i) bb |= squareBB(squares[i]);
  return bb;
}

// ---------------------------------------------------------------------------
// Square mapping
// ---------------------------------------------------------------------------

static void test_squareOf_corners(void) {
  // Row 0, col 0 = a8 (black's QR corner)
  TEST_ASSERT_EQUAL_INT(SQ_A8, squareOf(0, 0));
  // Row 0, col 7 = h8
  TEST_ASSERT_EQUAL_INT(SQ_H8, squareOf(0, 7));
  // Row 7, col 0 = a1 (white's QR corner)
  TEST_ASSERT_EQUAL_INT(SQ_A1, squareOf(7, 0));
  // Row 7, col 7 = h1
  TEST_ASSERT_EQUAL_INT(SQ_H1, squareOf(7, 7));
}

static void test_squareOf_e4(void) {
  // e4 = row 4, col 4 in the existing system
  // LERF: e4 = rank 4, file e = (4-1)*8 + 4 = 28
  Square e4 = squareOf(4, 4);
  TEST_ASSERT_EQUAL_INT(28, e4);
}

static void test_rowOf_colOf_roundtrip(void) {
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      Square sq = squareOf(row, col);
      TEST_ASSERT_EQUAL_INT(row, rowOf(sq));
      TEST_ASSERT_EQUAL_INT(col, colOf(sq));
    }
  }
}

static void test_squareOf_identity_roundtrip(void) {
  // Every square 0–63 survives the roundtrip
  for (Square sq = 0; sq < 64; ++sq) {
    TEST_ASSERT_EQUAL_INT(sq, squareOf(rowOf(sq), colOf(sq)));
  }
}

// ---------------------------------------------------------------------------
// Bit manipulation
// ---------------------------------------------------------------------------

static void test_popcount_basic(void) {
  TEST_ASSERT_EQUAL_INT(0, popcount(0));
  TEST_ASSERT_EQUAL_INT(1, popcount(1));
  TEST_ASSERT_EQUAL_INT(64, popcount(~0ULL));
  TEST_ASSERT_EQUAL_INT(8, popcount(RANK_1));
  TEST_ASSERT_EQUAL_INT(8, popcount(FILE_A));
}

static void test_lsb_basic(void) {
  TEST_ASSERT_EQUAL_INT(0, lsb(1ULL));
  TEST_ASSERT_EQUAL_INT(63, lsb(1ULL << 63));
  TEST_ASSERT_EQUAL_INT(3, lsb(0b11111000ULL));
}

static void test_popLsb_drains(void) {
  Bitboard bb = squareBB(10) | squareBB(20) | squareBB(40);
  TEST_ASSERT_EQUAL_INT(3, popcount(bb));

  Square s1 = popLsb(bb);
  TEST_ASSERT_EQUAL_INT(10, s1);
  TEST_ASSERT_EQUAL_INT(2, popcount(bb));

  Square s2 = popLsb(bb);
  TEST_ASSERT_EQUAL_INT(20, s2);

  Square s3 = popLsb(bb);
  TEST_ASSERT_EQUAL_INT(40, s3);

  TEST_ASSERT_EQUAL_UINT64(0, bb);
}

// ---------------------------------------------------------------------------
// File/rank masks
// ---------------------------------------------------------------------------

static void test_file_rank_masks(void) {
  TEST_ASSERT_EQUAL_INT(8, popcount(FILE_A));
  TEST_ASSERT_EQUAL_INT(8, popcount(FILE_H));
  TEST_ASSERT_EQUAL_INT(8, popcount(RANK_1));
  TEST_ASSERT_EQUAL_INT(8, popcount(RANK_8));
  // FILE_A and FILE_H should not overlap
  TEST_ASSERT_EQUAL_UINT64(0, FILE_A & FILE_H);
  // RANK_1 and RANK_8 should not overlap
  TEST_ASSERT_EQUAL_UINT64(0, RANK_1 & RANK_8);
}

// ---------------------------------------------------------------------------
// Directional shifts
// ---------------------------------------------------------------------------

static void test_shift_north_south(void) {
  Bitboard a1 = squareBB(SQ_A1);
  Bitboard a2 = shiftNorth(a1);
  TEST_ASSERT_EQUAL_UINT64(squareBB(squareOf(6, 0)), a2);  // a2
  TEST_ASSERT_EQUAL_UINT64(a1, shiftSouth(a2));
}

static void test_shift_east_no_wrap(void) {
  // h1 shifted east should produce 0 (not wrap to a2)
  Bitboard h1 = squareBB(SQ_H1);
  TEST_ASSERT_EQUAL_UINT64(0, shiftEast(h1));
}

static void test_shift_west_no_wrap(void) {
  // a1 shifted west should produce 0 (not wrap to h-something)
  Bitboard a1 = squareBB(SQ_A1);
  TEST_ASSERT_EQUAL_UINT64(0, shiftWest(a1));
}

// ---------------------------------------------------------------------------
// BitboardSet operations
// ---------------------------------------------------------------------------

static void test_bitboardset_setPiece_removePiece(void) {
  BitboardSet bb;
  Square e4 = squareOf(4, 4);

  bb.setPiece(e4, Piece::W_KNIGHT);
  TEST_ASSERT_EQUAL_UINT64(squareBB(e4), bb.occupied);
  TEST_ASSERT_EQUAL_UINT64(squareBB(e4), bb.byColor[0]);  // WHITE
  TEST_ASSERT_EQUAL_UINT64(0, bb.byColor[1]);              // BLACK
  TEST_ASSERT_EQUAL_INT(1, popcount(bb.occupied));

  // pieceOn should find it
  Piece found = bb.pieceOn(e4);
  TEST_ASSERT_ENUM_EQ(Piece::W_KNIGHT, found);

  bb.removePiece(e4, Piece::W_KNIGHT);
  TEST_ASSERT_EQUAL_UINT64(0, bb.occupied);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, bb.pieceOn(e4));
}

static void test_bitboardset_movePiece(void) {
  BitboardSet bb;
  Square e2 = squareOf(6, 4);  // e2
  Square e4 = squareOf(4, 4);  // e4

  bb.setPiece(e2, Piece::W_PAWN);
  bb.movePiece(e2, e4, Piece::W_PAWN);

  TEST_ASSERT_EQUAL_UINT64(squareBB(e4), bb.occupied);
  TEST_ASSERT_ENUM_EQ(Piece::NONE, bb.pieceOn(e2));
  TEST_ASSERT_ENUM_EQ(Piece::W_PAWN, bb.pieceOn(e4));
}

static void test_bitboardset_multiple_pieces(void) {
  BitboardSet bb;
  Square e1 = squareOf(7, 4);
  Square d8 = squareOf(0, 3);

  bb.setPiece(e1, Piece::W_KING);
  bb.setPiece(d8, Piece::B_QUEEN);

  TEST_ASSERT_EQUAL_INT(2, popcount(bb.occupied));
  TEST_ASSERT_EQUAL_INT(1, popcount(bb.byColor[0]));  // 1 white piece
  TEST_ASSERT_EQUAL_INT(1, popcount(bb.byColor[1]));  // 1 black piece
  TEST_ASSERT_ENUM_EQ(Piece::W_KING, bb.pieceOn(e1));
  TEST_ASSERT_ENUM_EQ(Piece::B_QUEEN, bb.pieceOn(d8));
}

static void test_bitboardset_clear(void) {
  BitboardSet bb;
  bb.setPiece(squareOf(0, 0), Piece::B_ROOK);
  bb.setPiece(squareOf(7, 7), Piece::W_ROOK);
  bb.clear();
  TEST_ASSERT_EQUAL_UINT64(0, bb.occupied);
  for (int i = 0; i < 12; ++i)
    TEST_ASSERT_EQUAL_UINT64(0, bb.byPiece[i]);
}

// ---------------------------------------------------------------------------
// Knight attacks
// ---------------------------------------------------------------------------

static void test_knight_attacks_e4(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);  // LERF: 28
  Bitboard attacks = KNIGHT_ATTACKS[e4];

  // Knight on e4 should attack: d2, f2, c3, g3, c5, g5, d6, f6
  Square expected[] = {
    squareOf(6, 3),  // d2
    squareOf(6, 5),  // f2
    squareOf(5, 2),  // c3
    squareOf(5, 6),  // g3
    squareOf(3, 2),  // c5
    squareOf(3, 6),  // g5
    squareOf(2, 3),  // d6
    squareOf(2, 5),  // f6
  };
  TEST_ASSERT_EQUAL_UINT64(bbFromSquares(expected, 8), attacks);
  TEST_ASSERT_EQUAL_INT(8, popcount(attacks));
}

static void test_knight_attacks_a1_corner(void) {
  initAttacks();
  Bitboard attacks = KNIGHT_ATTACKS[SQ_A1];

  // Knight on a1: only b3 and c2
  Square expected[] = {
    squareOf(5, 1),  // b3
    squareOf(6, 2),  // c2
  };
  TEST_ASSERT_EQUAL_UINT64(bbFromSquares(expected, 2), attacks);
  TEST_ASSERT_EQUAL_INT(2, popcount(attacks));
}

// ---------------------------------------------------------------------------
// King attacks
// ---------------------------------------------------------------------------

static void test_king_attacks_e4(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Bitboard attacks = KING_ATTACKS[e4];

  // King on e4: d3, e3, f3, d4, f4, d5, e5, f5
  TEST_ASSERT_EQUAL_INT(8, popcount(attacks));
  // Verify a few specific squares
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(5, 3))) != 0);  // d3
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(3, 5))) != 0);  // f5
}

static void test_king_attacks_a1_corner(void) {
  initAttacks();
  Bitboard attacks = KING_ATTACKS[SQ_A1];

  // King on a1: a2, b1, b2
  Square expected[] = {
    squareOf(6, 0),  // a2
    squareOf(7, 1),  // b1
    squareOf(6, 1),  // b2
  };
  TEST_ASSERT_EQUAL_UINT64(bbFromSquares(expected, 3), attacks);
}

// ---------------------------------------------------------------------------
// Pawn attacks
// ---------------------------------------------------------------------------

static void test_pawn_attacks_white_e4(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Bitboard attacks = PAWN_ATTACKS[0][e4];  // WHITE

  // White pawn on e4 attacks d5 and f5
  Square expected[] = {
    squareOf(3, 3),  // d5
    squareOf(3, 5),  // f5
  };
  TEST_ASSERT_EQUAL_UINT64(bbFromSquares(expected, 2), attacks);
}

static void test_pawn_attacks_black_e5(void) {
  initAttacks();
  Square e5 = squareOf(3, 4);
  Bitboard attacks = PAWN_ATTACKS[1][e5];  // BLACK

  // Black pawn on e5 attacks d4 and f4
  Square expected[] = {
    squareOf(4, 3),  // d4
    squareOf(4, 5),  // f4
  };
  TEST_ASSERT_EQUAL_UINT64(bbFromSquares(expected, 2), attacks);
}

static void test_pawn_attacks_a_file_no_wrap(void) {
  initAttacks();
  Square a4 = squareOf(4, 0);

  // White pawn on a4: only b5 (no wrap to h-file)
  Bitboard whiteAtk = PAWN_ATTACKS[0][a4];
  TEST_ASSERT_EQUAL_INT(1, popcount(whiteAtk));
  TEST_ASSERT_TRUE((whiteAtk & squareBB(squareOf(3, 1))) != 0);  // b5
}

// ---------------------------------------------------------------------------
// Rook attacks (slider)
// ---------------------------------------------------------------------------

static void test_rook_attacks_e4_empty(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);

  // Empty board: rook on e4 attacks full file e + full rank 4, minus e4 itself
  Bitboard attacks = rookAttacks(e4, 0);

  // Should have 14 squares: 7 on file + 7 on rank
  TEST_ASSERT_EQUAL_INT(14, popcount(attacks));

  // Self-square should NOT be attacked
  TEST_ASSERT_EQUAL_UINT64(0, attacks & squareBB(e4));

  // e1 and e8 should be attacked (end of file)
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(7, 4))) != 0);  // e1
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(0, 4))) != 0);  // e8

  // a4 and h4 should be attacked (end of rank)
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(4, 0))) != 0);  // a4
  TEST_ASSERT_TRUE((attacks & squareBB(squareOf(4, 7))) != 0);  // h4
}

static void test_rook_attacks_e4_with_blocker(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Square e7 = squareOf(1, 4);

  // Blocker on e7: rook on e4 should reach e7 but not e8
  Bitboard occupied = squareBB(e4) | squareBB(e7);
  Bitboard attacks = rookAttacks(e4, occupied);

  TEST_ASSERT_TRUE((attacks & squareBB(e7)) != 0);             // blocker included
  TEST_ASSERT_EQUAL_UINT64(0, attacks & squareBB(squareOf(0, 4)));  // e8 blocked
}

static void test_rook_attacks_a1_empty(void) {
  initAttacks();
  Bitboard attacks = rookAttacks(SQ_A1, 0);

  // Rook on a1, empty board: 7 (rank 1) + 7 (file a) = 14
  TEST_ASSERT_EQUAL_INT(14, popcount(attacks));
}

// ---------------------------------------------------------------------------
// Bishop attacks (slider)
// ---------------------------------------------------------------------------

static void test_bishop_attacks_e4_empty(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Bitboard attacks = bishopAttacks(e4, 0);

  // Bishop on e4, empty board: NE(3) + NW(4) + SE(3) + SW(4) = ... let's count
  // NE: f5, g6, h7 (3)
  // NW: d5, c6, b7, a8 (4)
  // SE: f3, g2, h1 (3)
  // SW: d3, c2, b1 (3)
  // Total: 13
  TEST_ASSERT_EQUAL_INT(13, popcount(attacks));

  // Self-square not attacked
  TEST_ASSERT_EQUAL_UINT64(0, attacks & squareBB(e4));
}

static void test_bishop_attacks_with_blocker(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Square g6 = squareOf(2, 6);

  // Blocker on g6: NE ray should hit f5, g6 but not h7
  Bitboard occupied = squareBB(e4) | squareBB(g6);
  Bitboard attacks = bishopAttacks(e4, occupied);

  Square f5 = squareOf(3, 5);
  Square h7 = squareOf(1, 7);

  TEST_ASSERT_TRUE((attacks & squareBB(f5)) != 0);   // f5 reached
  TEST_ASSERT_TRUE((attacks & squareBB(g6)) != 0);   // g6 (blocker) included
  TEST_ASSERT_EQUAL_UINT64(0, attacks & squareBB(h7));  // h7 blocked
}

static void test_bishop_attacks_a1_empty(void) {
  initAttacks();
  Bitboard attacks = bishopAttacks(SQ_A1, 0);

  // a1 corner: only NE diagonal — b2, c3, d4, e5, f6, g7, h8 = 7
  TEST_ASSERT_EQUAL_INT(7, popcount(attacks));
}

// ---------------------------------------------------------------------------
// Queen attacks (combined)
// ---------------------------------------------------------------------------

static void test_queen_attacks_e4_empty(void) {
  initAttacks();
  Square e4 = squareOf(4, 4);
  Bitboard attacks = queenAttacks(e4, 0);

  // Rook: 14 + Bishop: 13 = 27
  TEST_ASSERT_EQUAL_INT(27, popcount(attacks));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// X-ray attacks (used for pin detection in Phase 2)
static void test_xray_rook_north_through_blocker(void) {
  // King on e1, friendly rook on e4, enemy rook on e8.
  // Regular rook attack from e1 stops at e4. X-ray passes through to e8.
  Square e1 = squareOf(7, 4), e4 = squareOf(4, 4), e8 = squareOf(0, 4);
  Bitboard occ = squareBB(e1) | squareBB(e4) | squareBB(e8);
  Bitboard friendly = squareBB(e4);

  Bitboard regular = rookAttacks(e1, occ);
  Bitboard xray = xrayRookAttacks(occ, friendly, e1);

  TEST_ASSERT_TRUE(regular & squareBB(e4));   // e4 visible normally
  TEST_ASSERT_FALSE(regular & squareBB(e8));  // e8 blocked by e4
  TEST_ASSERT_TRUE(xray & squareBB(e8));      // e8 visible through xray
}

static void test_xray_rook_no_friendly_blocker(void) {
  // With no friendly pieces, xray equals regular rook attacks.
  Square e4 = squareOf(4, 4);
  Bitboard occ = squareBB(e4);
  TEST_ASSERT_EQUAL_UINT64(rookAttacks(e4, occ), xrayRookAttacks(occ, 0, e4));
}

static void test_xray_bishop_diagonal_through_blocker(void) {
  // King on a1, friendly bishop on c3, enemy queen on e5.
  // Regular bishop attack from a1 stops at c3. X-ray passes through to e5.
  Square a1 = squareOf(7, 0), c3 = squareOf(5, 2), e5 = squareOf(3, 4);
  Bitboard occ = squareBB(a1) | squareBB(c3) | squareBB(e5);
  Bitboard friendly = squareBB(c3);

  Bitboard regular = bishopAttacks(a1, occ);
  Bitboard xray = xrayBishopAttacks(occ, friendly, a1);

  TEST_ASSERT_TRUE(regular & squareBB(c3));   // c3 visible normally
  TEST_ASSERT_FALSE(regular & squareBB(e5));  // e5 blocked by c3
  TEST_ASSERT_TRUE(xray & squareBB(e5));      // e5 visible through xray
}

static void test_ray_between_same_file(void) {
  Square e1 = squareOf(7, 4), e4 = squareOf(4, 4);
  Square e2 = squareOf(6, 4), e3 = squareOf(5, 4);
  Bitboard between = rayBetween(e1, e4);
  TEST_ASSERT_TRUE(between & squareBB(e2));
  TEST_ASSERT_TRUE(between & squareBB(e3));
  TEST_ASSERT_FALSE(between & squareBB(e1));  // endpoint excluded
  TEST_ASSERT_FALSE(between & squareBB(e4));  // endpoint excluded
  TEST_ASSERT_EQUAL_INT(2, popcount(between));
}

static void test_ray_between_same_rank(void) {
  Square a1 = squareOf(7, 0), d1 = squareOf(7, 3);
  Square b1 = squareOf(7, 1), c1 = squareOf(7, 2);
  Bitboard between = rayBetween(a1, d1);
  TEST_ASSERT_TRUE(between & squareBB(b1));
  TEST_ASSERT_TRUE(between & squareBB(c1));
  TEST_ASSERT_EQUAL_INT(2, popcount(between));
}

static void test_ray_between_same_diagonal(void) {
  // a1–c3 diagonal: b2 lies between them.
  Square a1 = squareOf(7, 0), c3 = squareOf(5, 2), b2 = squareOf(6, 1);
  Bitboard between = rayBetween(a1, c3);
  TEST_ASSERT_TRUE(between & squareBB(b2));
  TEST_ASSERT_EQUAL_INT(1, popcount(between));
}

static void test_ray_between_anti_diagonal(void) {
  // h1–f3 anti-diagonal: g2 lies between them.
  Square h1 = squareOf(7, 7), f3 = squareOf(5, 5), g2 = squareOf(6, 6);
  Bitboard between = rayBetween(h1, f3);
  TEST_ASSERT_TRUE(between & squareBB(g2));
  TEST_ASSERT_EQUAL_INT(1, popcount(between));
}

static void test_ray_between_adjacent_returns_zero(void) {
  Square e1 = squareOf(7, 4), e2 = squareOf(6, 4);
  TEST_ASSERT_EQUAL_UINT64(0, rayBetween(e1, e2));
}

static void test_ray_between_not_colinear_returns_zero(void) {
  // a1 and b3 share neither rank, file, nor diagonal.
  Square a1 = squareOf(7, 0), b3 = squareOf(5, 1);
  TEST_ASSERT_EQUAL_UINT64(0, rayBetween(a1, b3));
}

void register_chess_bitboard_tests() {
  // Square mapping
  RUN_TEST(test_squareOf_corners);
  RUN_TEST(test_squareOf_e4);
  RUN_TEST(test_rowOf_colOf_roundtrip);
  RUN_TEST(test_squareOf_identity_roundtrip);

  // Bit manipulation
  RUN_TEST(test_popcount_basic);
  RUN_TEST(test_lsb_basic);
  RUN_TEST(test_popLsb_drains);

  // File/rank masks
  RUN_TEST(test_file_rank_masks);

  // Directional shifts
  RUN_TEST(test_shift_north_south);
  RUN_TEST(test_shift_east_no_wrap);
  RUN_TEST(test_shift_west_no_wrap);

  // BitboardSet
  RUN_TEST(test_bitboardset_setPiece_removePiece);
  RUN_TEST(test_bitboardset_movePiece);
  RUN_TEST(test_bitboardset_multiple_pieces);
  RUN_TEST(test_bitboardset_clear);

  // Knight attacks
  RUN_TEST(test_knight_attacks_e4);
  RUN_TEST(test_knight_attacks_a1_corner);

  // King attacks
  RUN_TEST(test_king_attacks_e4);
  RUN_TEST(test_king_attacks_a1_corner);

  // Pawn attacks
  RUN_TEST(test_pawn_attacks_white_e4);
  RUN_TEST(test_pawn_attacks_black_e5);
  RUN_TEST(test_pawn_attacks_a_file_no_wrap);

  // Rook attacks (slider)
  RUN_TEST(test_rook_attacks_e4_empty);
  RUN_TEST(test_rook_attacks_e4_with_blocker);
  RUN_TEST(test_rook_attacks_a1_empty);

  // Bishop attacks (slider)
  RUN_TEST(test_bishop_attacks_e4_empty);
  RUN_TEST(test_bishop_attacks_with_blocker);
  RUN_TEST(test_bishop_attacks_a1_empty);

  // Queen attacks
  RUN_TEST(test_queen_attacks_e4_empty);

  // X-ray attacks
  RUN_TEST(test_xray_rook_north_through_blocker);
  RUN_TEST(test_xray_rook_no_friendly_blocker);
  RUN_TEST(test_xray_bishop_diagonal_through_blocker);

  // rayBetween
  RUN_TEST(test_ray_between_same_file);
  RUN_TEST(test_ray_between_same_rank);
  RUN_TEST(test_ray_between_same_diagonal);
  RUN_TEST(test_ray_between_anti_diagonal);
  RUN_TEST(test_ray_between_adjacent_returns_zero);
  RUN_TEST(test_ray_between_not_colinear_returns_zero);
}
