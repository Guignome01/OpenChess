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
  static void addPawnMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& flags, int& moveCount, int moves[][2]);
  static void addRookMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]);
  static void addKnightMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]);
  static void addBishopMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]);
  static void addQueenMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]);
  static void addSlidingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const int directions[][2], int dirCount, int& moveCount, int moves[][2]);
  static void addKingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling);

  static void addCastlingMoves(const Piece board[8][8], int row, int col, Color pieceColor, uint8_t castlingRights, int& moveCount, int moves[][2]);

  static bool isSquareOccupiedByOpponent(const Piece board[8][8], int row, int col, Color pieceColor);
  static bool isSquareEmpty(const Piece board[8][8], int row, int col);

  // Check detection helpers
  static void getPseudoLegalMoves(const Piece board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling = true);
  static bool leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags);

 public:
  // En passant legality query (used by ChessBoard for Zobrist hashing)
  static bool hasLegalEnPassantCapture(const Piece board[8][8], Color sideToMove, const PositionState& flags);

  // Square attack detection
  static bool isSquareUnderAttack(const Piece board[8][8], int row, int col, Color defendingColor);

  // Main move generation function (returns only legal moves)
  static void getPossibleMoves(const Piece board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2]);

  // Move validation
  static bool isValidMove(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags);

  // Game state checks
  static bool isCheck(const Piece board[8][8], Color kingColor);
  static bool isPromotion(Piece piece, int targetRow);
  static bool hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& flags);
  static bool isCheckmate(const Piece board[8][8], Color kingColor, const PositionState& flags);
  static bool isStalemate(const Piece board[8][8], Color colorToMove, const PositionState& flags);
  static bool isInsufficientMaterial(const Piece board[8][8]);
  static bool isThreefoldRepetition(const uint64_t* positionHistory, int positionHistoryCount);
  static bool isFiftyMoveRule(const PositionState& state);
  static bool isDraw(const Piece board[8][8], Color colorToMove, const PositionState& state,
                     const uint64_t* positionHistory, int positionHistoryCount);
  static GameResult isGameOver(const Piece board[8][8], Color colorToMove, const PositionState& state,
                               const uint64_t* positionHistory, int positionHistoryCount, char& winner);
};

#endif  // CORE_CHESS_RULES_H
