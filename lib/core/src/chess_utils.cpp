#include "chess_utils.h"

#include <cctype>
#include <string>

namespace ChessUtils {

float evaluatePosition(const char board[8][8]) {
  // Simple material evaluation
  // Positive = White advantage, negative = Black advantage
  float evaluation = 0.0f;

  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      char piece = board[row][col];
      float value = 0.0f;

      // Get piece value (uppercase = White, lowercase = Black)
      switch (tolower(piece)) {
        case 'p':
          value = 1.0f;
          break; // Pawn
        case 'n':
          value = 3.0f;
          break; // Knight
        case 'b':
          value = 3.0f;
          break; // Bishop
        case 'r':
          value = 5.0f;
          break; // Rook
        case 'q':
          value = 9.0f;
          break; // Queen
        case 'k':
          value = 0.0f;
          break; // King (not counted)
        default:
          continue; // Empty square
      }

      // Add to evaluation (positive for White, negative for Black)
      if (piece >= 'A' && piece <= 'Z')
        evaluation += value; // White piece
      else if (piece >= 'a' && piece <= 'z')
        evaluation -= value; // Black piece
    }

  return evaluation;
}

}  // namespace ChessUtils
