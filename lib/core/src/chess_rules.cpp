#include "chess_rules.h"

#include <cstring>

#include "chess_iterator.h"
#include "chess_utils.h"

using namespace ChessBitboard;
using BB = BitboardSet;

// ---------------------------
// ChessRules Implementation (stateless, bitboard-based)
// ---------------------------

bool ChessRules::hasLegalEnPassantCapture(const BB& bb, const Piece mailbox[],
                                          Color sideToMove, const PositionState& state) {
  if (state.epRow < 0 || state.epCol < 0) return false;
  Square epSq = squareOf(state.epRow, state.epCol);
  int capturerRow = state.epRow - ChessPiece::pawnDirection(sideToMove);
  Piece capturerPawn = ChessPiece::makePiece(sideToMove, PieceType::PAWN);
  if (state.epCol > 0) {
    Square from = squareOf(capturerRow, state.epCol - 1);
    if (mailbox[from] == capturerPawn && !leavesInCheck(bb, mailbox, from, epSq, state))
      return true;
  }
  if (state.epCol < 7) {
    Square from = squareOf(capturerRow, state.epCol + 1);
    if (mailbox[from] == capturerPawn && !leavesInCheck(bb, mailbox, from, epSq, state))
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Pseudo-legal move generation
// ---------------------------------------------------------------------------

void ChessRules::getPseudoLegalMoves(const BB& bb, const Piece mailbox[],
                                      Square sq, const PositionState& state,
                                      MoveList& moves, bool includeCastling) {
  moves.clear();
  Piece piece = mailbox[sq];
  if (piece == Piece::NONE) return;

  Color pieceColor = ChessPiece::pieceColor(piece);

  switch (ChessPiece::pieceType(piece)) {
    case PieceType::PAWN:   addPawnMoves(bb, mailbox, sq, pieceColor, state, moves); break;
    case PieceType::ROOK:   addRookMoves(bb, sq, pieceColor, moves); break;
    case PieceType::KNIGHT: addKnightMoves(bb, sq, pieceColor, moves); break;
    case PieceType::BISHOP: addBishopMoves(bb, sq, pieceColor, moves); break;
    case PieceType::QUEEN:  addQueenMoves(bb, sq, pieceColor, moves); break;
    case PieceType::KING:   addKingMoves(bb, mailbox, sq, pieceColor, state, moves, includeCastling); break;
    default: break;
  }
}

// Legal move generation — filters pseudo-legal moves through leavesInCheck.
void ChessRules::getPossibleMoves(const BB& bb, const Piece mailbox[],
                                   int row, int col, const PositionState& state,
                                   MoveList& moves) {
  Square sq = squareOf(row, col);
  MoveList pseudoMoves;
  getPseudoLegalMoves(bb, mailbox, sq, state, pseudoMoves, true);

  Piece piece = mailbox[sq];
  bool isKing = ChessPiece::pieceType(piece) == PieceType::KING;

  // Find king square for leavesInCheck
  Square kingSq;
  if (isKing) {
    kingSq = sq;
  } else {
    Color color = ChessPiece::pieceColor(piece);
    int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(color, PieceType::KING));
    Bitboard kingBB = bb.byPiece[kidx];
    if (!kingBB) { moves.clear(); return; }
    kingSq = lsb(kingBB);
  }

  moves.clear();
  for (int i = 0; i < pseudoMoves.count; i++) {
    Square target = pseudoMoves.square(i);
    Square checkSq = isKing ? target : kingSq;
    if (!leavesInCheck(bb, mailbox, sq, target, state, checkSq))
      moves.addSquare(target);
  }
}

// ---------------------------------------------------------------------------
// Pawn moves (bitboard-based)
// ---------------------------------------------------------------------------

void ChessRules::addPawnMoves(const BB& bb, const Piece mailbox[],
                               Square sq, Color pieceColor,
                               const PositionState& state, MoveList& moves) {
  int row = rowOf(sq);
  int col = colOf(sq);
  int direction = ChessPiece::pawnDirection(pieceColor);
  Bitboard friendly = bb.byColor[ChessPiece::raw(pieceColor)];

  // One square forward
  int fwdRow = row + direction;
  if ((unsigned)fwdRow < 8) {
    Square fwdSq = squareOf(fwdRow, col);
    if (!(bb.occupied & squareBB(fwdSq))) {
      moves.addSquare(fwdSq);

      // Initial two-square move
      int startRow = ChessPiece::homeRow(pieceColor) + direction;
      if (row == startRow) {
        int dblRow = row + 2 * direction;
        Square dblSq = squareOf(dblRow, col);
        if (!(bb.occupied & squareBB(dblSq)))
          moves.addSquare(dblSq);
      }
    }
  }

  // Diagonal captures
  int captureRow = row + direction;
  if ((unsigned)captureRow < 8) {
    for (int dc = -1; dc <= 1; dc += 2) {
      int captureCol = col + dc;
      if ((unsigned)captureCol < 8) {
        Square capSq = squareOf(captureRow, captureCol);
        Bitboard capBit = squareBB(capSq);
        // Capture if enemy piece present
        if ((bb.occupied & capBit) && !(friendly & capBit))
          moves.addSquare(capSq);
      }
    }
  }

  // En passant captures
  if (state.epRow >= 0 && state.epCol >= 0 && row == state.epRow - direction) {
    for (int dc = -1; dc <= 1; dc += 2) {
      if (col + dc == state.epCol)
        moves.addSquare(squareOf(state.epRow, state.epCol));
    }
  }
}

// ---------------------------------------------------------------------------
// Slider moves via attack tables + bitboard masking
// ---------------------------------------------------------------------------

void ChessRules::addRookMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves) {
  Bitboard attacks = ChessAttacks::rookAttacks(sq, bb.occupied);
  attacks &= ~bb.byColor[ChessPiece::raw(pieceColor)];  // exclude friendly pieces
  while (attacks)
    moves.addSquare(popLsb(attacks));
}

