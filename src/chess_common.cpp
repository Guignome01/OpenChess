#include "chess_common.h"

namespace ChessCommon {

void copyBoard(const char src[8][8], char dst[8][8]) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      dst[row][col] = src[row][col];
    }
  }
}

uint8_t recomputeCastlingRightsFromBoard(const char board[8][8]) {
  uint8_t rights = 0;

  if (board[7][4] == 'K') {
    if (board[7][7] == 'R') rights |= 0x01;
    if (board[7][0] == 'R') rights |= 0x02;
  }
  if (board[0][4] == 'k') {
    if (board[0][7] == 'r') rights |= 0x04;
    if (board[0][0] == 'r') rights |= 0x08;
  }

  return rights;
}

void updateCastlingRightsAfterMove(uint8_t& castlingRights, int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece) {
  // King moved => lose both rights for that color
  if (movedPiece == 'K') {
    castlingRights &= ~(0x01 | 0x02);
  } else if (movedPiece == 'k') {
    castlingRights &= ~(0x04 | 0x08);
  }

  // Rook moved from corner => lose that side's right
  if (movedPiece == 'R') {
    if (fromRow == 7 && fromCol == 7) castlingRights &= ~0x01;
    if (fromRow == 7 && fromCol == 0) castlingRights &= ~0x02;
  } else if (movedPiece == 'r') {
    if (fromRow == 0 && fromCol == 7) castlingRights &= ~0x04;
    if (fromRow == 0 && fromCol == 0) castlingRights &= ~0x08;
  }

  // Rook captured on corner => lose that side's right
  if (capturedPiece == 'R') {
    if (toRow == 7 && toCol == 7) castlingRights &= ~0x01;
    if (toRow == 7 && toCol == 0) castlingRights &= ~0x02;
  } else if (capturedPiece == 'r') {
    if (toRow == 0 && toCol == 7) castlingRights &= ~0x04;
    if (toRow == 0 && toCol == 0) castlingRights &= ~0x08;
  }
}

bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
  char upper = (piece >= 'a' && piece <= 'z') ? (piece - 32) : piece;
  return (upper == 'K' && fromRow == toRow && (toCol - fromCol == 2 || toCol - fromCol == -2));
}

void applyCastlingRookInternal(BoardDriver* boardDriver, char board[8][8], int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece) {
  int deltaCol = kingToCol - kingFromCol;
  if (kingFromRow != kingToRow) return;
  if (deltaCol != 2 && deltaCol != -2) return;

  int rookFromCol = (deltaCol == 2) ? 7 : 0;
  int rookToCol = (deltaCol == 2) ? 5 : 3;
  char rookPiece = (kingPiece >= 'a' && kingPiece <= 'z') ? 'r' : 'R';

  // Update board state
  board[kingToRow][rookToCol] = rookPiece;
  board[kingToRow][rookFromCol] = ' ';

  if (!boardDriver) return;

  // Handle LED prompts and wait for rook move
  Serial.printf("Castling: please move rook from %c%d to %c%d\n", (char)('a' + rookFromCol), 8 - kingToRow, (char)('a' + rookToCol), 8 - kingToRow);

  // Wait for rook to be lifted from its original square
  boardDriver->clearAllLEDs();
  boardDriver->setSquareLED(kingToRow, rookFromCol, LedColors::PickupCyan.r, LedColors::PickupCyan.g, LedColors::PickupCyan.b);
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::MoveWhite.r, LedColors::MoveWhite.g, LedColors::MoveWhite.b);
  boardDriver->showLEDs();

  while (boardDriver->getSensorState(kingToRow, rookFromCol)) {
    boardDriver->readSensors();
    delay(50);
  }

  // Wait for rook to be placed on destination square
  boardDriver->clearAllLEDs();
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::MoveWhite.r, LedColors::MoveWhite.g, LedColors::MoveWhite.b);
  boardDriver->showLEDs();

  while (!boardDriver->getSensorState(kingToRow, rookToCol)) {
    boardDriver->readSensors();
    delay(50);
  }

  boardDriver->clearAllLEDs();
}

bool applyPawnPromotionIfNeeded(ChessEngine* engine, char board[8][8], int toRow, int toCol, char movedPiece, char& promotedPieceOut) {
  if (!engine) return false;

  if (!engine->isPawnPromotion(movedPiece, toRow)) {
    return false;
  }

  promotedPieceOut = engine->getPromotedPiece(movedPiece);
  board[toRow][toCol] = promotedPieceOut;
  return true;
}

static bool findKingPosition(const char board[8][8], char colorToMove, int& kingRow, int& kingCol) {
  const char kingPiece = (colorToMove == 'w') ? 'K' : 'k';
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (board[row][col] == kingPiece) {
        kingRow = row;
        kingCol = col;
        return true;
      }
    }
  }
  kingRow = -1;
  kingCol = -1;
  return false;
}

bool handleGameState(BoardDriver* boardDriver, ChessEngine* engine, const char board[8][8], char colorToMove) {
  if (!boardDriver || !engine) return false;

  if (engine->isCheckmate(board, colorToMove)) {
    char winnerColor = (colorToMove == 'w') ? 'b' : 'w';
    Serial.printf("CHECKMATE! %s wins!\n", colorName(winnerColor));
    boardDriver->fireworkAnimation();
    return true;
  }

  if (engine->isStalemate(board, colorToMove)) {
    Serial.println("STALEMATE! Game is a draw.");
    boardDriver->clearAllLEDs();
    return true;
  }

  if (engine->isKingInCheck(board, colorToMove)) {
    Serial.printf("%s is in CHECK!\n", colorName(colorToMove));
    boardDriver->clearAllLEDs();

    int kingRow = -1;
    int kingCol = -1;
    if (findKingPosition(board, colorToMove, kingRow, kingCol)) {
      boardDriver->blinkSquare(kingRow, kingCol, LedColors::CheckAmber.r, LedColors::CheckAmber.g, LedColors::CheckAmber.b);
    }

    boardDriver->clearAllLEDs();
  }

  return false;
}

void confirmSquareCompletion(BoardDriver* boardDriver, int row, int col) {
  boardDriver->blinkSquare(row, col, LedColors::ConfirmGreen.r, LedColors::ConfirmGreen.g, LedColors::ConfirmGreen.b, 2);
}

} // namespace ChessCommon
