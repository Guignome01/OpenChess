#include "chess_game.h"
#include "chess_utils.h"
#include "wifi_manager_esp32.h"
#include <string.h>

const char ChessGame::INITIAL_BOARD[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, // row 0 = rank 8 (black pieces, top row)
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // row 1 = rank 7 (black pawns)
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 2 = rank 6
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 3 = rank 5
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 4 = rank 4
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 5 = rank 3
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, // row 6 = rank 2 (white pawns)
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  // row 7 = rank 1 (White pieces, bottom row)
};

ChessGame::ChessGame(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm) : boardDriver(bd), chessEngine(ce), wifiManager(wm), currentTurn('w'), gameOver(false) {}

void ChessGame::initializeBoard() {
  currentTurn = 'w';
  gameOver = false;
  memcpy(board, INITIAL_BOARD, sizeof(INITIAL_BOARD));
  chessEngine->reset();
  wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), 0);
}

void ChessGame::waitForBoardSetup() {
  Serial.println("Set up the board in the starting position...");

  bool allPresent = false;
  while (!allPresent) {
    boardDriver->readSensors();
    allPresent = true;
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (INITIAL_BOARD[row][col] != ' ' && !boardDriver->getSensorState(row, col)) {
          allPresent = false;
          break;
        }
      }
      if (!allPresent)
        break;
    }
    boardDriver->updateSetupDisplay();
    delay(SENSOR_READ_DELAY_MS);
  }

  Serial.println("Board setup complete! Game starting...");
  boardDriver->fireworkAnimation();
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
}

void ChessGame::processPlayerMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
  char capturedPiece = board[toRow][toCol];

  updateEnPassantTarget(fromRow, fromCol, toRow, piece);
  if (ChessUtils::isEnPassantMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece)) {
    capturedPiece = applyEnPassant(toRow, toCol, piece);
    Serial.printf("En passant capture of %c\n", capturedPiece);
  }

  board[toRow][toCol] = piece;
  board[fromRow][fromCol] = ' ';

  if (ChessUtils::isCastlingMove(fromRow, fromCol, toRow, toCol, piece))
    applyCastling(fromRow, fromCol, toRow, toCol, piece);
  updateCastlingRightsAfterMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece);

  Serial.printf("Player moved %c from %c%d to %c%d\n", piece, (char)('a' + fromCol), 8 - fromRow, (char)('a' + toCol), 8 - toRow);

  if (capturedPiece != ' ') {
    Serial.printf("Captured %c\n", capturedPiece);
    boardDriver->captureAnimation();
  }

  char promotedPiece = ' ';
  if (applyPawnPromotionIfNeeded(toRow, toCol, piece, promotedPiece)) {
    Serial.printf("Pawn promoted to %c\n", promotedPiece);
    boardDriver->promotionAnimation(toCol);
  }

  confirmSquareCompletion(toRow, toCol);
}

