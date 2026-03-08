#include "chess_utils.h"

#include <cctype>
#include <string>

#include "chess_iterator.h"

namespace ChessUtils {

float evaluatePosition(const char board[8][8]) {
  // Simple material evaluation
  // Positive = White advantage, negative = Black advantage
  float evaluation = 0.0f;

  ChessIterator::forEachPiece(board, [&](int, int, char piece) {
    float value = 0.0f;

    switch (tolower(piece)) {
      case 'p': value = 1.0f; break;
      case 'n': value = 3.0f; break;
      case 'b': value = 3.0f; break;
      case 'r': value = 5.0f; break;
      case 'q': value = 9.0f; break;
      default: return;  // King or unexpected
    }

    if (piece >= 'A' && piece <= 'Z')
      evaluation += value;
    else
      evaluation -= value;
  });

  return evaluation;
}

}  // namespace ChessUtils