void ChessRules::addBishopMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves) {
  Bitboard attacks = ChessAttacks::bishopAttacks(sq, bb.occupied);
  attacks &= ~bb.byColor[ChessPiece::raw(pieceColor)];
  while (attacks)
    moves.addSquare(popLsb(attacks));
}

void ChessRules::addQueenMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves) {
  Bitboard attacks = ChessAttacks::queenAttacks(sq, bb.occupied);
  attacks &= ~bb.byColor[ChessPiece::raw(pieceColor)];
  while (attacks)
    moves.addSquare(popLsb(attacks));
}

void ChessRules::addKnightMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves) {
  Bitboard attacks = ChessAttacks::KNIGHT_ATTACKS[sq];
  attacks &= ~bb.byColor[ChessPiece::raw(pieceColor)];
  while (attacks)
    moves.addSquare(popLsb(attacks));
}

void ChessRules::addKingMoves(const BB& bb, const Piece mailbox[],
                               Square sq, Color pieceColor,
                               const PositionState& state, MoveList& moves,
                               bool includeCastling) {
  Bitboard attacks = ChessAttacks::KING_ATTACKS[sq];
  attacks &= ~bb.byColor[ChessPiece::raw(pieceColor)];
  while (attacks)
    moves.addSquare(popLsb(attacks));

  if (includeCastling)
    addCastlingMoves(bb, mailbox, sq, pieceColor, state.castlingRights, moves);
}

