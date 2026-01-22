#include "chess_engine.h"
#include "chess_utils.h"
#include <Arduino.h>

// ---------------------------
// ChessEngine Implementation
// ---------------------------

ChessEngine::ChessEngine() : castlingRights(0x0F), enPassantTargetRow(-1), enPassantTargetCol(-1) {}

void ChessEngine::setCastlingRights(uint8_t rights) {
  castlingRights = rights;
}

uint8_t ChessEngine::getCastlingRights() const {
  return castlingRights;
}

void ChessEngine::setEnPassantTarget(int row, int col) {
  enPassantTargetRow = row;
  enPassantTargetCol = col;
}

void ChessEngine::clearEnPassantTarget() {
  enPassantTargetRow = -1;
  enPassantTargetCol = -1;
}

void ChessEngine::getEnPassantTarget(int& row, int& col) const {
  row = enPassantTargetRow;
  col = enPassantTargetCol;
}

bool ChessEngine::hasEnPassantTarget() const {
  return enPassantTargetRow != -1 && enPassantTargetCol != -1;
}

// Generate pseudo-legal moves (without check filtering)
void ChessEngine::getPseudoLegalMoves(const char board[8][8], int row, int col, int& moveCount, int moves[][2], bool includeCastling) {
  moveCount = 0;
  char piece = board[row][col];

  if (piece == ' ')
    return; // Empty square

  char pieceColor = ChessUtils::getPieceColor(piece);

  switch (toupper(piece)) {
    case 'P': // Pawn
      addPawnMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'R': // Rook
      addRookMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'N': // Knight
      addKnightMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'B': // Bishop
      addBishopMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'Q': // Queen
      addQueenMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'K': // King
      addKingMoves(board, row, col, pieceColor, moveCount, moves, includeCastling);
      break;
  }
}

// Main move generation function (returns only legal moves)
void ChessEngine::getPossibleMoves(const char board[8][8], int row, int col, int& moveCount, int moves[][2]) {
  // First generate all pseudo-legal moves
  int pseudoMoves[28][2];
  int pseudoMoveCount = 0;

  getPseudoLegalMoves(board, row, col, pseudoMoveCount, pseudoMoves, true);

  // Filter out moves that would leave the king in check
  moveCount = 0;
  for (int i = 0; i < pseudoMoveCount; i++) {
    int toRow = pseudoMoves[i][0];
    int toCol = pseudoMoves[i][1];

    // Only add this move if it doesn't leave the king in check
    if (!wouldMoveLeaveKingInCheck(board, row, col, toRow, toCol)) {
      moves[moveCount][0] = toRow;
      moves[moveCount][1] = toCol;
      moveCount++;
    }
  }
}

// Pawn move generation
void ChessEngine::addPawnMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  // Board layout: row 0 = rank 8 (black), row 7 = rank 1 (White)
  // White pawns move from row 6 (rank 2) toward row 0 (rank 8): direction -1
  // Black pawns move from row 1 (rank 7) toward row 7 (rank 1): direction +1
  int direction = (pieceColor == 'w') ? -1 : 1;

  // One square forward
  if (isValidSquare(row + direction, col) && isSquareEmpty(board, row + direction, col)) {
    moves[moveCount][0] = row + direction;
    moves[moveCount][1] = col;
    moveCount++;

    // Initial two-square move
    // White pawns start at row 6 (rank 2), Black pawns start at row 1 (rank 7)
    if ((pieceColor == 'w' && row == 6) || (pieceColor == 'b' && row == 1))
      if (isSquareEmpty(board, row + 2 * direction, col)) {
        moves[moveCount][0] = row + 2 * direction;
        moves[moveCount][1] = col;
        moveCount++;
      }
  }

  // Diagonal captures
  int captureColumns[] = {col - 1, col + 1};
  for (int i = 0; i < 2; i++) {
    int captureRow = row + direction;
    int captureCol = captureColumns[i];

    if (isValidSquare(captureRow, captureCol) &&
        isSquareOccupiedByOpponent(board, captureRow, captureCol, pieceColor)) {
      moves[moveCount][0] = captureRow;
      moves[moveCount][1] = captureCol;
      moveCount++;
    }
  }

  // En passant captures
  if (hasEnPassantTarget()) {
    // The pawn must be on the correct rank (white on row 3/rank 5, black on row 4/rank 4) and the en passant target must be diagonally forward
    if ((pieceColor == 'w' && row == 3) || (pieceColor == 'b' && row == 4)) {
      for (int i = 0; i < 2; i++) {
        int captureCol = captureColumns[i];
        if (captureCol == enPassantTargetCol && row + direction == enPassantTargetRow) {
          moves[moveCount][0] = enPassantTargetRow;
          moves[moveCount][1] = enPassantTargetCol;
          moveCount++;
        }
      }
    }
  }
}

