#ifndef CHESS_UTILS_H
#define CHESS_UTILS_H

#include "led_colors.h"
#include <Arduino.h>

// Forward declaration
class ChessEngine;

class ChessUtils {
 public:
  static const char* colorName(char color) {
    return (color == 'w') ? "White" : ((color == 'b') ? "Black" : "Unknown");
  }

  static LedRGB colorLed(char color) {
    return (color == 'w') ? LedColors::White : ((color == 'b') ? LedColors::Blue : LedColors::Off);
  }

  static char getPieceColor(char piece) {
    return (piece >= 'a' && piece <= 'z') ? 'b' : 'w';
  }

  static bool isWhitePiece(char piece) {
    return (piece >= 'A' && piece <= 'Z');
  }

  static bool isBlackPiece(char piece) {
    return (piece >= 'a' && piece <= 'z');
  }

  static bool isEnPassantMove(int fromRow, int fromCol, int toRow, int toCol, char piece, char capturedPiece) {
    return (toupper(piece) == 'P' && fromCol != toCol && capturedPiece == ' ');
  }

  static int getEnPassantCapturedPawnRow(int toRow, char piece) {
    return toRow - (isWhitePiece(piece) ? -1 : 1);
  }

  static bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
    return (toupper(piece) == 'K' && fromRow == toRow && (toCol - fromCol == 2 || toCol - fromCol == -2));
  }

  // Convert castling rights bitmask (KQkq) to string used in FEN.
  // rights: bitmask where 0x01=K, 0x02=Q, 0x04=k, 0x08=q
  static String castlingRightsToString(uint8_t rights);
  static uint8_t castlingRightsFromString(const String& rightsStr);

  // Convert board state to FEN notation
  // board: 8x8 array representing the chess board
  // currentTurn: 'w' for White's turn, 'b' for Black's turn
  // chessEngine: ChessEngine pointer to get castling rights and en passant target square
  // Returns: FEN string representation
  static String boardToFEN(const char board[8][8], char currentTurn, ChessEngine* chessEngine = nullptr);

  // Parse FEN notation and update board state
  // fen: FEN string to parse
  // board: 8x8 array to update with parsed position
  // currentTurn: output parameter for whose turn it is - 'w' or 'b' (optional)
  // chessEngine: ChessEngine pointer to set castling rights and en passant target square
  static void fenToBoard(const String& fen, char board[8][8], char& currentTurn, ChessEngine* chessEngine = nullptr);

  // Print current board state to Serial for debugging
  // board: 8x8 array representing the chess board
  static void printBoard(const char board[8][8]);

  // Evaluate board position using simple material count
  // Returns evaluation in pawns (positive = White advantage, negative = Black advantage)
  // Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9
  static float evaluatePosition(const char board[8][8]);

  // Convert array coordinates to a UCI move string (e.g. "e2e4", "e7e8q")
  static String toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Parse a UCI move string (e.g. "e2e4", "e7e8q") into array coordinates
  // Returns true if the string is a valid UCI move; fills promotion with the
  // promotion character (or ' ' if none)
  static bool parseUCIMove(const String& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

  // Initialize NVS for ESP32 (required before Preferences.begin)
  static bool ensureNvsInitialized();
};

#endif // CHESS_UTILS_H
