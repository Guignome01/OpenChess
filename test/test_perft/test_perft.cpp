#include <unity.h>

#include <chess_board.h>
#include <chess_history.h>
#include <chess_rules.h>

// ---------------------------------------------------------------------------
// Perft — exhaustive move-tree node count for move generator correctness.
// Reference: Chess Programming Wiki "Perft Results" page.
//
// Standalone test suite: run with `pio test -e native -f test_perft`
// ---------------------------------------------------------------------------

static ChessBoard perftBoard;

void setUp(void) {}
void tearDown(void) {}

// Recursive perft: count leaf nodes at the given depth.
// depth == 0 returns 1 (the current position is a leaf).
static uint64_t perft(ChessBoard& cb, int depth) {
  if (depth == 0) return 1;

  uint64_t nodes = 0;
  const auto& board = cb.getBoard();
  Color turn = cb.currentTurn();
  PositionState prevState = cb.positionState();

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      Piece piece = board[row][col];
      if (piece == Piece::NONE) continue;
      if (ChessPiece::pieceColor(piece) != turn) continue;

      int moveCount = 0;
      int moves[28][2];
      cb.getPossibleMoves(row, col, moveCount, moves);

      for (int i = 0; i < moveCount; i++) {
        int toRow = moves[i][0];
        int toCol = moves[i][1];
        bool isPromo = ChessRules::isPromotion(piece, toRow);

        // Promotion moves must be counted once per promotion piece
        const char promos[] = {'q', 'r', 'b', 'n'};
        int promoCount = isPromo ? 4 : 1;

        for (int p = 0; p < promoCount; p++) {
          char promoPiece = isPromo ? promos[p] : ' ';
          Piece targetPiece = board[toRow][toCol];

          MoveResult result = cb.makeMove(row, col, toRow, toCol, promoPiece);
          if (!result.valid) continue;

          MoveEntry entry = buildMoveEntry(row, col, toRow, toCol,
                                           piece, targetPiece,
                                           result, prevState);

          nodes += perft(cb, depth - 1);

          cb.reverseMove(entry);
        }
      }
    }
  }
  return nodes;
}

// ---------------------------------------------------------------------------
// Test positions from Chess Programming Wiki "Perft Results"
// ---------------------------------------------------------------------------

// Position 1: Initial position
static void test_perft_initial_position(void) {
  perftBoard.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  TEST_ASSERT_EQUAL_UINT64(20, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(400, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(8902, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(197281, perft(perftBoard, 4));
  TEST_ASSERT_EQUAL_UINT64(4865609, perft(perftBoard, 5));
}

// Position 2: Kiwipete — rich in castling, en passant, and promotion edge cases
static void test_perft_kiwipete(void) {
  perftBoard.loadFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  TEST_ASSERT_EQUAL_UINT64(48, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(2039, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(97862, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(4085603, perft(perftBoard, 4));
}

// Position 3 — exposed kings, pawn captures, rook endgame
static void test_perft_position3(void) {
  perftBoard.loadFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
  TEST_ASSERT_EQUAL_UINT64(14, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(191, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(2812, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(43238, perft(perftBoard, 4));
}

// Position 4 — promotions, checks, en passant
static void test_perft_position4(void) {
  perftBoard.loadFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
  TEST_ASSERT_EQUAL_UINT64(6, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(264, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(9467, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(422333, perft(perftBoard, 4));
}

// Position 5 — promotion on d8, mixed castling rights
static void test_perft_position5(void) {
  perftBoard.loadFEN("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
  TEST_ASSERT_EQUAL_UINT64(44, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(1486, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(62379, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(2103487, perft(perftBoard, 4));
}

// Position 6 — symmetrical, double fianchetto
static void test_perft_position6(void) {
  perftBoard.loadFEN("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
  TEST_ASSERT_EQUAL_UINT64(46, perft(perftBoard, 1));
  TEST_ASSERT_EQUAL_UINT64(2079, perft(perftBoard, 2));
  TEST_ASSERT_EQUAL_UINT64(89890, perft(perftBoard, 3));
  TEST_ASSERT_EQUAL_UINT64(3894594, perft(perftBoard, 4));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_perft_initial_position);
  RUN_TEST(test_perft_kiwipete);
  RUN_TEST(test_perft_position3);
  RUN_TEST(test_perft_position4);
  RUN_TEST(test_perft_position5);
  RUN_TEST(test_perft_position6);
  return UNITY_END();
}