void ChessRules::addCastlingMoves(const BB& bb, const Piece mailbox[],
                                   Square sq, Color pieceColor,
                                   uint8_t castlingRights, MoveList& moves) {
  int homeRow = ChessPiece::homeRow(pieceColor);
  Piece kingPiece = ChessPiece::makePiece(pieceColor, PieceType::KING);
  Piece rookPiece = ChessPiece::makePiece(pieceColor, PieceType::ROOK);

  int row = rowOf(sq);
  int col = colOf(sq);
  if (row != homeRow || col != 4) return;
  if (mailbox[sq] != kingPiece) return;

  // King cannot castle while in check.
  if (isSquareUnderAttack(bb, sq, pieceColor)) return;

  // King-side castling (e -> g)
  if (ChessUtils::hasCastlingRight(castlingRights, pieceColor, true)) {
    Square f = squareOf(homeRow, 5);
    Square g = squareOf(homeRow, 6);
    Square h = squareOf(homeRow, 7);
    if (mailbox[f] == Piece::NONE && mailbox[g] == Piece::NONE && mailbox[h] == rookPiece)
      if (!isSquareUnderAttack(bb, f, pieceColor) && !isSquareUnderAttack(bb, g, pieceColor))
        moves.addSquare(g);
  }

  // Queen-side castling (e -> c)
  if (ChessUtils::hasCastlingRight(castlingRights, pieceColor, false)) {
    Square d = squareOf(homeRow, 3);
    Square c = squareOf(homeRow, 2);
    Square b = squareOf(homeRow, 1);
    Square a = squareOf(homeRow, 0);
    if (mailbox[d] == Piece::NONE && mailbox[c] == Piece::NONE &&
        mailbox[b] == Piece::NONE && mailbox[a] == rookPiece)
      if (!isSquareUnderAttack(bb, d, pieceColor) && !isSquareUnderAttack(bb, c, pieceColor))
        moves.addSquare(c);
  }
}

// ---------------------------------------------------------------------------
// Move validation
// ---------------------------------------------------------------------------

bool ChessRules::isValidMove(const BB& bb, const Piece mailbox[],
                              int fromRow, int fromCol, int toRow, int toCol,
                              const PositionState& state) {
  Square from = squareOf(fromRow, fromCol);
  Square to = squareOf(toRow, toCol);
  MoveList pseudoMoves;
  getPseudoLegalMoves(bb, mailbox, from, state, pseudoMoves, true);

  for (int i = 0; i < pseudoMoves.count; i++)
    if (pseudoMoves.square(i) == to)
      return !leavesInCheck(bb, mailbox, from, to, state);

  return false;
}

// ---------------------------------------------------------------------------
// Check Detection — bitboard-based attack lookup
// ---------------------------------------------------------------------------

bool ChessRules::isSquareUnderAttack(const BB& bb, Square sq, Color defendingColor) {
  Color atk = ~defendingColor;
  int ci = ChessPiece::raw(atk);

  // Pawn attacks: PAWN_ATTACKS[defending][sq] gives squares from which
  // an attacking pawn could reach sq.
  int pawnIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::PAWN));
  if (ChessAttacks::PAWN_ATTACKS[ChessPiece::raw(defendingColor)][sq] & bb.byPiece[pawnIdx])
    return true;

  // Knight attacks
  int knightIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::KNIGHT));
  if (ChessAttacks::KNIGHT_ATTACKS[sq] & bb.byPiece[knightIdx])
    return true;

  // King attacks
  int kingIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::KING));
  if (ChessAttacks::KING_ATTACKS[sq] & bb.byPiece[kingIdx])
    return true;

  // Rook/Queen — orthogonal rays
  int rookIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::ROOK));
  int queenIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::QUEEN));
  Bitboard rookQueens = bb.byPiece[rookIdx] | bb.byPiece[queenIdx];
  if (ChessAttacks::rookAttacks(sq, bb.occupied) & rookQueens)
    return true;

  // Bishop/Queen — diagonal rays
  int bishopIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(atk, PieceType::BISHOP));
  Bitboard bishopQueens = bb.byPiece[bishopIdx] | bb.byPiece[queenIdx];
  if (ChessAttacks::bishopAttacks(sq, bb.occupied) & bishopQueens)
    return true;

  return false;
}

// ---------------------------------------------------------------------------
// leavesInCheck — copy-make on bitboard only (no mailbox copy)
// ---------------------------------------------------------------------------