// Rook move generation
void ChessEngine::addRookMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  for (int d = 0; d < 4; d++)
    for (int step = 1; step < 8; step++) {
      int newRow = row + step * directions[d][0];
      int newCol = col + step * directions[d][1];

      if (!isValidSquare(newRow, newCol))
        break;

      if (isSquareEmpty(board, newRow, newCol)) {
        moves[moveCount][0] = newRow;
        moves[moveCount][1] = newCol;
        moveCount++;
      } else {
        // Check if it's a capturable piece
        if (isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
          moves[moveCount][0] = newRow;
          moves[moveCount][1] = newCol;
          moveCount++;
        }
        break; // Can't move past any piece
      }
    }
}

// Knight move generation
void ChessEngine::addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  int knightMoves[8][2] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + knightMoves[i][0];
    int newCol = col + knightMoves[i][1];

    if (isValidSquare(newRow, newCol))
      if (isSquareEmpty(board, newRow, newCol) ||
          isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
        moves[moveCount][0] = newRow;
        moves[moveCount][1] = newCol;
        moveCount++;
      }
  }
}

// Bishop move generation
void ChessEngine::addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

  for (int d = 0; d < 4; d++)
    for (int step = 1; step < 8; step++) {
      int newRow = row + step * directions[d][0];
      int newCol = col + step * directions[d][1];

      if (!isValidSquare(newRow, newCol))
        break;

      if (isSquareEmpty(board, newRow, newCol)) {
        moves[moveCount][0] = newRow;
        moves[moveCount][1] = newCol;
        moveCount++;
      } else {
        // Check if it's a capturable piece
        if (isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
          moves[moveCount][0] = newRow;
          moves[moveCount][1] = newCol;
          moveCount++;
        }
        break; // Can't move past any piece
      }
    }
}

// Queen move generation (combination of rook and bishop)
void ChessEngine::addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  addRookMoves(board, row, col, pieceColor, moveCount, moves);
  addBishopMoves(board, row, col, pieceColor, moveCount, moves);
}

// King move generation
void ChessEngine::addKingMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2], bool includeCastling) {
  int kingMoves[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + kingMoves[i][0];
    int newCol = col + kingMoves[i][1];

    if (isValidSquare(newRow, newCol))
      if (isSquareEmpty(board, newRow, newCol) ||
          isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
        moves[moveCount][0] = newRow;
        moves[moveCount][1] = newCol;
        moveCount++;
      }
  }

  if (includeCastling)
    addCastlingMoves(board, row, col, pieceColor, moveCount, moves);
}

bool ChessEngine::hasCastlingRight(char pieceColor, bool kingSide) const {
  if (pieceColor == 'w')
    return kingSide ? ((castlingRights & 0x01) != 0) : ((castlingRights & 0x02) != 0);
  return kingSide ? ((castlingRights & 0x04) != 0) : ((castlingRights & 0x08) != 0);
}

void ChessEngine::addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  // Castling is only possible from the starting king square.
  // Board layout: row 0 = rank 8, row 7 = rank 1.
  int homeRow = (pieceColor == 'w') ? 7 : 0;
  char kingPiece = (pieceColor == 'w') ? 'K' : 'k';
  char rookPiece = (pieceColor == 'w') ? 'R' : 'r';

  if (row != homeRow || col != 4) return;
  if (board[row][col] != kingPiece) return;

  // King cannot castle while in check.
  if (isSquareUnderAttack(board, row, col, pieceColor)) return;

  // King-side castling (e -> g)
  if (hasCastlingRight(pieceColor, true))
    // Squares between king and rook must be empty: f, g
    if (board[homeRow][5] == ' ' && board[homeRow][6] == ' ' && board[homeRow][7] == rookPiece)
      // Squares king passes through must not be under attack: f, g
      if (!isSquareUnderAttack(board, homeRow, 5, pieceColor) && !isSquareUnderAttack(board, homeRow, 6, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 6;
        moveCount++;
      }

  // Queen-side castling (e -> c)
  if (hasCastlingRight(pieceColor, false))
    // Squares between king and rook must be empty: d, c, b
    if (board[homeRow][3] == ' ' && board[homeRow][2] == ' ' && board[homeRow][1] == ' ' && board[homeRow][0] == rookPiece)
      // Squares king passes through must not be under attack: d, c
      if (!isSquareUnderAttack(board, homeRow, 3, pieceColor) && !isSquareUnderAttack(board, homeRow, 2, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 2;
        moveCount++;
      }
}

// Helper function to check if a square is occupied by an opponent piece
bool ChessEngine::isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor) {
  char targetPiece = board[row][col];
  if (targetPiece == ' ')
    return false;

  char targetColor = ChessUtils::getPieceColor(targetPiece);
  return targetColor != pieceColor;
}

