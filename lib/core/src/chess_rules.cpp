#include "chess_rules.h"

#include <cstring>

#include "chess_iterator.h"
#include "chess_utils.h"

using namespace ChessBitboard;
using BB = BitboardSet;

// ---------------------------------------------------------------------------
// File-local helpers for pin-aware legal move generation
// ---------------------------------------------------------------------------
// These are internal implementation details, not part of ChessRules' public
// API. They support the pin+check mask approach: compute once per position,
// then filter each pseudo-legal target in O(1) via bitwise AND.

namespace {

// Up to 8 absolute pins possible (4 orthogonal + 4 diagonal rays from king).
struct PinData {
  Bitboard pinned;      // bitset of all pinned friendly piece squares
  Bitboard pinRay[8];   // legal-move mask per pinned piece (king→pinner ray, inclusive)
  Square pinnedSq[8];   // the pinned piece square corresponding to pinRay[i]
  int count;            // number of recorded pins (≤ 8)
};

// Returns the pin-ray mask for a piece on `sq`.
// If pinned, only targets on the ray are legal. If not pinned, returns ~0ULL
// (all targets valid from a pin perspective — checkMask still applies).
static Bitboard pinRayFor(const PinData& pinData, Square sq) {
  if (!(pinData.pinned & squareBB(sq))) return ~0ULL;
  for (int i = 0; i < pinData.count; i++)
    if (pinData.pinnedSq[i] == sq) return pinData.pinRay[i];
  return ~0ULL;  // defensive fallback (should not be reached)
}

// Returns a bitboard of ALL squares from which `attackingColor` pieces attack `sq`.
// Same logic as isSquareUnderAttack() but accumulates instead of early-returning.
// Used for check detection: popcount gives checker count, lsb gives checker square.
static Bitboard attackersOfSquare(const BB& bb, Square sq, Color attackingColor) {
  Color defending = ~attackingColor;
  Bitboard attackers = 0;

  // Leaper attacks: precomputed tables, O(1) per piece type.
  int pawnIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::PAWN));
  attackers |= ChessAttacks::PAWN_ATTACKS[ChessPiece::raw(defending)][sq] & bb.byPiece[pawnIdx];

  int knightIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::KNIGHT));
  attackers |= ChessAttacks::KNIGHT_ATTACKS[sq] & bb.byPiece[knightIdx];

  int kingIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::KING));
  attackers |= ChessAttacks::KING_ATTACKS[sq] & bb.byPiece[kingIdx];

  // Slider attacks: ray loops to find rook/queen and bishop/queen attackers.
  int rookIdx   = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::ROOK));
  int queenIdx  = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::QUEEN));
  int bishopIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(attackingColor, PieceType::BISHOP));
  Bitboard rookQueens   = bb.byPiece[rookIdx]   | bb.byPiece[queenIdx];
  Bitboard bishopQueens = bb.byPiece[bishopIdx] | bb.byPiece[queenIdx];
  attackers |= ChessAttacks::rookAttacks(sq, bb.occupied)   & rookQueens;
  attackers |= ChessAttacks::bishopAttacks(sq, bb.occupied) & bishopQueens;

  return attackers;
}

// Detects all absolute pins against `kingSq` for `sideToMove`.
//
// Algorithm:
//   1. Fire x-ray rook/bishop attacks FROM the king square, looking through
//      friendly pieces. Any enemy rook/queen (orthogonal) or bishop/queen
//      (diagonal) hit is a potential pinner.
//   2. For each potential pinner, compute the ray between king and pinner.
//   3. If exactly one friendly piece sits on that ray, it is pinned — its
//      legal moves are restricted to squares on the ray (including capturing
//      the pinner).
static PinData computePinData(const BB& bb, Square kingSq, Color sideToMove) {
  PinData data = {};
  Bitboard friendly = bb.byColor[ChessPiece::raw(sideToMove)];
  Color enemy = ~sideToMove;

  // Gather enemy slider bitboards.
  int rookIdx   = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(enemy, PieceType::ROOK));
  int queenIdx  = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(enemy, PieceType::QUEEN));
  int bishopIdx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(enemy, PieceType::BISHOP));
  Bitboard enemyRookQueens   = bb.byPiece[rookIdx]   | bb.byPiece[queenIdx];
  Bitboard enemyBishopQueens = bb.byPiece[bishopIdx] | bb.byPiece[queenIdx];

  // X-ray from king through friendly blockers to find potential pinners.
  Bitboard pinners =
      (ChessAttacks::xrayRookAttacks(bb.occupied, friendly, kingSq)   & enemyRookQueens)
    | (ChessAttacks::xrayBishopAttacks(bb.occupied, friendly, kingSq) & enemyBishopQueens);

  // For each potential pinner, validate that exactly one friendly piece
  // sits between king and pinner (= the pinned piece).
  while (pinners) {
    Square pinner = popLsb(pinners);
    Bitboard ray = ChessAttacks::rayBetween(kingSq, pinner) | squareBB(pinner);
    Bitboard pinnedBit = ray & friendly;
    if (popcount(pinnedBit) != 1) continue;  // 0 = no blocker, 2+ = shielded
    data.pinRay[data.count] = ray;
    data.pinnedSq[data.count] = lsb(pinnedBit);
    data.pinned |= pinnedBit;
    data.count++;
  }

  return data;
}

}  // anonymous namespace