bool ChessGame::tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol, char& movedPiece) {
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      // Continue if nothing was picked up from this square
      if (!boardDriver->getSensorPrev(row, col) || boardDriver->getSensorState(row, col))
        continue;

      char piece = board[row][col];

      // Skip empty squares
      if (piece == ' ')
        continue;

      // Check if it's the correct player's piece
      if (ChessUtils::getPieceColor(piece) != playerColor) {
        Serial.printf("Wrong turn! It's %s's turn to move.\n", ChessUtils::colorName(playerColor));
        boardDriver->blinkSquare(row, col, LedColors::Red.r, LedColors::Red.g, LedColors::Red.b, 2);
        continue;
      }

      Serial.printf("Piece pickup from %c%d\n", (char)('a' + col), 8 - row);

      // Generate possible moves
      int moveCount = 0;
      int moves[28][2];
      chessEngine->getPossibleMoves(board, row, col, moveCount, moves);

      // Light up current square and possible move squares
      boardDriver->setSquareLED(row, col, LedColors::Cyan.r, LedColors::Cyan.g, LedColors::Cyan.b);

      // Highlight possible move squares (different colors for empty vs capture)
      for (int i = 0; i < moveCount; i++) {
        int r = moves[i][0];
        int c = moves[i][1];

        bool isEnPassantCapture = ChessUtils::isEnPassantMove(row, col, r, c, piece, board[r][c]);
        if (board[r][c] == ' ' && !isEnPassantCapture) {
          boardDriver->setSquareLED(r, c, LedColors::White.r, LedColors::White.g, LedColors::White.b);
        } else {
          boardDriver->setSquareLED(r, c, LedColors::Red.r, LedColors::Red.g, LedColors::Red.b);
          if (isEnPassantCapture) {
            int capturedPawnRow = ChessUtils::getEnPassantCapturedPawnRow(r, piece);
            boardDriver->setSquareLED(capturedPawnRow, c, LedColors::Purple.r, LedColors::Purple.g, LedColors::Purple.b);
          }
        }
      }
      boardDriver->showLEDs();

      // Wait for piece placement - handle both normal moves and captures
      int targetRow = -1, targetCol = -1;
      bool piecePlaced = false;

      while (!piecePlaced) {
        boardDriver->readSensors();

        // First check if the original piece was placed back
        if (boardDriver->getSensorState(row, col)) {
          targetRow = row;
          targetCol = col;
          piecePlaced = true;
          break;
        }

        // Then check all squares for a regular move or capture initiation
        for (int r2 = 0; r2 < 8; r2++) {
          for (int c2 = 0; c2 < 8; c2++) {
            // Skip the original square which was already checked
            if (r2 == row && c2 == col)
              continue;

            // Check if this would be a legal move
            bool isLegalMove = false;
            for (int i = 0; i < moveCount; i++)
              if (moves[i][0] == r2 && moves[i][1] == c2) {
                isLegalMove = true;
                break;
              }

            // If not a legal move, no need to check further
            if (!isLegalMove)
              continue;

            // For capture moves: detect when the target square is empty (captured piece removed)
            // This works whether the piece was just removed or was already removed before pickup
            bool isEnPassantCapture = ChessUtils::isEnPassantMove(row, col, r2, c2, piece, board[r2][c2]);
            int enPassantCapturedPawnRow = ChessUtils::getEnPassantCapturedPawnRow(r2, piece);
            auto isCapturedPiecePickedUp = [&]() -> bool {
              if (isEnPassantCapture)
                return !boardDriver->getSensorState(enPassantCapturedPawnRow, c2);
              else
                return !boardDriver->getSensorState(r2, c2);
            };
            if ((board[r2][c2] != ' ' || isEnPassantCapture) && isCapturedPiecePickedUp()) {
              Serial.printf("Capture initiated at %c%d\n", (char)('a' + c2), 8 - r2);
              // Store the target square and wait for the capturing piece to be placed there
              targetRow = r2;
              targetCol = c2;
              piecePlaced = true;
              if (isEnPassantCapture)
                boardDriver->setSquareLED(enPassantCapturedPawnRow, c2, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b);
              // Blink the capture square to indicate waiting for piece placement
              boardDriver->blinkSquare(r2, c2, LedColors::Red.r, LedColors::Red.g, LedColors::Red.b, 2, false);
              // Wait for the capturing piece to be placed (or returned to origin to cancel)
              while (!boardDriver->getSensorState(r2, c2)) {
                boardDriver->readSensors();
                // Allow cancellation by placing the piece back to its original position
                if (boardDriver->getSensorState(row, col)) {
                  Serial.println("Capture cancelled");
                  targetRow = row;
                  targetCol = col;
                  break;
                }
                delay(SENSOR_READ_DELAY_MS);
              }
              break;
            }

            // For normal non-capture moves: detect when a piece is placed on an empty square
            if ((board[r2][c2] == ' ' && !isEnPassantCapture) && boardDriver->getSensorState(r2, c2)) {
              targetRow = r2;
              targetCol = c2;
              piecePlaced = true;
              break;
            }
          }
        }

        delay(SENSOR_READ_DELAY_MS);
      }

      if (targetRow == row && targetCol == col) {
        Serial.println("Pickup cancelled");
        boardDriver->clearAllLEDs();
        return false;
      }

      bool legalMove = false;
      for (int i = 0; i < moveCount; i++)
        if (moves[i][0] == targetRow && moves[i][1] == targetCol) {
          legalMove = true;
          break;
        }

      if (!legalMove) {
        Serial.println("Illegal move, reverting");
        boardDriver->clearAllLEDs();
        return false;
      }

      fromRow = row;
      fromCol = col;
      toRow = targetRow;
      toCol = targetCol;
      movedPiece = piece;

      boardDriver->clearAllLEDs();
      return true;
    }

  return false;
}

