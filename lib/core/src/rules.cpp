#include "rules.h"

#include "attacks.h"
#include "movegen.h"

namespace LibreChess {
namespace rules {

using namespace LibreChess;
namespace atk = LibreChess::attacks;

// ---------------------------------------------------------------------------
// Check detection
// ---------------------------------------------------------------------------

bool isCheck(const BitboardSet& bb, Color kingColor) {
  int kidx = piece::pieceZobristIndex(piece::makePiece(kingColor, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return false;
  return atk::isSquareUnderAttack(bb, lsb(kingBB), kingColor);
}

bool isCheck(const BitboardSet& bb, Square kingSq, Color kingColor) {
  return atk::isSquareUnderAttack(bb, kingSq, kingColor);
}

// ---------------------------------------------------------------------------
// Checkmate / Stalemate
// ---------------------------------------------------------------------------

bool isCheckmate(const BitboardSet& bb, const Piece mailbox[],
                 Color kingColor, const PositionState& state) {
  return isCheck(bb, kingColor) && !movegen::hasAnyLegalMove(bb, mailbox, kingColor, state);
}

bool isStalemate(const BitboardSet& bb, const Piece mailbox[],
                 Color colorToMove, const PositionState& state) {
  return !isCheck(bb, colorToMove) && !movegen::hasAnyLegalMove(bb, mailbox, colorToMove, state);
}

// ---------------------------------------------------------------------------
// Insufficient Material
// ---------------------------------------------------------------------------

bool isInsufficientMaterial(const BitboardSet& bb) {
  // Any pawns, rooks, or queens → sufficient material
  int pIdx = piece::pieceZobristIndex(Piece::W_PAWN);
  if (bb.byPiece[pIdx] | bb.byPiece[pIdx + 6]) return false;

  int rIdx = piece::pieceZobristIndex(Piece::W_ROOK);
  if (bb.byPiece[rIdx] | bb.byPiece[rIdx + 6]) return false;

  int qIdx = piece::pieceZobristIndex(Piece::W_QUEEN);
  if (bb.byPiece[qIdx] | bb.byPiece[qIdx + 6]) return false;

  int wKnightIdx = piece::pieceZobristIndex(Piece::W_KNIGHT);
  int wBishopIdx = piece::pieceZobristIndex(Piece::W_BISHOP);
  int bKnightIdx = piece::pieceZobristIndex(Piece::B_KNIGHT);
  int bBishopIdx = piece::pieceZobristIndex(Piece::B_BISHOP);

  int whiteMinors = popcount(bb.byPiece[wKnightIdx]) + popcount(bb.byPiece[wBishopIdx]);
  int blackMinors = popcount(bb.byPiece[bKnightIdx]) + popcount(bb.byPiece[bBishopIdx]);

  if (whiteMinors > 1 || blackMinors > 1) return false;

  // K vs K
  if (whiteMinors == 0 && blackMinors == 0) return true;

  // K+minor vs K
  if ((whiteMinors == 1 && blackMinors == 0) ||
      (whiteMinors == 0 && blackMinors == 1))
    return true;

  // K+B vs K+B with same-color bishops
  if (whiteMinors == 1 && blackMinors == 1) {
    Bitboard wb = bb.byPiece[wBishopIdx];
    Bitboard bBish = bb.byPiece[bBishopIdx];
    if (wb && bBish) {
      bool wOnDark = (wb & DARK_SQUARES) != 0;
      bool bOnDark = (bBish & DARK_SQUARES) != 0;
      return wOnDark == bOnDark;
    }
  }

  return false;
}

// ---------------------------------------------------------------------------
// Draw / Game-over detection
// ---------------------------------------------------------------------------

bool isThreefoldRepetition(const HashHistory& hashes) {
  if (hashes.count < 5) return false;

  uint64_t current = hashes.keys[hashes.count - 1];
  int count = 1;

  for (int i = hashes.count - 3; i >= 0; i -= 2) {
    if (hashes.keys[i] == current) {
      count++;
      if (count >= 3) return true;
    }
  }
  return false;
}

bool isFiftyMoveRule(const PositionState& state) {
  return state.halfmoveClock >= 100;
}

bool isDraw(const BitboardSet& bb, const Piece mailbox[], Color colorToMove,
            const PositionState& state, const HashHistory& hashes) {
  return isStalemate(bb, mailbox, colorToMove, state)
      || isFiftyMoveRule(state)
      || isInsufficientMaterial(bb)
      || isThreefoldRepetition(hashes);
}

GameResult isGameOver(const BitboardSet& bb, const Piece mailbox[], Color colorToMove,
                      const PositionState& state, const HashHistory& hashes,
                      char& winner) {
  int kidx = piece::pieceZobristIndex(piece::makePiece(colorToMove, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) {
    winner = ' ';
    return GameResult::IN_PROGRESS;
  }
  Square kingSq = lsb(kingBB);

  bool inCheck = atk::isSquareUnderAttack(bb, kingSq, colorToMove);
  bool hasLegal = movegen::hasAnyLegalMove(bb, mailbox, colorToMove, state);

  if (!hasLegal) {
    if (inCheck) {
      winner = piece::colorToChar(~colorToMove);
      return GameResult::CHECKMATE;
    } else {
      winner = 'd';
      return GameResult::STALEMATE;
    }
  }
  if (isFiftyMoveRule(state)) {
    winner = 'd';
    return GameResult::DRAW_50;
  }
  if (isInsufficientMaterial(bb)) {
    winner = 'd';
    return GameResult::DRAW_INSUFFICIENT;
  }
  if (isThreefoldRepetition(hashes)) {
    winner = 'd';
    return GameResult::DRAW_3FOLD;
  }
  winner = ' ';
  return GameResult::IN_PROGRESS;
}

}  // namespace rules
}  // namespace LibreChess
