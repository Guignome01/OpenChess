#ifndef CORE_CHESS_RULES_H
#define CORE_CHESS_RULES_H

// ---------------------------------------------------------------------------
// Backward-compatibility forwarder — delegates to movegen:: and rules::.
// Removed in Phase 6.
// ---------------------------------------------------------------------------

#include "movegen.h"
#include "rules.h"
#include "utils.h"

class ChessRules {
  using BB = ChessBitboard::BitboardSet;
  using Bitboard = ChessBitboard::Bitboard;
  using Square = ChessBitboard::Square;

 public:
  // --- Forwarded to LibreChess::movegen ---

  static bool hasLegalEnPassantCapture(const BB& bb, const Piece mailbox[],
                                       Color sideToMove, const PositionState& state) {
    return LibreChess::movegen::hasLegalEnPassantCapture(bb, mailbox, sideToMove, state);
  }

  static void getPossibleMoves(const BB& bb, const Piece mailbox[],
                                int row, int col, const PositionState& state,
                                MoveList& moves) {
    LibreChess::movegen::getPossibleMoves(bb, mailbox, row, col, state, moves);
  }

  static bool isValidMove(const BB& bb, const Piece mailbox[],
                           int fromRow, int fromCol, int toRow, int toCol,
                           const PositionState& state) {
    return LibreChess::movegen::isValidMove(bb, mailbox, fromRow, fromCol, toRow, toCol, state);
  }

  static bool isValidMove(const BB& bb, const Piece mailbox[],
                           Square from, Square to,
                           const PositionState& state, Square kingSq) {
    return LibreChess::movegen::isValidMove(bb, mailbox, from, to, state, kingSq);
  }

  static void generateAllMoves(const BB& bb, const Piece mailbox[],
                                Color color, const PositionState& state,
                                MoveList& moves) {
    LibreChess::movegen::generateAllMoves(bb, mailbox, color, state, moves);
  }

  static void generateCaptures(const BB& bb, const Piece mailbox[],
                                Color color, const PositionState& state,
                                MoveList& moves) {
    LibreChess::movegen::generateCaptures(bb, mailbox, color, state, moves);
  }

  static bool hasAnyLegalMove(const BB& bb, const Piece mailbox[],
                               Color color, const PositionState& state) {
    return LibreChess::movegen::hasAnyLegalMove(bb, mailbox, color, state);
  }

  // --- Forwarded to LibreChess::attacks ---

  static bool isSquareUnderAttack(const BB& bb, Square sq, Color defendingColor) {
    return LibreChess::attacks::isSquareUnderAttack(bb, sq, defendingColor);
  }

  static bool isSquareUnderAttack(const BB& bb, int row, int col, Color defendingColor) {
    return LibreChess::attacks::isSquareUnderAttack(bb, row, col, defendingColor);
  }

  // --- Forwarded to LibreChess::rules ---

  static bool isCheck(const BB& bb, Color kingColor) {
    return LibreChess::rules::isCheck(bb, kingColor);
  }

  static bool isCheck(const BB& bb, Square kingSq, Color kingColor) {
    return LibreChess::rules::isCheck(bb, kingSq, kingColor);
  }

  static bool isCheckmate(const BB& bb, const Piece mailbox[],
                           Color kingColor, const PositionState& state) {
    return LibreChess::rules::isCheckmate(bb, mailbox, kingColor, state);
  }

  static bool isStalemate(const BB& bb, const Piece mailbox[],
                           Color colorToMove, const PositionState& state) {
    return LibreChess::rules::isStalemate(bb, mailbox, colorToMove, state);
  }

  static bool isInsufficientMaterial(const BB& bb) {
    return LibreChess::rules::isInsufficientMaterial(bb);
  }

  static bool isThreefoldRepetition(const HashHistory& hashes) {
    return LibreChess::rules::isThreefoldRepetition(hashes);
  }

  static bool isFiftyMoveRule(const PositionState& state) {
    return LibreChess::rules::isFiftyMoveRule(state);
  }

  static bool isDraw(const BB& bb, const Piece mailbox[], Color colorToMove,
                     const PositionState& state, const HashHistory& hashes) {
    return LibreChess::rules::isDraw(bb, mailbox, colorToMove, state, hashes);
  }

  static GameResult isGameOver(const BB& bb, const Piece mailbox[], Color colorToMove,
                                const PositionState& state, const HashHistory& hashes,
                                char& winner) {
    return LibreChess::rules::isGameOver(bb, mailbox, colorToMove, state, hashes, winner);
  }
};

#endif  // CORE_CHESS_RULES_H
