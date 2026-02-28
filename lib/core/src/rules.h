#ifndef RULES_H
#define RULES_H

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
  static void addPawnMoves(const char board[8][8], int row, int col, char pieceColor, const PositionState& flags, int& moveCount, int moves[][2]);
  static void addRookMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]);
  static void addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]);
  static void addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]);
  static void addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]);
  static void addKingMoves(const char board[8][8], int row, int col, char pieceColor, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling);

  static bool hasCastlingRight(uint8_t castlingRights, char pieceColor, bool kingSide);
  static void addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, uint8_t castlingRights, int& moveCount, int moves[][2]);

  static bool isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor);
  static bool isSquareEmpty(const char board[8][8], int row, int col);
  static bool isValidSquare(int row, int col);

  // Check detection helpers
  static void getPseudoLegalMoves(const char board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling = true);
  static bool wouldMoveLeaveKingInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags);
  static void applyMove(char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags, char& capturedPiece);

 public:
  // En passant legality query (used by ChessBoard for Zobrist hashing)
  static bool hasLegalEnPassantCapture(const char board[8][8], char sideToMove, const PositionState& flags);

  // Square attack detection
  static bool isSquareUnderAttack(const char board[8][8], int row, int col, char defendingColor);

  // Main move generation function (returns only legal moves)
  static void getPossibleMoves(const char board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2]);

  // Move validation
  static bool isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags);

  // Game state checks
  static bool findKingPosition(const char board[8][8], char kingColor, int& kingRow, int& kingCol);
  static bool isKingInCheck(const char board[8][8], char kingColor);
  static bool isPawnPromotion(char piece, int targetRow);
  static bool hasAnyLegalMove(const char board[8][8], char color, const PositionState& flags);
  static bool isCheckmate(const char board[8][8], char kingColor, const PositionState& flags);
  static bool isStalemate(const char board[8][8], char colorToMove, const PositionState& flags);
};

#endif // RULES_H
