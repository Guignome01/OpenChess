#ifndef CORE_CHESS_HASH_H
#define CORE_CHESS_HASH_H

// Zobrist hashing for position comparison and threefold repetition detection.
// Keys are generated at compile time via a deterministic xorshift64 PRNG.

#include <stdint.h>

#include "chess_iterator.h"
#include "chess_rules.h"
#include "types.h"

#ifndef PROGMEM
#define PROGMEM
#endif

namespace ChessHash {

// ---------------------------------------------------------------------------
// Constexpr key generation (deterministic xorshift64, seed=0x12345678ABCDEF01)
// ---------------------------------------------------------------------------

constexpr uint64_t xorshift64(uint64_t s) {
  s ^= s << 13;
  s ^= s >> 7;
  s ^= s << 17;
  return s;
}

struct Keys {
  uint64_t pieces[12][64]{};   // P=0 N=1 B=2 R=3 Q=4 K=5 p=6 n=7 b=8 r=9 q=10 k=11
  uint64_t castling[16]{};     // indexed by castling rights bitmask
  uint64_t enPassant[8]{};     // indexed by file (0–7)
  uint64_t sideToMove{};

  constexpr Keys() {
    uint64_t s = 0x12345678ABCDEF01ULL;
    for (int p = 0; p < 12; ++p)
      for (int sq = 0; sq < 64; ++sq) {
        s = xorshift64(s);
        pieces[p][sq] = s;
      }
    for (int i = 0; i < 16; ++i) {
      s = xorshift64(s);
      castling[i] = s;
    }
    for (int i = 0; i < 8; ++i) {
      s = xorshift64(s);
      enPassant[i] = s;
    }
    s = xorshift64(s);
    sideToMove = s;
  }
};

static constexpr Keys KEYS PROGMEM = Keys();

// ---------------------------------------------------------------------------
// Hash computation
// ---------------------------------------------------------------------------

inline uint64_t computeHash(const Piece board[8][8], Color turn,
                            const PositionState& state) {
  uint64_t hash = 0;

  ChessIterator::forEachPiece(board, [&](int row, int col, Piece piece) {
    int idx = ChessPiece::pieceZobristIndex(piece);
    hash ^= KEYS.pieces[idx][row * 8 + col];
  });

  hash ^= KEYS.castling[state.castlingRights];

  if (ChessRules::hasLegalEnPassantCapture(board, turn, state)) {
    hash ^= KEYS.enPassant[state.epCol];
  }

  if (turn == Color::BLACK) hash ^= KEYS.sideToMove;

  return hash;
}

}  // namespace ChessHash

#endif  // CORE_CHESS_HASH_H
