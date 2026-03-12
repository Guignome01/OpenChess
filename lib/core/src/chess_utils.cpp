#include "chess_utils.h"

#include <cctype>
#include <string>

#include "chess_iterator.h"

namespace ChessUtils {

void applyBoardTransform(char board[8][8], int fromRow, int fromCol,
                         int toRow, int toCol,
                         const PositionState& state, char& capturedPiece) {
  char piece = board[fromRow][fromCol];
  capturedPiece = board[toRow][toCol];

  // En passant capture — remove the captured pawn
  auto ep = checkEnPassant(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  if (ep.isCapture) {
    capturedPiece = board[ep.capturedPawnRow][toCol];
    board[ep.capturedPawnRow][toCol] = ' ';
  }

  // Move the piece
  board[toRow][toCol] = piece;
  board[fromRow][fromCol] = ' ';

  // Castling — move the rook
  auto castle = checkCastling(fromRow, fromCol, toRow, toCol, piece);
  if (castle.isCastling) {
    char rook = makePiece('R', getPieceColor(piece));
    board[toRow][castle.rookToCol] = rook;
    board[toRow][castle.rookFromCol] = ' ';
  }
}

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
      result += rankChar(row);
      result += ' ';
    }
    result += (piece == ' ') ? '.' : piece;
    result += ' ';
    if (col == 7) {
      result += ' ';
      result += rankChar(row);
      result += '\n';
    }
  });
  result += "  a b c d e f g h\n";
  result += "===================";
  return result;
}

}  // namespace ChessUtils
