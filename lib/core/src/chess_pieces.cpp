#include "chess_pieces.h"

#include "chess_bitboard.h"

namespace ChessPieces {

using namespace ChessBitboard;
using namespace ChessPiece;

Bitboard PASSED_PAWN_MASK[2][64] = {};
Bitboard ISOLATED_PAWN_MASK[8] = {};
Bitboard FORWARD_FILE_MASK[2][64] = {};

static Bitboard adjacentFilesMask(int file) {
  Bitboard mask = 0;
  if (file > 0) mask |= fileBB(file - 1);
  if (file < 7) mask |= fileBB(file + 1);
  return mask;
}

void initPawnMasks() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  for (int file = 0; file < 8; ++file)
    ISOLATED_PAWN_MASK[file] = adjacentFilesMask(file);

  for (Square sq = 0; sq < 64; ++sq) {
    const int rank = sq / 8;  // 0=rank1, 7=rank8 (LERF)
    const int file = sq & 7;

    Bitboard sameAndAdjacentFiles = fileBB(file);
    if (file > 0) sameAndAdjacentFiles |= fileBB(file - 1);
    if (file < 7) sameAndAdjacentFiles |= fileBB(file + 1);

    Bitboard whitePassed = 0;
    Bitboard blackPassed = 0;
    Bitboard whiteForward = 0;
    Bitboard blackForward = 0;

    // White moves toward increasing rank index (north in LERF).
    for (int r = rank + 1; r < 8; ++r) {
      Bitboard rankMask = rankBB(r);
      whitePassed |= sameAndAdjacentFiles & rankMask;
      whiteForward |= fileBB(file) & rankMask;
    }

    // Black moves toward decreasing rank index (south in LERF).
    for (int r = rank - 1; r >= 0; --r) {
      Bitboard rankMask = rankBB(r);
      blackPassed |= sameAndAdjacentFiles & rankMask;
      blackForward |= fileBB(file) & rankMask;
    }

    PASSED_PAWN_MASK[raw(Color::WHITE)][sq] = whitePassed;
    PASSED_PAWN_MASK[raw(Color::BLACK)][sq] = blackPassed;
    FORWARD_FILE_MASK[raw(Color::WHITE)][sq] = whiteForward;
    FORWARD_FILE_MASK[raw(Color::BLACK)][sq] = blackForward;
  }
}

bool isPassed(Square sq, Color color, Bitboard enemyPawns) {
  return (enemyPawns & PASSED_PAWN_MASK[raw(color)][sq]) == 0;
}

bool isIsolated(Square sq, Bitboard friendlyPawns) {
  return (friendlyPawns & ISOLATED_PAWN_MASK[colOf(sq)]) == 0;
}

bool isDoubled(Square sq, Color color, Bitboard friendlyPawns) {
  return (friendlyPawns & FORWARD_FILE_MASK[raw(color)][sq]) != 0;
}

bool isBackward(Square sq, Color color, Bitboard friendlyPawns, Bitboard enemyPawnAttacks) {
  const int file = colOf(sq);
  Bitboard adjacentPawns = friendlyPawns & ISOLATED_PAWN_MASK[file];
  if (!adjacentPawns) return false;

  Square nextSq = SQ_NONE;
  if (color == Color::WHITE) {
    if (sq >= 56) return false;
    nextSq = sq + 8;
  } else {
    if (sq < 8) return false;
    nextSq = sq - 8;
  }

  Bitboard nextSquare = squareBB(nextSq);
  if ((enemyPawnAttacks & nextSquare) == 0) return false;

  Bitboard adjacentAttacks = (color == Color::WHITE)
      ? (shiftNE(adjacentPawns) | shiftNW(adjacentPawns))
      : (shiftSE(adjacentPawns) | shiftSW(adjacentPawns));

  return (adjacentAttacks & nextSquare) == 0;
}

}  // namespace ChessPieces
