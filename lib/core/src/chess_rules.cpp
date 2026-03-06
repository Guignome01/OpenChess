#include "chess_rules.h"
#include "chess_utils.h"
#include <cctype>
#include <cstring>

// ---------------------------
// ChessRules Implementation (stateless)
// ---------------------------

bool ChessRules::hasLegalEnPassantCapture(const char board[8][8], char sideToMove, const PositionState& flags) {
  if (flags.epRow < 0 || flags.epCol < 0) return false;
  int capturerRow = (sideToMove == 'w') ? flags.epRow + 1 : flags.epRow - 1;
  char capturerPawn = (sideToMove == 'w') ? 'P' : 'p';
  if (flags.epCol > 0 && board[capturerRow][flags.epCol - 1] == capturerPawn && !leavesInCheck(board, capturerRow, flags.epCol - 1, flags.epRow, flags.epCol, flags))
    return true;
  if (flags.epCol < 7 && board[capturerRow][flags.epCol + 1] == capturerPawn && !leavesInCheck(board, capturerRow, flags.epCol + 1, flags.epRow, flags.epCol, flags))
    return true;
  return false;
}

// Generate pseudo-legal moves (without check filtering)
void ChessRules::getPseudoLegalMoves(const char board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling) {
  moveCount = 0;
  char piece = board[row][col];

  if (piece == ' ')
    return;

  char pieceColor = ChessUtils::getPieceColor(piece);

  switch (toupper(piece)) {
    case 'P':
      addPawnMoves(board, row, col, pieceColor, flags, moveCount, moves);
      break;
    case 'R':
      addRookMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'N':
      addKnightMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'B':
      addBishopMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'Q':
      addQueenMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case 'K':
      addKingMoves(board, row, col, pieceColor, flags, moveCount, moves, includeCastling);
      break;
  }
}

// Main move generation function (returns only legal moves)
void ChessRules::getPossibleMoves(const char board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2]) {
  int pseudoMoves[MAX_MOVES_PER_PIECE][2];
  int pseudoMoveCount = 0;

  getPseudoLegalMoves(board, row, col, flags, pseudoMoveCount, pseudoMoves, true);

  moveCount = 0;
  for (int i = 0; i < pseudoMoveCount; i++) {
    int toRow = pseudoMoves[i][0];
    int toCol = pseudoMoves[i][1];

    if (!leavesInCheck(board, row, col, toRow, toCol, flags)) {
      moves[moveCount][0] = toRow;
      moves[moveCount][1] = toCol;
      moveCount++;
    }
  }
}

// Pawn move generation
void ChessRules::addPawnMoves(const char board[8][8], int row, int col, char pieceColor, const PositionState& flags, int& moveCount, int moves[][2]) {
  int direction = (pieceColor == 'w') ? -1 : 1;

  // One square forward
  if (isValidSquare(row + direction, col) && isSquareEmpty(board, row + direction, col)) {
    moves[moveCount][0] = row + direction;
    moves[moveCount][1] = col;
    moveCount++;

    // Initial two-square move
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
  if (flags.epRow >= 0 && flags.epCol >= 0) {
    if ((pieceColor == 'w' && row == 3) || (pieceColor == 'b' && row == 4)) {
      for (int i = 0; i < 2; i++) {
        int captureCol = captureColumns[i];
        if (captureCol == flags.epCol && row + direction == flags.epRow) {
          moves[moveCount][0] = flags.epRow;
          moves[moveCount][1] = flags.epCol;
          moveCount++;
        }
      }
    }
  }
}

// Sliding piece move generation (shared by rook, bishop, queen)
void ChessRules::addSlidingMoves(const char board[8][8], int row, int col, char pieceColor, const int directions[][2], int dirCount, int& moveCount, int moves[][2]) {
  for (int d = 0; d < dirCount; d++)
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
        if (isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
          moves[moveCount][0] = newRow;
          moves[moveCount][1] = newCol;
          moveCount++;
        }
        break;
      }
    }
}

