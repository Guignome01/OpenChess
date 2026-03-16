#include <unity.h>

#include <fen.h>
#include <uci.h>

#include "../test_helpers.h"

using namespace LibreChess;
using namespace LibreChess::uci;

// ===========================================================================
// Helpers
// ===========================================================================

// Run a single command through the UCI handler and return output lines.
static std::vector<std::string> runCommand(UCIHandler& handler,
                                           const std::string& cmd) {
  StringUCIStream stream;
  handler.processCommand(cmd, stream);
  return stream.output();
}

// Check if any output line contains the given substring.
static bool outputContains(const std::vector<std::string>& lines,
                           const std::string& substr) {
  for (const auto& line : lines)
    if (line.find(substr) != std::string::npos) return true;
  return false;
}

// ===========================================================================
// UCI identification
// ===========================================================================

static void test_uci_id(void) {
  UCIHandler handler(64);
  auto out = runCommand(handler, "uci");
  TEST_ASSERT_TRUE(outputContains(out, "id name LibreChess"));
  TEST_ASSERT_TRUE(outputContains(out, "uciok"));
}

// ===========================================================================
// isready -> readyok
// ===========================================================================

static void test_uci_isready(void) {
  UCIHandler handler(64);
  auto out = runCommand(handler, "isready");
  TEST_ASSERT_TRUE(outputContains(out, "readyok"));
}

// ===========================================================================
// position startpos
// ===========================================================================

static void test_uci_position_startpos(void) {
  UCIHandler handler(64);
  runCommand(handler, "position startpos");
  // Verify the internal position matches the starting FEN
  TEST_ASSERT_TRUE(handler.position().sideToMove() == Color::WHITE);
  // Starting position hash should be non-zero
  TEST_ASSERT_TRUE(handler.position().hash() != 0);
}

// ===========================================================================
// position startpos moves e2e4 e7e5
// ===========================================================================

static void test_uci_position_startpos_moves(void) {
  UCIHandler handler(64);
  runCommand(handler, "position startpos moves e2e4 e7e5");
  // After 1.e4 e5, it's White's turn again
  TEST_ASSERT_TRUE(handler.position().sideToMove() == Color::WHITE);
  // Pawn should be on e4 (not e2) and e5 (not e7)
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(4, 4)) == Piece::W_PAWN);
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(3, 4)) == Piece::B_PAWN);
}

// ===========================================================================
// position fen <custom>
// ===========================================================================

static void test_uci_position_fen(void) {
  UCIHandler handler(64);
  runCommand(handler, "position fen 8/8/8/8/8/8/8/4K2R w K - 0 1");
  TEST_ASSERT_TRUE(handler.position().sideToMove() == Color::WHITE);
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(7, 4)) == Piece::W_KING);
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(7, 7)) == Piece::W_ROOK);
}

// ===========================================================================
// position fen <fen> moves <moves>
// ===========================================================================

static void test_uci_position_fen_moves(void) {
  UCIHandler handler(64);
  // Position with just kings and a rook. Apply O-O (e1g1).
  runCommand(handler, "position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1 moves e1g1");
  // After O-O, king should be on g1 and rook on f1
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(7, 6)) == Piece::W_KING);
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(7, 5)) == Piece::W_ROOK);
}

// ===========================================================================
// go depth 1 -> bestmove <legal move>
// ===========================================================================

static void test_uci_go_depth(void) {
  UCIHandler handler(64);
  runCommand(handler, "position startpos");
  auto out = runCommand(handler, "go depth 1");
  // Should contain "bestmove " followed by a move
  TEST_ASSERT_TRUE(outputContains(out, "bestmove "));
  // bestmove should NOT be "0000"
  TEST_ASSERT_FALSE(outputContains(out, "bestmove 0000"));
}

// ===========================================================================
// go depth N emits info lines
// ===========================================================================

static void test_uci_go_info_output(void) {
  UCIHandler handler(64);
  runCommand(handler, "position startpos");
  auto out = runCommand(handler, "go depth 2");
  // Should have at least one "info depth" line and one "bestmove" line
  TEST_ASSERT_TRUE(outputContains(out, "info depth"));
  TEST_ASSERT_TRUE(outputContains(out, "bestmove "));
}

// ===========================================================================
// ucinewgame resets state
// ===========================================================================

static void test_uci_newgame(void) {
  UCIHandler handler(64);
  // Apply some moves
  runCommand(handler, "position startpos moves e2e4 e7e5 g1f3");
  // Reset
  runCommand(handler, "ucinewgame");
  // Position should be back to starting
  TEST_ASSERT_TRUE(handler.position().sideToMove() == Color::WHITE);
  // Pawn should be on e2 again (starting position)
  TEST_ASSERT_TRUE(handler.position().pieceOn(squareOf(6, 4)) == Piece::W_PAWN);
}

// ===========================================================================
// quit returns false
// ===========================================================================

static void test_uci_quit(void) {
  UCIHandler handler(64);
  StringUCIStream stream;
  bool cont = handler.processCommand("quit", stream);
  TEST_ASSERT_FALSE(cont);
}

// ===========================================================================
// Unknown command is silently ignored
// ===========================================================================

static void test_uci_unknown_command(void) {
  UCIHandler handler(64);
  auto out = runCommand(handler, "foobar");
  // Should produce no output
  TEST_ASSERT_EQUAL_INT(0, (int)out.size());
}

// ===========================================================================
// loop() processes multiple commands
// ===========================================================================

static void test_uci_loop(void) {
  UCIHandler handler(64);
  StringUCIStream stream;
  stream.addInput("uci");
  stream.addInput("isready");
  stream.addInput("quit");
  handler.loop(stream);
  auto& out = stream.output();
  TEST_ASSERT_TRUE(outputContains(out, "uciok"));
  TEST_ASSERT_TRUE(outputContains(out, "readyok"));
}

// ===========================================================================
// Registration
// ===========================================================================

void register_uci_tests() {
  RUN_TEST(test_uci_id);
  RUN_TEST(test_uci_isready);
  RUN_TEST(test_uci_position_startpos);
  RUN_TEST(test_uci_position_startpos_moves);
  RUN_TEST(test_uci_position_fen);
  RUN_TEST(test_uci_position_fen_moves);
  RUN_TEST(test_uci_go_depth);
  RUN_TEST(test_uci_go_info_output);
  RUN_TEST(test_uci_newgame);
  RUN_TEST(test_uci_quit);
  RUN_TEST(test_uci_unknown_command);
  RUN_TEST(test_uci_loop);
}
