#include <unity.h>

#include <chess_utils.h>
#include <chess_rules.h>

#include "../test_helpers.h"

// Shared globals — accessible via extern from other translation units
char board[8][8];
bool needsDefaultKings = false;

void setUp(void) {
  clearBoard(board);
  if (needsDefaultKings) {
    placePiece(board, 'K', "h1");
    placePiece(board, 'k', "h8");
  }
}

void tearDown(void) {}

// Registration functions defined in other translation units
void register_rules_moves_tests();
void register_rules_check_tests();
void register_rules_special_tests();
void register_utils_tests();
void register_codec_tests();
void register_board_tests();
void register_recorder_tests();
void register_history_tests();
void register_chess_game_tests();

int main(int argc, char** argv) {
  UNITY_BEGIN();
  register_rules_moves_tests();
  register_rules_check_tests();
  register_rules_special_tests();
  register_utils_tests();
  register_codec_tests();
  register_board_tests();
  register_recorder_tests();
  register_history_tests();
  register_chess_game_tests();
  return UNITY_END();
}