// Rook move generation
void ChessRules::addRookMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  static const int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moveCount, moves);
}

// Knight move generation
void ChessRules::addKnightMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
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
void ChessRules::addBishopMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  static const int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moveCount, moves);
}

// Queen move generation (combination of rook and bishop)
void ChessRules::addQueenMoves(const char board[8][8], int row, int col, char pieceColor, int& moveCount, int moves[][2]) {
  addRookMoves(board, row, col, pieceColor, moveCount, moves);
  addBishopMoves(board, row, col, pieceColor, moveCount, moves);
}

// King move generation
void ChessRules::addKingMoves(const char board[8][8], int row, int col, char pieceColor, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling) {
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
    addCastlingMoves(board, row, col, pieceColor, flags.castlingRights, moveCount, moves);
}

bool ChessRules::hasCastlingRight(uint8_t castlingRights, char pieceColor, bool kingSide) {
  if (pieceColor == 'w')
    return kingSide ? ((castlingRights & 0x01) != 0) : ((castlingRights & 0x02) != 0);
  return kingSide ? ((castlingRights & 0x04) != 0) : ((castlingRights & 0x08) != 0);
}

void ChessRules::addCastlingMoves(const char board[8][8], int row, int col, char pieceColor, uint8_t castlingRights, int& moveCount, int moves[][2]) {
  int homeRow = (pieceColor == 'w') ? 7 : 0;
  char kingPiece = (pieceColor == 'w') ? 'K' : 'k';
  char rookPiece = (pieceColor == 'w') ? 'R' : 'r';

  if (row != homeRow || col != 4) return;
  if (board[row][col] != kingPiece) return;

  // King cannot castle while in check.
  if (isSquareUnderAttack(board, row, col, pieceColor)) return;

  // King-side castling (e -> g)
  if (hasCastlingRight(castlingRights, pieceColor, true))
    if (board[homeRow][5] == ' ' && board[homeRow][6] == ' ' && board[homeRow][7] == rookPiece)
      if (!isSquareUnderAttack(board, homeRow, 5, pieceColor) && !isSquareUnderAttack(board, homeRow, 6, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 6;
        moveCount++;
      }

  // Queen-side castling (e -> c)
  if (hasCastlingRight(castlingRights, pieceColor, false))
    if (board[homeRow][3] == ' ' && board[homeRow][2] == ' ' && board[homeRow][1] == ' ' && board[homeRow][0] == rookPiece)
      if (!isSquareUnderAttack(board, homeRow, 3, pieceColor) && !isSquareUnderAttack(board, homeRow, 2, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 2;
        moveCount++;
      }
}

// Helper functions
bool ChessRules::isSquareOccupiedByOpponent(const char board[8][8], int row, int col, char pieceColor) {
  char targetPiece = board[row][col];
  if (targetPiece == ' ')
    return false;

  char targetColor = ChessUtils::getPieceColor(targetPiece);
  return targetColor != pieceColor;
}

bool ChessRules::isSquareEmpty(const char board[8][8], int row, int col) {
  return board[row][col] == ' ';
}

bool ChessRules::isValidSquare(int row, int col) {
  return ChessUtils::isValidSquare(row, col);
}

// Move validation
bool ChessRules::isValidMove(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags) {
  int moveCount = 0;
  int moves[MAX_MOVES_PER_PIECE][2];

  // getPossibleMoves already filters out moves that leave the king in check,
  // so we only need to check if the target square is in the legal move list.
  getPossibleMoves(board, fromRow, fromCol, flags, moveCount, moves);

  for (int i = 0; i < moveCount; i++)
    if (moves[i][0] == toRow && moves[i][1] == toCol)
      return true;

  return false;
}

// Check if a pawn move results in promotion
bool ChessRules::isPromotion(char piece, int targetRow) {
  if (piece == 'P' && targetRow == 0)
    return true;
  if (piece == 'p' && targetRow == 7)
    return true;
  return false;
}

// ---------------------------
// Check Detection Functions
// ---------------------------

bool ChessRules::isSquareUnderAttack(const char board[8][8], int row, int col, char defendingColor) {
  char attackingColor = ChessUtils::opponentColor(defendingColor);

  // Empty flags — attack detection doesn't need castling or en passant.
  // Pawns are handled separately (attack pattern only, not en passant).
  // Non-pawn pieces don't use castling in attack mode (includeCastling=false).
  PositionState emptyFlags;
  emptyFlags.castlingRights = 0;
  emptyFlags.epRow = -1;
  emptyFlags.epCol = -1;

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

      int moveCount = 0;
      int moves[MAX_MOVES_PER_PIECE][2];
      // IMPORTANT: for attack detection, do NOT include castling moves
      getPseudoLegalMoves(board, r, c, emptyFlags, moveCount, moves, false);

      for (int i = 0; i < moveCount; i++)
        if (moves[i][0] == row && moves[i][1] == col)
          return true;
    }

  return false;
}

// Apply a temporary move on a board copy (for check detection)
void ChessRules::applyMove(char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags, char& capturedPiece) {
  capturedPiece = board[toRow][toCol];
  char movingPiece = board[fromRow][fromCol];

  board[toRow][toCol] = movingPiece;
  board[fromRow][fromCol] = ' ';

  // Handle castling as a compound move (move rook too)
  auto ci = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, movingPiece);
  if (ci.isCastling) {
    char rookPiece = ChessUtils::makePiece('R', ChessUtils::getPieceColor(movingPiece));
    if (board[toRow][ci.rookFromCol] == rookPiece) {
      board[toRow][ci.rookToCol] = rookPiece;
      board[toRow][ci.rookFromCol] = ' ';
    }
  }

  // Handle en passant capture
  auto ei = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, movingPiece, capturedPiece);
  if (ei.isCapture) {
    capturedPiece = board[ei.capturedPawnRow][toCol];
    board[ei.capturedPawnRow][toCol] = ' ';
  }
}

