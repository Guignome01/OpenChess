#ifndef CORE_CHESS_RULES_H
#define CORE_CHESS_RULES_H

#include "chess_movegen.h"
#include "chess_bitboard.h"
#include "chess_utils.h"
#include "types.h"

// ---------------------------
// Chess Rules Class (stateless)
// ---------------------------
// Pure chess logic — evaluates positions, generates moves, detects check/mate.
// All position-dependent state (castling, en passant, clocks) is owned by the
// caller (ChessBoard) and passed in via PositionState where needed.
//
// Board representation: BitboardSet (piece bitboards + occupancy) plus a
// parallel Piece mailbox[64] for O(1) piece identity on any square.
class ChessRules {
 private:
  using BB = ChessBitboard::BitboardSet;
  using Bitboard = ChessBitboard::Bitboard;
  using Square = ChessBitboard::Square;

  // Pseudo-legal move generation per piece type
  static void addPawnMoves(const BB& bb, const Piece mailbox[], Square sq, Color pieceColor, const PositionState& state, MoveList& moves);
  static void addKnightMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves);
  static void addBishopMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves);
  static void addRookMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves);
  static void addQueenMoves(const BB& bb, Square sq, Color pieceColor, MoveList& moves);
  static void addKingMoves(const BB& bb, const Piece mailbox[], Square sq, Color pieceColor, const PositionState& state, MoveList& moves, bool includeCastling);
  static void addCastlingMoves(const BB& bb, const Piece mailbox[], Square sq, Color pieceColor, uint8_t castlingRights, MoveList& moves);

  static void getPseudoLegalMoves(const BB& bb, const Piece mailbox[], Square sq, const PositionState& state, MoveList& moves, bool includeCastling = true);

  // Legality check: copies BitboardSet (~120 bytes), applies move on the copy,
  // tests if the king is in check. No mailbox copy needed — attack detection
  // uses bitboards only. Used for king moves, EP captures, and isValidMove.
  static bool leavesInCheck(const BB& bb, const Piece mailbox[], Square from, Square to, const PositionState& state, Square kingSq);

  // BB-only forward move application for leavesInCheck (avoids mailbox copy).
  static void applyMoveBB(BB& bb, Square from, Square to,
                          Piece piece, Piece capturedPiece,
                          const ChessUtils::EnPassantInfo& ep,
                          const ChessUtils::CastlingInfo& castle);

  // hasAnyLegalMove with pre-found king position (uses pin+check-mask filtering)
  static bool hasAnyLegalMove(const BB& bb, const Piece mailbox[], Color color, const PositionState& state, Square kingSq);

  // Shared implementation for generateAllMoves / generateCaptures.
  static void generateMovesImpl(const BB& bb, const Piece mailbox[], Color color,
                                const PositionState& state, MoveList& out,
                                bool capturesOnly);

 public:
  // En passant legality query (used by ChessHash for Zobrist hashing)
  static bool hasLegalEnPassantCapture(const BB& bb, const Piece mailbox[], Color sideToMove, const PositionState& state);

  // Square attack detection using precomputed tables + ray loops.
  // Only needs the bitboard set — no mailbox required.
  static bool isSquareUnderAttack(const BB& bb, Square sq, Color defendingColor);

  // Row/col overload for callers using board coordinates.
  static bool isSquareUnderAttack(const BB& bb, int row, int col, Color defendingColor) {
    return isSquareUnderAttack(bb, ChessBitboard::squareOf(row, col), defendingColor);
  }

  // Main move generation function (returns only legal moves)
  static void getPossibleMoves(const BB& bb, const Piece mailbox[], int row, int col, const PositionState& state, MoveList& moves);

  // Move validation
  static bool isValidMove(const BB& bb, const Piece mailbox[], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state);
  static bool isValidMove(const BB& bb, const Piece mailbox[], Square from, Square to, const PositionState& state, Square kingSq);

  // Bulk legal move generation — populates a MoveList for the entire position.
  // Used by engine search. Computes pin+check masks once, then iterates all
  // friendly pieces via bitboard serialization for amortized O(1) filtering.
  static void generateAllMoves(const BB& bb, const Piece mailbox[],
                                Color color, const PositionState& state,
                                MoveList& moves);

  // Capture-only move generation for quiescence search.
  // Emits captures, EP captures, and capture-promotions only.
  static void generateCaptures(const BB& bb, const Piece mailbox[],
                                Color color, const PositionState& state,
                                MoveList& moves);

  // Game state checks
  static bool isCheck(const BB& bb, Color kingColor);
  static bool isCheck(const BB& bb, Square kingSq, Color kingColor);
  static bool hasAnyLegalMove(const BB& bb, const Piece mailbox[], Color color, const PositionState& state);
  static bool isCheckmate(const BB& bb, const Piece mailbox[], Color kingColor, const PositionState& state);
  static bool isStalemate(const BB& bb, const Piece mailbox[], Color colorToMove, const PositionState& state);
  static bool isInsufficientMaterial(const BB& bb);
  static bool isThreefoldRepetition(const HashHistory& hashes);
  static bool isFiftyMoveRule(const PositionState& state);
  static bool isDraw(const BB& bb, const Piece mailbox[], Color colorToMove, const PositionState& state,
                     const HashHistory& hashes);
  static GameResult isGameOver(const BB& bb, const Piece mailbox[], Color colorToMove, const PositionState& state,
                               const HashHistory& hashes, char& winner);
};

#endif  // CORE_CHESS_RULES_H