// ---------------------------
// ChessRules Implementation (stateless, bitboard-based)
// ---------------------------

// Checks whether `sideToMove` has a legal EP capture available.
// Used by ChessHash to decide whether to include the EP file in the Zobrist key
// (Zobrist spec: only hash EP if the capture is actually legal).
// Uses copy-make (leavesInCheck) because horizontal discovered checks through
// two pawns on the same rank make pin-based detection insufficient.
bool ChessRules::hasLegalEnPassantCapture(const BB& bb, const Piece mailbox[],
                                          Color sideToMove, const PositionState& state) {
  if (state.epRow < 0 || state.epCol < 0) return false;

  Square epSq = squareOf(state.epRow, state.epCol);
  int capturerRow = state.epRow - ChessPiece::pawnDirection(sideToMove);
  Piece capturerPawn = ChessPiece::makePiece(sideToMove, PieceType::PAWN);

  // Find king square once for both candidate captures.
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(sideToMove, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return false;
  Square kingSq = lsb(kingBB);

  // Check left neighbor (col - 1) and right neighbor (col + 1) of EP target.
  if (state.epCol > 0) {
    Square from = squareOf(capturerRow, state.epCol - 1);
    if (mailbox[from] == capturerPawn && !leavesInCheck(bb, mailbox, from, epSq, state, kingSq))
      return true;
  }
  if (state.epCol < 7) {
    Square from = squareOf(capturerRow, state.epCol + 1);
    if (mailbox[from] == capturerPawn && !leavesInCheck(bb, mailbox, from, epSq, state, kingSq))
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

// ---------------------------------------------------------------------------
// Legal move generation — pin+check mask filtering
// ---------------------------------------------------------------------------
// Strategy (computed once per call, not per target):
//   1. Compute check state: how many enemy pieces attack the king?
//   2. Compute pin state: which friendly pieces are absolutely pinned?
//   3. Non-king, non-EP moves: filter via O(1) bitwise AND against combined
//      pin+check mask. No leavesInCheck call needed.
//   4. King moves: each destination changes the king square, so full copy-make
//      legality check is required (leavesInCheck).
//   5. EP captures: horizontal discovered check (two pawns removed on one rank)
//      is undetectable by pin analysis, so leavesInCheck is always used.

void ChessRules::getPossibleMoves(const BB& bb, const Piece mailbox[],
                                   int row, int col, const PositionState& state,
                                   MoveList& moves) {
  moves.clear();
  Square sq = squareOf(row, col);
  Piece piece = mailbox[sq];
  if (piece == Piece::NONE) return;

  bool isKing = ChessPiece::pieceType(piece) == PieceType::KING;
  Color color = ChessPiece::pieceColor(piece);

  // Locate the king square (needed for check and pin detection).
  Square kingSq;
  if (isKing) {
    kingSq = sq;
  } else {
    int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(color, PieceType::KING));
    Bitboard kingBB = bb.byPiece[kidx];
    if (!kingBB) return;
    kingSq = lsb(kingBB);
  }

  // --- Check detection ---
  Bitboard checkers = attackersOfSquare(bb, kingSq, ~color);
  int checkerCount = popcount(checkers);

  // Double check: only the king can move (no single piece can block two checks).
  if (checkerCount >= 2 && !isKing) return;

  // Check mask: non-king targets must capture the checker or interpose on the
  // ray between king and checker. No check → all targets valid (~0ULL).
  // For leaper checks (knight/pawn), rayBetween returns 0, so only capture works.
  Bitboard checkMask = ~0ULL;
  if (checkerCount == 1) {
    Square checker = lsb(checkers);
    checkMask = ChessAttacks::rayBetween(kingSq, checker) | squareBB(checker);
  }

  // --- Generate pseudo-legal moves ---
  MoveList pseudoMoves;
  getPseudoLegalMoves(bb, mailbox, sq, state, pseudoMoves, true);

  // --- King moves: always use copy-make legality check ---
  // King safety depends on the full board state after the king moves to each
  // destination, which cannot be pre-computed with pin/check masks.
  if (isKing) {
    for (int i = 0; i < pseudoMoves.count; i++) {
      Square target = pseudoMoves.square(i);
      if (!leavesInCheck(bb, mailbox, sq, target, state, target))
        moves.addSquare(target);
    }
    return;
  }

  // --- Non-king moves: pin+check mask filtering ---
  PinData pinData = computePinData(bb, kingSq, color);

  // Combined legal mask for this piece (constant across all its targets):
  // - If pinned: only targets along the pin ray AND addressing the check.
  // - If not pinned: only targets addressing the check (or all, if no check).
  Bitboard legalMask = pinRayFor(pinData, sq) & checkMask;

  // Precompute EP target once (avoid repeated squareOf inside the loop).
  bool isPawn = ChessPiece::pieceType(piece) == PieceType::PAWN;
  bool hasEP = isPawn && state.epRow >= 0 && state.epCol >= 0;
  Square epTarget = hasEP ? squareOf(state.epRow, state.epCol) : Square(-1);

  for (int i = 0; i < pseudoMoves.count; i++) {
    Square target = pseudoMoves.square(i);

    // EP captures bypass mask filtering — removing two pawns on the same rank
    // can create a horizontal discovered check that pin analysis cannot detect.
    if (hasEP && target == epTarget) {
      if (!leavesInCheck(bb, mailbox, sq, target, state, kingSq))
        moves.addSquare(target);
      continue;
    }

    // Standard mask check: target must satisfy both pin and check constraints.
    if (squareBB(target) & legalMask)
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
// Two overloads:
//   1. (row, col) public — finds king square internally, delegates to Square overload.
//      Used by ChessNotation (SAN disambiguation) and tests.
//   2. (Square, kingSq) public — used by ChessBoard::makeMove() which already
//      has the king square cached (kingSquare_[raw(currentTurn_)]).
// Both use copy-make (leavesInCheck) since they validate a single move —
// pin-aware filtering amortizes only when checking ALL moves for a piece.

bool ChessRules::isValidMove(const BB& bb, const Piece mailbox[],
                              int fromRow, int fromCol, int toRow, int toCol,
                              const PositionState& state) {
  Square from = squareOf(fromRow, fromCol);
  Square to = squareOf(toRow, toCol);
  Piece piece = mailbox[from];
  if (piece == Piece::NONE) return false;

  // Find king square once (avoid rescanning per pseudo-legal target).
  Color color = ChessPiece::pieceColor(piece);
  bool isKingMove = ChessPiece::pieceType(piece) == PieceType::KING;
  Square kingSq;
  if (isKingMove) {
    kingSq = from;
  } else {
    int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(color, PieceType::KING));
    Bitboard kingBB = bb.byPiece[kidx];
    if (!kingBB) return false;
    kingSq = lsb(kingBB);
  }
  return isValidMove(bb, mailbox, from, to, state, kingSq);
}

bool ChessRules::isValidMove(const BB& bb, const Piece mailbox[],
                              Square from, Square to,
                              const PositionState& state, Square kingSq) {
  // Generate pseudo-legal moves and check if `to` is among them.
  MoveList pseudoMoves;
  getPseudoLegalMoves(bb, mailbox, from, state, pseudoMoves, true);

  // For king moves, check legality at the destination square (the king's new position).
  bool isKingMove = ChessPiece::pieceType(mailbox[from]) == PieceType::KING;
  Square checkSq = isKingMove ? to : kingSq;

  for (int i = 0; i < pseudoMoves.count; i++)
    if (pseudoMoves.square(i) == to)
      return !leavesInCheck(bb, mailbox, from, to, state, checkSq);

  return false;
}

// ---------------------------------------------------------------------------
// Check Detection — bitboard-based attack lookup
// ---------------------------------------------------------------------------
// Note: attackersOfSquare() in the anonymous namespace has identical logic but
// accumulates ALL attackers into a Bitboard (for pin/check-mask detection).
// This version early-returns for speed — it is the hot-path function called
// inside every leavesInCheck invocation.

bool ChessRules::isSquareUnderAttack(const BB& bb, Square sq, Color defendingColor) {
  Color atk = ~defendingColor;

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
// leavesInCheck — copy-make legality check (bitboard only, no mailbox copy)
// ---------------------------------------------------------------------------
// Copies the BitboardSet (~120 bytes), applies the move on the copy, and tests
// whether the king is still in check. Used for:
//   - King moves (each destination changes kingSq, so pin masks don't apply)
//   - EP captures (horizontal discovered check undetectable by pin analysis)
//   - isValidMove (external validation, not on the hot generation path)

void ChessRules::applyMoveBB(BB& bb, Square from, Square to,
                              Piece piece, Piece capturedPiece,
                              const ChessUtils::EnPassantInfo& ep,
                              const ChessUtils::CastlingInfo& castle) {
  // EP capture: remove the captured pawn from its actual square (not the target).
  if (ep.isCapture) {
    Square epSq = squareOf(ep.capturedPawnRow, colOf(to));
    Piece epPawn = ChessPiece::makePiece(~ChessPiece::pieceColor(piece), PieceType::PAWN);
    bb.removePiece(epSq, epPawn);
  } else if (capturedPiece != Piece::NONE) {
    bb.removePiece(to, capturedPiece);
  }

  // Move the piece from source to destination.
  bb.movePiece(from, to, piece);

  // Castling: move the rook alongside the king.
  if (castle.isCastling) {
    Piece rook = ChessPiece::makePiece(ChessPiece::pieceColor(piece), PieceType::ROOK);
    Square rookFrom = squareOf(rowOf(from), castle.rookFromCol);
    Square rookTo = squareOf(rowOf(from), castle.rookToCol);
    bb.movePiece(rookFrom, rookTo, rook);
  }
}

bool ChessRules::leavesInCheck(const BB& bb, const Piece mailbox[],
                                Square from, Square to,
                                const PositionState& state, Square kingSq) {
  Piece movingPiece = mailbox[from];
  Color movingColor = ChessPiece::pieceColor(movingPiece);
  Piece targetPiece = mailbox[to];

  // Detect special move types (EP capture, castling) from coordinates.
  auto ep = ChessUtils::checkEnPassant(rowOf(from), colOf(from), rowOf(to), colOf(to),
                                       movingPiece, targetPiece);
  auto castle = ChessUtils::checkCastling(rowOf(from), colOf(from), rowOf(to), colOf(to),
                                          movingPiece);

  // Copy bitboard only (~120 bytes) — no mailbox copy needed for attack detection.
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

bool ChessRules::hasAnyLegalMove(const BB& bb, const Piece mailbox[],
                                  Color color, const PositionState& state) {
  int kidx = ChessPiece::pieceZobristIndex(ChessPiece::makePiece(color, PieceType::KING));
  Bitboard kingBB = bb.byPiece[kidx];
  if (!kingBB) return false;
  return hasAnyLegalMove(bb, mailbox, color, state, lsb(kingBB));
}

// Determines whether `color` has at least one legal move from the given position.
// Uses the same pin+check mask approach as getPossibleMoves, but iterates over
// ALL friendly pieces and early-exits on the first legal move found.
bool ChessRules::hasAnyLegalMove(const BB& bb, const Piece mailbox[],
                                  Color color, const PositionState& state, Square kingSq) {
  // --- Check detection (same logic as getPossibleMoves) ---
  Bitboard checkers = attackersOfSquare(bb, kingSq, ~color);
  int checkerCount = popcount(checkers);
  bool isDoubleCheck = (checkerCount >= 2);

  Bitboard checkMask = ~0ULL;
  if (checkerCount == 1) {
    Square checker = lsb(checkers);
    checkMask = ChessAttacks::rayBetween(kingSq, checker) | squareBB(checker);
  }

  PinData pinData = computePinData(bb, kingSq, color);

  return ChessIterator::somePiece(bb, mailbox, [&](int r, int c, Piece piece) {
    if (ChessPiece::pieceColor(piece) != color) return false;
    Square sq = squareOf(r, c);
    bool isKing = ChessPiece::pieceType(piece) == PieceType::KING;

    // Double check: only king can move.
    if (isDoubleCheck && !isKing) return false;

    MoveList pseudoMoves;
    getPseudoLegalMoves(bb, mailbox, sq, state, pseudoMoves, true);

    // King moves: test each destination with copy-make.
    if (isKing) {
      for (int i = 0; i < pseudoMoves.count; i++)
        if (!leavesInCheck(bb, mailbox, sq, pseudoMoves.square(i), state, pseudoMoves.square(i)))
          return true;
      return false;
    }

    // Non-king: compute combined legal mask once for this piece.
    Bitboard legalMask = pinRayFor(pinData, sq) & checkMask;

    // Precompute EP target for pawns.
    bool isPawn = ChessPiece::pieceType(piece) == PieceType::PAWN;
    bool hasEP = isPawn && state.epRow >= 0 && state.epCol >= 0;
    Square epTarget = hasEP ? squareOf(state.epRow, state.epCol) : Square(-1);

    for (int i = 0; i < pseudoMoves.count; i++) {
      Square target = pseudoMoves.square(i);

      // EP captures: always use leavesInCheck (horizontal discovered check).
      if (hasEP && target == epTarget) {
        if (!leavesInCheck(bb, mailbox, sq, target, state, kingSq)) return true;
        continue;
      }

      // Standard mask check.
      if (squareBB(target) & legalMask) return true;
    }
    return false;
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
