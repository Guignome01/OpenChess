#include "zobrist.h"

#include "iterator.h"
#include "movegen.h"

namespace LibreChess {
namespace zobrist {

uint64_t computeHash(const BitboardSet& bb, const Piece mailbox[],
                     Color turn, const PositionState& state) {
  uint64_t hash = 0;

  // Iterate occupied squares via bitboard serialization for LERF indexing.
  Bitboard occ = bb.occupied;
  while (occ) {
    Square sq = popLsb(occ);
    int idx = piece::pieceZobristIndex(mailbox[sq]);
    hash ^= KEYS.pieces[idx][sq];
  }

  hash ^= KEYS.castling[state.castlingRights];

  if (movegen::hasLegalEnPassantCapture(bb, mailbox, turn, state))
    hash ^= KEYS.enPassant[state.epCol];

  if (turn == Color::BLACK) hash ^= KEYS.sideToMove;

  return hash;
}

}  // namespace zobrist
}  // namespace LibreChess
