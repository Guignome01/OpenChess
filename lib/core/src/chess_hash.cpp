#include "chess_hash.h"

#include "chess_iterator.h"
#include "chess_rules.h"

namespace ChessHash {

uint64_t computeHash(const ChessBitboard::BitboardSet& bb, const Piece mailbox[],
                     Color turn, const PositionState& state) {
  uint64_t hash = 0;

  // Iterate occupied squares via bitboard serialization for LERF indexing.
  ChessBitboard::Bitboard occ = bb.occupied;
  while (occ) {
    ChessBitboard::Square sq = ChessBitboard::popLsb(occ);
    int idx = ChessPiece::pieceZobristIndex(mailbox[sq]);
    hash ^= KEYS.pieces[idx][sq];
  }

  hash ^= KEYS.castling[state.castlingRights];

  if (ChessRules::hasLegalEnPassantCapture(bb, mailbox, turn, state))
    hash ^= KEYS.enPassant[state.epCol];

  if (turn == Color::BLACK) hash ^= KEYS.sideToMove;

  return hash;
}

}  // namespace ChessHash
