#ifndef CHESS_UTILS_H
#define CHESS_UTILS_H

#include <Arduino.h>

// Forward declaration
class ChessEngine;

class ChessUtils {
 public:
  static const char* colorName(char color) {
    return (color == 'w') ? "White" : "Black";
  }

  static const char getPieceColor(char piece) {
    return (piece >= 'a' && piece <= 'z') ? 'b' : 'w';
  }

  static bool isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece) {
    return (toupper(piece) == 'P' && fromCol != toCol && capturedPiece == ' ');
  }

  static int getEnPassantCapturedPawnRow(int toRow, char piece) {
    return toRow - ((getPieceColor(piece) == 'w') ? -1 : 1);
  }

  static bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
    return (toupper(piece) == 'K' && fromRow == toRow && (toCol - fromCol == 2 || toCol - fromCol == -2));
  }

  // Convert castling rights bitmask (KQkq) to string used in FEN.
  // rights: bitmask where 0x01=K, 0x02=Q, 0x04=k, 0x08=q
  static String castlingRightsToString(uint8_t rights);
  static uint8_t castlingRightsFromString(const String rightsStr);

  // Convert board state to FEN notation
  // board: 8x8 array representing the chess board
  // currentTurn: 'w' for white's turn, 'b' for black's turn
  // chessEngine: ChessEngine pointer to get castling rights and en passant target square
  // Returns: FEN string representation
  static String boardToFEN(const char board[8][8], char currentTurn, ChessEngine* chessEngine = nullptr);

  // Parse FEN notation and update board state
  // fen: FEN string to parse
  // board: 8x8 array to update with parsed position
  // currentTurn: output parameter for whose turn it is - 'w' or 'b' (optional)
  // chessEngine: ChessEngine pointer to set castling rights and en passant target square
  static void fenToBoard(String fen, char board[8][8], char& currentTurn, ChessEngine* chessEngine = nullptr);

  // Print current board state to Serial for debugging
  // board: 8x8 array representing the chess board
  static void printBoard(const char board[8][8]);

  // Evaluate board position using simple material count
  // Returns evaluation in pawns (positive = white advantage, negative = black advantage)
  // Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9
  static float evaluatePosition(const char board[8][8]);

  // Initialize NVS for ESP32 (required before Preferences.begin)
  static bool ensureNvsInitialized();
};

#endif // CHESS_UTILS_H
