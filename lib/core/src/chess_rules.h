#ifndef CORE_CHESS_RULES_H
#define CORE_CHESS_RULES_H

#include "types.h"

// ---------------------------
// Chess Rules Class (stateless)
// ---------------------------
// Pure chess logic — evaluates positions, generates moves, detects check/mate.
// All position-dependent state (castling, en passant, clocks) is owned by the
// caller (ChessBoard) and passed in via PositionState where needed.
class ChessRules {
 private:
  // Helper functions for move generation
  static void addPawnMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& state, MoveList& moves);
  static void addRookMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves);
  static void addKnightMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves);
  static void addBishopMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves);
  static void addQueenMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves);
  static void addSlidingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const int directions[][2], int dirCount, MoveList& moves);
  static void addKingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& state, MoveList& moves, bool includeCastling);

  static void addCastlingMoves(const Piece board[8][8], int row, int col, Color pieceColor, uint8_t castlingRights, MoveList& moves);

  static bool isSquareOccupiedByOpponent(const Piece board[8][8], int row, int col, Color pieceColor);
  static bool isSquareEmpty(const Piece board[8][8], int row, int col);

  // Check detection helpers
  static void getPseudoLegalMoves(const Piece board[8][8], int row, int col, const PositionState& state, MoveList& moves, bool includeCastling = true);
  static bool leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state);
  static bool leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state, int kingRow, int kingCol);

  // Early-exit legal move check for a single piece
  static bool hasLegalMove(const Piece board[8][8], int row, int col, const PositionState& state, int kingRow, int kingCol);

  // hasAnyLegalMove with pre-found king position
  static bool hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& state, int kingRow, int kingCol);

 public:
  // En passant legality query (used by ChessBoard for Zobrist hashing)
  static bool hasLegalEnPassantCapture(const Piece board[8][8], Color sideToMove, const PositionState& state);

  // Square attack detection
  static bool isSquareUnderAttack(const Piece board[8][8], int row, int col, Color defendingColor);

  // Main move generation function (returns only legal moves)
  static void getPossibleMoves(const Piece board[8][8], int row, int col, const PositionState& state, MoveList& moves);

  // Move validation
  static bool isValidMove(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state);

  // Game state checks
  static bool isCheck(const Piece board[8][8], Color kingColor);
  static bool isCheck(const Piece board[8][8], Color kingColor, int kingRow, int kingCol);
  static bool hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& state);
  static bool isCheckmate(const Piece board[8][8], Color kingColor, const PositionState& state);
  static bool isStalemate(const Piece board[8][8], Color colorToMove, const PositionState& state);
  static bool isInsufficientMaterial(const Piece board[8][8]);
  static bool isThreefoldRepetition(const HashHistory& hashes);
  static bool isFiftyMoveRule(const PositionState& state);
  static bool isDraw(const Piece board[8][8], Color colorToMove, const PositionState& state,
                     const HashHistory& hashes);
  static GameResult isGameOver(const Piece board[8][8], Color colorToMove, const PositionState& state,
                               const HashHistory& hashes, char& winner);
};

#endif  // CORE_CHESS_RULES_H
