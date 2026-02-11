#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <stdint.h>

// ---------------------------
// Chess Engine Class
// ---------------------------
class ChessEngine {
 private:
  // Castling rights bitmask
  // Bit 0: White king-side (K)
  // Bit 1: White queen-side (Q)
  // Bit 2: Black king-side (k)
  // Bit 3: Black queen-side (q)
  uint8_t castlingRights;

  // En passant target square (-1 if none)
  int enPassantTargetRow;
  int enPassantTargetCol;

  // Halfmove clock for 50-move rule (counts half-moves since last pawn move or capture)
  int halfmoveClock;
  // Fullmove clock: starts at 1, incremented after Black's move
  int fullmoveClock;

  // --- Zobrist hashing for threefold repetition detection ---
  // Position history (cleared on irreversible moves for memory efficiency)
#define MAX_POSITION_HISTORY 128
  uint64_t positionHistory[MAX_POSITION_HISTORY];
  int positionHistoryCount;

  static inline int pieceToZobristIndex(char piece) {
    const char* pieces = "PNBRQKpnbrqk";
    const char* p = __builtin_strchr(pieces, piece);
    return p ? (int)(p - pieces) : -1;
  };

  // Helper functions for move generation
  void addPawnMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;
  void addRookMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;
  void addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;
  void addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;
  void addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;
  void addKingMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2], bool includeCastling) const;

  bool hasCastlingRight(char pieceColor, bool kingSide) const;
  void addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) const;

  bool isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor) const;
  bool isSquareEmpty(const char board[8][8], int row, int col) const;
  bool isValidSquare(int row, int col) const;

  // Check detection helpers
  void getPseudoLegalMoves(const char board[8][8], int row, int col, int& moveCount, int moves[][2], bool includeCastling = true) const;
  bool isSquareUnderAttack(const char board[8][8], int row, int col, char defendingColor) const;
  bool wouldMoveLeaveKingInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol) const;
  void makeMove(char board[8][8], int fromRow, int fromCol, int toRow, int toCol, char& capturedPiece) const;

 public:
  ChessEngine();

  // Reset engine state to initial conditions (new game)
  void reset() {
    clearEnPassantTarget();
    castlingRights = 0x0F;
    halfmoveClock = 0;
    fullmoveClock = 1;
    clearPositionHistory();
  }

  // Set castling rights bitmask (KQkq = 0b1111)
  void setCastlingRights(uint8_t rights);
  uint8_t getCastlingRights() const;

  // En passant target square management
  void setEnPassantTarget(int row, int col);
  void clearEnPassantTarget();
  void getEnPassantTarget(int& row, int& col) const;
  bool hasEnPassantTarget() const;

  // Halfmove clock for 50-move rule
  int getHalfmoveClock() const;
  void setHalfmoveClock(int clock);
  void updateHalfmoveClock(char movedPiece, char capturedPiece);
  bool isFiftyMoveRule() const;

  // Fullmove clock (starts at 1, increments after Black's move)
  int getFullmoveClock() const;
  void setFullmoveClock(int clock);
  void incrementFullmoveClock(char sideJustMoved);

  // Threefold repetition detection (Zobrist hash-based)
  uint64_t computeZobristHash(const char board[8][8], char sideToMove) const;
  void recordPosition(const char board[8][8], char sideToMove);
  void clearPositionHistory();
  bool isThreefoldRepetition() const;

  // Main move generation function
  void getPossibleMoves(const char board[8][8], int row, int col, int& moveCount, int moves[][2]);

  // Move validation
  bool isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol);

  // Game state checks
  bool findKingPosition(const char board[8][8], char kingColor, int& kingRow, int& kingCol) const;
  bool isKingInCheck(const char board[8][8], char kingColor);
  bool isPawnPromotion(char piece, int targetRow);
  bool hasAnyLegalMove(const char board[8][8], char color);
  bool isCheckmate(const char board[8][8], char kingColor);
  bool isStalemate(const char board[8][8], char colorToMove);
};

#endif // CHESS_ENGINE_H