#include "utils.h"

#include <string>

namespace LibreChess {
namespace utils {

void applyBoardTransform(BitboardSet& bb, Piece mailbox[],
                         Square from, Square to,
                         const EnPassantInfo& ep, const CastlingInfo& castle,
                         Piece& capturedPiece) {
  Piece piece = mailbox[from];
  capturedPiece = mailbox[to];

  // En passant capture — remove the captured pawn
  if (ep.isCapture) {
    Square epSq = squareOf(ep.capturedPawnRow, colOf(to));
    capturedPiece = mailbox[epSq];
    bb.removePiece(epSq, capturedPiece);
    mailbox[epSq] = Piece::NONE;
  }

  // Remove captured piece at destination (non-EP)
  if (!ep.isCapture && capturedPiece != Piece::NONE)
    bb.removePiece(to, capturedPiece);

  // Move the piece
  bb.movePiece(from, to, piece);
  mailbox[from] = Piece::NONE;
  mailbox[to] = piece;

  // Castling — move the rook
  if (castle.isCastling) {
    int row = rowOf(to);
    Piece rook = piece::makePiece(piece::pieceColor(piece), PieceType::ROOK);
    Square rookFrom = squareOf(row, castle.rookFromCol);
    Square rookTo = squareOf(row, castle.rookToCol);
    bb.movePiece(rookFrom, rookTo, rook);
    mailbox[rookFrom] = Piece::NONE;
    mailbox[rookTo] = rook;
  }
}

std::string boardToText(const Piece mailbox[]) {
  std::string result = "====== BOARD ======\n";
  for (int row = 0; row < 8; ++row) {
    result += rankChar(row);
    result += ' ';
    for (int col = 0; col < 8; ++col) {
      char c = piece::pieceToChar(mailbox[squareOf(row, col)]);
      result += (c == ' ') ? '.' : c;
      result += ' ';
    }
    result += ' ';
    result += rankChar(row);
    result += '\n';
  }
  result += "  a b c d e f g h\n";
  result += "===================";
  return result;
}

}  // namespace utils
}  // namespace LibreChess