void ChessRules::applyMoveBB(BB& bb, Square from, Square to,
                              Piece piece, Piece capturedPiece,
                              const ChessUtils::EnPassantInfo& ep,
                              const ChessUtils::CastlingInfo& castle) {
  // EP capture: remove captured pawn
  if (ep.isCapture) {
    Square epSq = squareOf(ep.capturedPawnRow, colOf(to));
    Piece epPawn = ChessPiece::makePiece(~ChessPiece::pieceColor(piece), PieceType::PAWN);
    bb.removePiece(epSq, epPawn);
  } else if (capturedPiece != Piece::NONE) {
    bb.removePiece(to, capturedPiece);
  }

  // Move the piece
  bb.movePiece(from, to, piece);

  // Castling: move the rook
  if (castle.isCastling) {
    Piece rook = ChessPiece::makePiece(ChessPiece::pieceColor(piece), PieceType::ROOK);
    Square rookFrom = squareOf(rowOf(from), castle.rookFromCol);
    Square rookTo = squareOf(rowOf(from), castle.rookToCol);
    bb.movePiece(rookFrom, rookTo, rook);
  }
}

bool ChessRules::leavesInCheck(const BB& bb, const Piece mailbox[],
                                Square from, Square to,
                                const PositionState& state) {
  Piece movingPiece = mailbox[from];
  bool isKingMove = ChessPiece::pieceType(movingPiece) == PieceType::KING;

  if (isKingMove)
    return leavesInCheck(bb, mailbox, from, to, state, to);

  // Non-king: find the king
  Color movingColor = ChessPiece::pieceColor(movingPiece);
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(movingColor, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return true;
  return leavesInCheck(bb, mailbox, from, to, state, lsb(kingBB));
}

bool ChessRules::leavesInCheck(const BB& bb, const Piece mailbox[],
                                Square from, Square to,
                                const PositionState& state, Square kingSq) {
  Piece movingPiece = mailbox[from];
  Color movingColor = ChessPiece::pieceColor(movingPiece);
  Piece targetPiece = mailbox[to];

  auto ep = ChessUtils::checkEnPassant(rowOf(from), colOf(from), rowOf(to), colOf(to),
                                       movingPiece, targetPiece);
  auto castle = ChessUtils::checkCastling(rowOf(from), colOf(from), rowOf(to), colOf(to),
                                          movingPiece);

  // Copy bitboard only (120 bytes) — no mailbox copy needed for attack detection.
  BB testBB = bb;
  applyMoveBB(testBB, from, to, movingPiece, targetPiece, ep, castle);

  return isSquareUnderAttack(testBB, kingSq, movingColor);
}

// ---------------------------------------------------------------------------
// Check / Checkmate / Stalemate
// ---------------------------------------------------------------------------

bool ChessRules::isCheck(const BB& bb, Color kingColor) {
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(kingColor, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return false;
  return isSquareUnderAttack(bb, lsb(kingBB), kingColor);
}

bool ChessRules::isCheck(const BB& bb, Square kingSq, Color kingColor) {
  return isSquareUnderAttack(bb, kingSq, kingColor);
}

bool ChessRules::hasLegalMove(const BB& bb, const Piece mailbox[],
                               Square sq, const PositionState& state, Square kingSq) {
  MoveList pseudoMoves;
  getPseudoLegalMoves(bb, mailbox, sq, state, pseudoMoves, true);

  bool isKing = ChessPiece::pieceType(mailbox[sq]) == PieceType::KING;
  for (int i = 0; i < pseudoMoves.count; i++) {
    Square target = pseudoMoves.square(i);
    Square checkSq = isKing ? target : kingSq;
    if (!leavesInCheck(bb, mailbox, sq, target, state, checkSq))
      return true;
  }
  return false;
}

bool ChessRules::hasAnyLegalMove(const BB& bb, const Piece mailbox[],
                                  Color color, const PositionState& state) {
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(color, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return false;
  return hasAnyLegalMove(bb, mailbox, color, state, lsb(kingBB));
}

bool ChessRules::hasAnyLegalMove(const BB& bb, const Piece mailbox[],
                                  Color color, const PositionState& state, Square kingSq) {
  return ChessIterator::somePiece(bb, mailbox, [&](int r, int c, Piece piece) {
    if (ChessPiece::pieceColor(piece) != color) return false;
    return hasLegalMove(bb, mailbox, squareOf(r, c), state, kingSq);
  });
}

bool ChessRules::isCheckmate(const BB& bb, const Piece mailbox[],
                              Color kingColor, const PositionState& state) {
  return isCheck(bb, kingColor) && !hasAnyLegalMove(bb, mailbox, kingColor, state);
}

bool ChessRules::isStalemate(const BB& bb, const Piece mailbox[],
                              Color colorToMove, const PositionState& state) {
  return !isCheck(bb, colorToMove) && !hasAnyLegalMove(bb, mailbox, colorToMove, state);
}

// ---------------------------------------------------------------------------
// Insufficient Material (bitboard popcount approach)
// ---------------------------------------------------------------------------

bool ChessRules::isInsufficientMaterial(const BB& bb) {
  // Any pawns, rooks, or queens → sufficient material
  int pIdx = ChessPiece::pieceZobristIndex(Piece::W_PAWN);
  if (bb.byPiece[pIdx] | bb.byPiece[pIdx + 6]) return false;

  int rIdx = ChessPiece::pieceZobristIndex(Piece::W_ROOK);
  if (bb.byPiece[rIdx] | bb.byPiece[rIdx + 6]) return false;

  int qIdx = ChessPiece::pieceZobristIndex(Piece::W_QUEEN);
  if (bb.byPiece[qIdx] | bb.byPiece[qIdx + 6]) return false;

  int wKnightIdx = ChessPiece::pieceZobristIndex(Piece::W_KNIGHT);
  int wBishopIdx = ChessPiece::pieceZobristIndex(Piece::W_BISHOP);
  int bKnightIdx = ChessPiece::pieceZobristIndex(Piece::B_KNIGHT);
  int bBishopIdx = ChessPiece::pieceZobristIndex(Piece::B_BISHOP);

  int whiteMinors = popcount(bb.byPiece[wKnightIdx]) + popcount(bb.byPiece[wBishopIdx]);
  int blackMinors = popcount(bb.byPiece[bKnightIdx]) + popcount(bb.byPiece[bBishopIdx]);

  // Two or more minors on one side → sufficient
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
      // Square color parity: (rank + file) & 1 where rank = sq/8, file = sq%8
      constexpr Bitboard COLOR_0 = 0x55AA55AA55AA55AAULL;  // a1, c1, b2, d2, ...
      bool wOnColor0 = (wb & COLOR_0) != 0;
      bool bOnColor0 = (bBish & COLOR_0) != 0;
      return wOnColor0 == bOnColor0;
    }
  }

  return false;
}

// ---------------------------------------------------------------------------
// Draw / Game-over detection (unchanged logic, new signatures)
// ---------------------------------------------------------------------------

bool ChessRules::isThreefoldRepetition(const HashHistory& hashes) {
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

bool ChessRules::isFiftyMoveRule(const PositionState& state) {
  return state.halfmoveClock >= 100;
}

bool ChessRules::isDraw(const BB& bb, const Piece mailbox[], Color colorToMove,
                         const PositionState& state, const HashHistory& hashes) {
  return isStalemate(bb, mailbox, colorToMove, state)
      || isFiftyMoveRule(state)
      || isInsufficientMaterial(bb)
      || isThreefoldRepetition(hashes);
}

GameResult ChessRules::isGameOver(const BB& bb, const Piece mailbox[], Color colorToMove,
                                   const PositionState& state, const HashHistory& hashes,
                                   char& winner) {
  // Find king once — reuse for both check detection and legal move search
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(colorToMove, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) {
    winner = ' ';
    return GameResult::IN_PROGRESS;
  }
  Square kingSq = lsb(kingBB);

  bool inCheck = isSquareUnderAttack(bb, kingSq, colorToMove);
  bool hasLegal = hasAnyLegalMove(bb, mailbox, colorToMove, state, kingSq);

  if (!hasLegal) {
    if (inCheck) {
      winner = ChessPiece::colorToChar(~colorToMove);
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
