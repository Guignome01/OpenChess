#include "chess_utils.h"

#include <string>

namespace ChessUtils {

void applyBoardTransform(ChessBitboard::BitboardSet& bb, Piece mailbox[],
                         ChessBitboard::Square from, ChessBitboard::Square to,
                         const EnPassantInfo& ep, const CastlingInfo& castle,
                         Piece& capturedPiece) {
  using namespace ChessBitboard;
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
    Piece rook = ChessPiece::makePiece(ChessPiece::pieceColor(piece), PieceType::ROOK);
    Square rookFrom = squareOf(row, castle.rookFromCol);
    Square rookTo = squareOf(row, castle.rookToCol);
    bb.movePiece(rookFrom, rookTo, rook);
    mailbox[rookFrom] = Piece::NONE;
    mailbox[rookTo] = rook;
  }
}

float evaluatePosition(const ChessBitboard::BitboardSet& bb) {
  // Piece values indexed by pieceZobristIndex offset (P=0 N=1 B=2 R=3 Q=4 K=5)
  static constexpr float VALUES[] = {1.0f, 3.0f, 3.0f, 5.0f, 9.0f, 0.0f};
  float eval = 0.0f;
  for (int i = 0; i < 6; ++i) {
    eval += VALUES[i] * ChessBitboard::popcount(bb.byPiece[i]);      // white
    eval -= VALUES[i] * ChessBitboard::popcount(bb.byPiece[i + 6]);  // black
  }
  return eval;
}

std::string boardToText(const Piece mailbox[]) {
  std::string result = "====== BOARD ======\n";
  for (int row = 0; row < 8; ++row) {
    result += rankChar(row);
    result += ' ';
    for (int col = 0; col < 8; ++col) {
      char c = ChessPiece::pieceToChar(mailbox[ChessBitboard::squareOf(row, col)]);
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

}  // namespace ChessUtils