bool ChessGame::findKingPosition(char colorToMove, int& kingRow, int& kingCol) {
  const char kingPiece = (colorToMove == 'w') ? 'K' : 'k';
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      if (board[row][col] == kingPiece) {
        kingRow = row;
        kingCol = col;
        return true;
      }
  kingRow = -1;
  kingCol = -1;
  return false;
}

void ChessGame::updateGameStatus() {
  currentTurn = (currentTurn == 'w') ? 'b' : 'w';

  if (chessEngine->isCheckmate(board, currentTurn)) {
    char winnerColor = (currentTurn == 'w') ? 'b' : 'w';
    Serial.printf("CHECKMATE! %s wins!\n", ChessUtils::colorName(winnerColor));
    boardDriver->fireworkAnimation();
    gameOver = true;
    return;
  }

  if (chessEngine->isStalemate(board, currentTurn)) {
    Serial.println("STALEMATE! Game is a draw.");
    boardDriver->clearAllLEDs();
    gameOver = true;
    return;
  }

  if (chessEngine->isKingInCheck(board, currentTurn)) {
    Serial.printf("%s is in CHECK!\n", ChessUtils::colorName(currentTurn));
    boardDriver->clearAllLEDs();

    int kingRow = -1;
    int kingCol = -1;
    if (findKingPosition(currentTurn, kingRow, kingCol))
      boardDriver->blinkSquare(kingRow, kingCol, LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b);

    boardDriver->clearAllLEDs();
  }

  Serial.printf("It's %s's turn !\n", ChessUtils::colorName(currentTurn));
}

void ChessGame::setBoardState(char newBoardState[8][8]) {
  memcpy(board, newBoardState, 64);
  recomputeCastlingRightsFromBoard();
  // Update sensor previous state to match new board
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
  ChessUtils::printBoard(board);
}

void ChessGame::setBoardStateFromFEN(const String& fen) {
  ChessUtils::fenToBoard(fen, board, currentTurn, chessEngine);
  // Update sensor previous state to match new board
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
  Serial.println("Board state set from FEN: " + fen);
  ChessUtils::printBoard(board);
}

void ChessGame::recomputeCastlingRightsFromBoard() {
  uint8_t rights = 0;

  if (board[7][4] == 'K') {
    if (board[7][7] == 'R') rights |= 0x01;
    if (board[7][0] == 'R') rights |= 0x02;
  }
  if (board[0][4] == 'k') {
    if (board[0][7] == 'r') rights |= 0x04;
    if (board[0][0] == 'r') rights |= 0x08;
  }

  chessEngine->setCastlingRights(rights);
}

