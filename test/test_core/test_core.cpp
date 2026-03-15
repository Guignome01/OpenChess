#include <unity.h>

#include <chess_utils.h>
#include <chess_rules.h>

#include "../test_helpers.h"

// Shared globals — accessible via extern from other translation units
BitboardSet bb;
Piece mailbox[64];
bool needsDefaultKings = false;

void setUp(void) {
  clearBoard(bb, mailbox);
  if (needsDefaultKings) {
    placePiece(bb, mailbox, Piece::W_KING, "h1");
    placePiece(bb, mailbox, Piece::B_KING, "h8");
  }
}

void tearDown(void) {}

// Registration functions defined in other translation units
void register_chess_rules_moves_tests();
void register_chess_rules_check_tests();
void register_chess_rules_special_tests();
void register_chess_utils_tests();
void register_chess_fen_tests();
void register_chess_board_tests();
void register_chess_history_tests();
void register_chess_history_recording_tests();
void register_chess_game_tests();
void register_chess_notation_tests();
void register_chess_iterator_tests();
void register_chess_hash_tests();
void register_chess_bitboard_tests();
void register_chess_movegen_tests();
void register_chess_pieces_tests();

int main(int argc, char** argv) {
  UNITY_BEGIN();
  register_chess_bitboard_tests();
  register_chess_rules_moves_tests();
  register_chess_rules_check_tests();
  register_chess_rules_special_tests();
  register_chess_utils_tests();
  register_chess_fen_tests();
  register_chess_notation_tests();
  register_chess_iterator_tests();
  register_chess_hash_tests();
  register_chess_pieces_tests();
  register_chess_movegen_tests();
  register_chess_board_tests();
  register_chess_history_tests();
  register_chess_history_recording_tests();
  register_chess_game_tests();
  return UNITY_END();
}