// Helper function to check if a square is empty
bool ChessEngine::isSquareEmpty(const char board[8][8], int row, int col) {
  return board[row][col] == ' ';
}

// Helper function to check if coordinates are within board bounds
bool ChessEngine::isValidSquare(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8;
}

// Move validation
bool ChessEngine::isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol) {
  int moveCount = 0;
  int moves[28][2]; // Maximum possible moves for a queen

  getPossibleMoves(board, fromRow, fromCol, moveCount, moves);

  // First check if it's a pseudo-legal move (piece can move there according to its movement rules)
  bool isPseudoLegal = false;
  for (int i = 0; i < moveCount; i++)
    if (moves[i][0] == toRow && moves[i][1] == toCol) {
      isPseudoLegal = true;
      break;
    }

  if (!isPseudoLegal)
    return false; // Not even a valid move according to piece rules

  // Check if this move would leave the king in check (illegal move)
  if (wouldMoveLeaveKingInCheck(board, fromRow, fromCol, toRow, toCol))
    return false; // Move would leave king in check

  return true; // Move is legal
}

// Check if a pawn move results in promotion
bool ChessEngine::isPawnPromotion(char piece, int targetRow) {
  // Board layout: row 0 = rank 8, row 7 = rank 1
  if (piece == 'P' && targetRow == 0)
    return true; // White pawn reaches row 0 (rank 8)
  if (piece == 'p' && targetRow == 7)
    return true; // Black pawn reaches row 7 (rank
  return false;
}

// Get the promoted piece (always queen for now)
char ChessEngine::getPromotedPiece(char piece) {
  return (piece == 'P') ? 'Q' : 'q';
}

// Utility function to print a move in readable format
void ChessEngine::printMove(int fromRow, int fromCol, int toRow, int toCol) {
  Serial.printf("%c%d to %c%d\n", (char)('a' + fromCol), fromRow + 1, (char)('a' + toCol), toRow + 1);
}

// Convert algebraic notation file (a-h) to column index (0-7)
char ChessEngine::algebraicToCol(char file) {
  return file - 'a';
}

// Convert algebraic notation rank (1-8) to row index (0-7)
int ChessEngine::algebraicToRow(int rank) {
  return rank - 1;
}

// ---------------------------
// Check Detection Functions
// ---------------------------

// Find the king position for a given color
bool ChessEngine::findKing(const char board[8][8], char kingColor, int& kingRow, int& kingCol) {
  char kingPiece = (kingColor == 'w') ? 'K' : 'k';

  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      if (board[row][col] == kingPiece) {
        kingRow = row;
        kingCol = col;
        return true;
      }
  return false; // King not found (shouldn't happen in a valid game)
}

// Check if a square is under attack by the opponent
bool ChessEngine::isSquareUnderAttack(const char board[8][8], int row, int col, char defendingColor) {
  char attackingColor = (defendingColor == 'w') ? 'b' : 'w';

  // Check all opponent pieces to see if any can attack this square
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++) {
      char piece = board[r][c];
      if (piece == ' ') continue;

      char pieceColor = ChessUtils::getPieceColor(piece);
      if (pieceColor != attackingColor) continue;

      // Pawns are special: their attack pattern differs from their move pattern.
      if (toupper(piece) == 'P') {
        int direction = (pieceColor == 'w') ? -1 : 1;
        if (r + direction == row && (c - 1 == col || c + 1 == col))
          return true;
        continue;
      }

      // Get pseudo-legal moves for this enemy piece (no check filtering to avoid recursion)
      int moveCount = 0;
      int moves[28][2];
      // IMPORTANT: for attack detection, do NOT include castling moves
      getPseudoLegalMoves(board, r, c, moveCount, moves, false);

      // Check if any of those moves target our square
      for (int i = 0; i < moveCount; i++)
        if (moves[i][0] == row && moves[i][1] == col)
          return true; // Square is under attack
    }

  return false; // Square is safe
}