void ChessGame::updateCastlingRightsAfterMove(int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece) {
  uint8_t rights = chessEngine->getCastlingRights();

  // King moved => lose both rights for that color
  if (movedPiece == 'K')
    rights &= ~(0x01 | 0x02);
  else if (movedPiece == 'k')
    rights &= ~(0x04 | 0x08);

  // Rook moved from corner => lose that side's right
  if (movedPiece == 'R') {
    if (fromRow == 7 && fromCol == 7) rights &= ~0x01;
    if (fromRow == 7 && fromCol == 0) rights &= ~0x02;
  } else if (movedPiece == 'r') {
    if (fromRow == 0 && fromCol == 7) rights &= ~0x04;
    if (fromRow == 0 && fromCol == 0) rights &= ~0x08;
  }

  // Rook captured on corner => lose that side's right
  if (capturedPiece == 'R') {
    if (toRow == 7 && toCol == 7) rights &= ~0x01;
    if (toRow == 7 && toCol == 0) rights &= ~0x02;
  } else if (capturedPiece == 'r') {
    if (toRow == 0 && toCol == 7) rights &= ~0x04;
    if (toRow == 0 && toCol == 0) rights &= ~0x08;
  }

  chessEngine->setCastlingRights(rights);
}

void ChessGame::applyCastling(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece) {
  int deltaCol = kingToCol - kingFromCol;
  if (kingFromRow != kingToRow) return;
  if (deltaCol != 2 && deltaCol != -2) return;

  int rookFromCol = (deltaCol == 2) ? 7 : 0;
  int rookToCol = (deltaCol == 2) ? 5 : 3;
  char rookPiece = (kingPiece >= 'a' && kingPiece <= 'z') ? 'r' : 'R';

  // Update board state
  board[kingToRow][rookToCol] = rookPiece;
  board[kingToRow][rookFromCol] = ' ';

  // Handle LED prompts and wait for rook move
  Serial.printf("Castling: please move rook from %c%d to %c%d\n", (char)('a' + rookFromCol), 8 - kingToRow, (char)('a' + rookToCol), 8 - kingToRow);

  // Wait for rook to be lifted from its original square
  boardDriver->clearAllLEDs();
  boardDriver->setSquareLED(kingToRow, rookFromCol, LedColors::Cyan.r, LedColors::Cyan.g, LedColors::Cyan.b);
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::White.r, LedColors::White.g, LedColors::White.b);
  boardDriver->showLEDs();

  while (boardDriver->getSensorState(kingToRow, rookFromCol)) {
    boardDriver->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  // Wait for rook to be placed on destination square
  boardDriver->clearAllLEDs();
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::White.r, LedColors::White.g, LedColors::White.b);
  boardDriver->showLEDs();

  while (!boardDriver->getSensorState(kingToRow, rookToCol)) {
    boardDriver->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  boardDriver->clearAllLEDs();
}

char ChessGame::applyEnPassant(int toRow, int toCol, char piece) {
  int capturedPawnRow = ChessUtils::getEnPassantCapturedPawnRow(toRow, piece);
  char capturedPawn = board[capturedPawnRow][toCol];
  board[capturedPawnRow][toCol] = ' ';
  return capturedPawn;
}

void ChessGame::updateEnPassantTarget(int fromRow, int fromCol, int toRow, char piece) {
  if (toupper(piece) == 'P' && abs(toRow - fromRow) == 2) {
    int enPassantRow = (fromRow + toRow) / 2;
    chessEngine->setEnPassantTarget(enPassantRow, fromCol);
  } else {
    chessEngine->clearEnPassantTarget();
  }
}

bool ChessGame::applyPawnPromotionIfNeeded(int toRow, int toCol, char movedPiece, char& promotedPieceOut) {
  if (!chessEngine->isPawnPromotion(movedPiece, toRow))
    return false;

  promotedPieceOut = chessEngine->getPromotedPiece(movedPiece);
  board[toRow][toCol] = promotedPieceOut;
  return true;
}

void ChessGame::confirmSquareCompletion(int row, int col) {
  boardDriver->blinkSquare(row, col, LedColors::Green.r, LedColors::Green.g, LedColors::Green.b, 2);
}
