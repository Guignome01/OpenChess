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
      case 'k': return;  // King has no material value
      default: return;
    }

    if (isWhitePiece(piece))
      evaluation += value;
    else
      evaluation -= value;
  });

  return evaluation;
}

std::string boardToText(const char board[8][8]) {
  std::string result = "====== BOARD ======\n";
  ChessIterator::forEachSquare(board, [&](int row, int col, char piece) {
    if (col == 0) {
      result += static_cast<char>('0' + (8 - row));
      result += ' ';
    }
    result += (piece == ' ') ? '.' : piece;
    result += ' ';
    if (col == 7) {
      result += ' ';
      result += static_cast<char>('0' + (8 - row));
      result += '\n';
    }
  });
  result += "  a b c d e f g h\n";
  result += "===================";
  return result;
}

}  // namespace ChessUtils