// Make a temporary move on a board copy
void ChessEngine::makeMove(char board[8][8], int fromRow, int fromCol, int toRow, int toCol, char& capturedPiece) {
  capturedPiece = board[toRow][toCol];
  char movingPiece = board[fromRow][fromCol];

  board[toRow][toCol] = movingPiece;
  board[fromRow][fromCol] = ' ';

  // Handle castling as a compound move (move rook too)
  if (toupper(movingPiece) == 'K' && fromRow == toRow) {
    int deltaCol = toCol - fromCol;
    if (deltaCol == 2) {
      // King-side: rook h-file -> f-file
      int rookFromCol = 7;
      int rookToCol = 5;
      char rookPiece = (movingPiece >= 'a' && movingPiece <= 'z') ? 'r' : 'R';
      if (board[toRow][rookFromCol] == rookPiece) {
        board[toRow][rookToCol] = rookPiece;
        board[toRow][rookFromCol] = ' ';
      }
    } else if (deltaCol == -2) {
      // Queen-side: rook a-file -> d-file
      int rookFromCol = 0;
      int rookToCol = 3;
      char rookPiece = (movingPiece >= 'a' && movingPiece <= 'z') ? 'r' : 'R';
      if (board[toRow][rookFromCol] == rookPiece) {
        board[toRow][rookToCol] = rookPiece;
        board[toRow][rookFromCol] = ' ';
      }
    }
  }

  // Handle en passant capture
  if (toupper(movingPiece) == 'P' && hasEnPassantTarget()) {
    // If moving to the en passant target square, remove the captured pawn
    if (toRow == enPassantTargetRow && toCol == enPassantTargetCol && capturedPiece == ' ') {
      // The captured pawn is one square behind the target (in the opposite direction)
      int capturedPawnRow = toRow - ((ChessUtils::getPieceColor(movingPiece) == 'w') ? -1 : 1);
      capturedPiece = board[capturedPawnRow][toCol];
      board[capturedPawnRow][toCol] = ' ';
    }
  }
}

// Check if a move would leave the king in check
bool ChessEngine::wouldMoveLeaveKingInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol) {
  // Create a copy of the board to test the move
  char testBoard[8][8];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      testBoard[r][c] = board[r][c];

  // Get the color of the piece being moved
  char movingPiece = testBoard[fromRow][fromCol];
  char movingColor = ChessUtils::getPieceColor(movingPiece);

  // Make the move on the test board
  char capturedPiece;
  makeMove(testBoard, fromRow, fromCol, toRow, toCol, capturedPiece);

  // Find the king (it might have moved if the piece being moved was the king)
  int kingRow, kingCol;
  bool kingFound = findKing(testBoard, movingColor, kingRow, kingCol);

  if (!kingFound)
    return true; // If king not found, move is definitely illegal

  // Check if the king is in check after the move
  bool inCheck = isSquareUnderAttack(testBoard, kingRow, kingCol, movingColor);

  return inCheck;
}

// Check if the king of a given color is currently in check
bool ChessEngine::isKingInCheck(const char board[8][8], char kingColor) {
  int kingRow, kingCol;

  if (!findKing(board, kingColor, kingRow, kingCol))
    return false; // King not found

  return isSquareUnderAttack(board, kingRow, kingCol, kingColor);
}

// Check if the king is in checkmate
bool ChessEngine::isCheckmate(const char board[8][8], char kingColor) {
  // First, the king must be in check
  if (!isKingInCheck(board, kingColor))
    return false;

  // Check if any legal move exists that would get out of check
  for (int fromRow = 0; fromRow < 8; fromRow++)
    for (int fromCol = 0; fromCol < 8; fromCol++) {
      char piece = board[fromRow][fromCol];
      if (piece == ' ') continue;

      char pieceColor = ChessUtils::getPieceColor(piece);
      if (pieceColor != kingColor) continue;

      // Get all possible moves for this piece
      int moveCount = 0;
      int moves[28][2];
      getPossibleMoves(board, fromRow, fromCol, moveCount, moves);

      // Check each possible move
      for (int i = 0; i < moveCount; i++) {
        int toRow = moves[i][0];
        int toCol = moves[i][1];

        // If this move doesn't leave king in check, it's not checkmate
        if (!wouldMoveLeaveKingInCheck(board, fromRow, fromCol, toRow, toCol))
          return false; // Found a legal move
      }
    }

  return true; // No legal moves found - it's checkmate
}

// Check if the game is in stalemate
bool ChessEngine::isStalemate(const char board[8][8], char colorToMove) {
  // Stalemate: not in check, but no legal moves
  if (isKingInCheck(board, colorToMove))
    return false; // In check, can't be stalemate

  // Check if any legal move exists
  for (int fromRow = 0; fromRow < 8; fromRow++)
    for (int fromCol = 0; fromCol < 8; fromCol++) {
      char piece = board[fromRow][fromCol];
      if (piece == ' ') continue;

      char pieceColor = ChessUtils::getPieceColor(piece);
      if (pieceColor != colorToMove) continue;

      // Get all possible moves for this piece
      int moveCount = 0;
      int moves[28][2];
      getPossibleMoves(board, fromRow, fromCol, moveCount, moves);

      // Check each possible move
      for (int i = 0; i < moveCount; i++) {
        int toRow = moves[i][0];
        int toCol = moves[i][1];

        // If this move doesn't leave king in check, it's a legal move
        if (!wouldMoveLeaveKingInCheck(board, fromRow, fromCol, toRow, toCol))
          return false; // Found a legal move - not stalemate
      }
    }

  return true; // No legal moves and not in check - it's stalemate
}