bool ChessRules::leavesInCheck(const char board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags) {
  char testBoard[8][8];
  memcpy(testBoard, board, sizeof(testBoard));

  char movingPiece = testBoard[fromRow][fromCol];
  char movingColor = ChessUtils::getPieceColor(movingPiece);

  char capturedPiece;
  applyMove(testBoard, fromRow, fromCol, toRow, toCol, flags, capturedPiece);

  int kingRow, kingCol;
  bool kingFound = ChessUtils::findKingPosition(testBoard, movingColor, kingRow, kingCol);

  if (!kingFound)
    return true;

  return isSquareUnderAttack(testBoard, kingRow, kingCol, movingColor);
}

bool ChessRules::isCheck(const char board[8][8], char kingColor) {
  int kingRow, kingCol;

  if (!ChessUtils::findKingPosition(board, kingColor, kingRow, kingCol))
    return false;

  return isSquareUnderAttack(board, kingRow, kingCol, kingColor);
}

bool ChessRules::hasAnyLegalMove(const char board[8][8], char color, const PositionState& flags) {
  for (int fromRow = 0; fromRow < 8; fromRow++)
    for (int fromCol = 0; fromCol < 8; fromCol++) {
      char piece = board[fromRow][fromCol];
      if (piece == ' ') continue;

      if (ChessUtils::getPieceColor(piece) != color) continue;

      int moveCount = 0;
      int moves[MAX_MOVES_PER_PIECE][2];
      getPossibleMoves(board, fromRow, fromCol, flags, moveCount, moves);
      if (moveCount > 0)
        return true;
    }

  return false;
}

bool ChessRules::isCheckmate(const char board[8][8], char kingColor, const PositionState& flags) {
  return isCheck(board, kingColor) && !hasAnyLegalMove(board, kingColor, flags);
}

bool ChessRules::isStalemate(const char board[8][8], char colorToMove, const PositionState& flags) {
  return !isCheck(board, colorToMove) && !hasAnyLegalMove(board, colorToMove, flags);
}
