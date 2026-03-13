#include "chess_utils.h"

#include <string>

#include "chess_iterator.h"

namespace ChessUtils {

void applyBoardTransform(Piece board[8][8], int fromRow, int fromCol,
                         int toRow, int toCol,
                         const EnPassantInfo& ep, const CastlingInfo& castle,
                         Piece& capturedPiece) {
  Piece piece = board[fromRow][fromCol];
  capturedPiece = board[toRow][toCol];

  // En passant capture — remove the captured pawn
  if (ep.isCapture) {
    capturedPiece = board[ep.capturedPawnRow][toCol];
    board[ep.capturedPawnRow][toCol] = Piece::NONE;
  }

  // Move the piece
  board[toRow][toCol] = piece;
  board[fromRow][fromCol] = Piece::NONE;

  // Castling — move the rook
  if (castle.isCastling) {
    Piece rook = ChessPiece::makePiece(ChessPiece::pieceColor(piece), PieceType::ROOK);
    board[toRow][castle.rookToCol] = rook;
    board[toRow][castle.rookFromCol] = Piece::NONE;
  }
}

float evaluatePosition(const Piece board[8][8]) {
  float evaluation = 0.0f;

  ChessIterator::forEachPiece(board, [&](int, int, Piece piece) {
    float value = ChessPiece::pieceValue(piece);
    if (value == 0.0f) return;  // King or unknown

    if (ChessPiece::isWhite(piece))
      evaluation += value;
    else
      evaluation -= value;
  });

  return evaluation;
}

std::string boardToText(const Piece board[8][8]) {
  std::string result = "====== BOARD ======\n";
  ChessIterator::forEachSquare(board, [&](int row, int col, Piece piece) {
    if (col == 0) {
      result += rankChar(row);
      result += ' ';
    }
    char c = ChessPiece::pieceToChar(piece);
    result += (c == ' ') ? '.' : c;
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
