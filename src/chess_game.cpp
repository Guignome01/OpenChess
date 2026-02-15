#include "chess_game.h"
#include "chess_utils.h"
#include "move_history.h"
#include "wifi_manager_esp32.h"
#include <string.h>

const char ChessGame::INITIAL_BOARD[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, // row 0 = rank 8 (Black pieces, top row)
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // row 1 = rank 7 (Black pawns)
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 2 = rank 6
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 3 = rank 5
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 4 = rank 4
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 5 = rank 3
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, // row 6 = rank 2 (White pawns)
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  // row 7 = rank 1 (White pieces, bottom row)
};

ChessGame::ChessGame(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, MoveHistory* mh) : boardDriver(bd), chessEngine(ce), wifiManager(wm), moveHistory(mh), currentTurn('w'), gameOver(false), replaying(false) {}

void ChessGame::initializeBoard() {
  currentTurn = 'w';
  gameOver = false;
  memcpy(board, INITIAL_BOARD, sizeof(INITIAL_BOARD));
  chessEngine->reset();
  chessEngine->recordPosition(board, currentTurn);
  wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
}

void ChessGame::waitForBoardSetup(const char targetBoard[8][8]) {
  Serial.println("Set up the board in the required position...");

  boardDriver->acquireLEDs();
  boardDriver->clearAllLEDs(false);
  bool allCorrect = false;
  while (!allCorrect) {
    boardDriver->readSensors();
    allCorrect = true;

    // Check every square
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        bool shouldHavePiece = (targetBoard[row][col] != ' ');
        bool hasPiece = boardDriver->getSensorState(row, col);
        if (shouldHavePiece != hasPiece) {
          allCorrect = false;
          break;
        }
      }
      if (!allCorrect)
        break;
    }

    // Update LED display to show required setup
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        bool shouldHavePiece = (targetBoard[row][col] != ' ');
        bool hasPiece = boardDriver->getSensorState(row, col);

        if (shouldHavePiece && !hasPiece) {
          // Need to place a piece here - show where pieces should go
          if (ChessUtils::isWhitePiece(targetBoard[row][col]))
            boardDriver->setSquareLED(row, col, ChessUtils::colorLed('w'));
          else
            boardDriver->setSquareLED(row, col, ChessUtils::colorLed('b'));
        } else if (!shouldHavePiece && hasPiece) {
          // Need to remove a piece from here - show in red
          boardDriver->setSquareLED(row, col, LedColors::Red);
        } else {
          // Correct state - no LED
          boardDriver->setSquareLED(row, col, LedColors::Off);
        }
      }
    }
    boardDriver->showLEDs();

    delay(SENSOR_READ_DELAY_MS);
  }
  boardDriver->releaseLEDs();

  Serial.println("Board setup complete! Game starting...");
  boardDriver->fireworkAnimation();
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
}

void ChessGame::applyMove(int fromRow, int fromCol, int toRow, int toCol, char promotion, bool isRemoteMove) {
  char piece = board[fromRow][fromCol];
  char capturedPiece = board[toRow][toCol];

  bool isCastling = ChessUtils::isCastlingMove(fromRow, fromCol, toRow, toCol, piece);
  bool isEnPassantCapture = ChessUtils::isEnPassantMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  int enPassantCapturedPawnRow = ChessUtils::getEnPassantCapturedPawnRow(toRow, piece);
  if (toupper(piece) == 'P' && abs(toRow - fromRow) == 2) {
    int enPassantRow = (fromRow + toRow) / 2;
    chessEngine->setEnPassantTarget(enPassantRow, fromCol);
  } else {
    chessEngine->clearEnPassantTarget();
  }
  if (isEnPassantCapture) {
    capturedPiece = board[enPassantCapturedPawnRow][toCol];
    board[enPassantCapturedPawnRow][toCol] = ' ';
  }

  chessEngine->updateHalfmoveClock(piece, capturedPiece);

  board[toRow][toCol] = piece;
  board[fromRow][fromCol] = ' ';

  Serial.printf("%s %s: %c %c%d -> %c%d\n", isRemoteMove ? "Remote" : "Player", isCastling ? "castling" : (isEnPassantCapture ? "en passant" : (capturedPiece != ' ' ? "capture" : "move")), piece, (char)('a' + fromCol), 8 - fromRow, (char)('a' + toCol), 8 - toRow);

  if (isRemoteMove && !isCastling && !replaying)
    waitForRemoteMoveCompletion(fromRow, fromCol, toRow, toCol, capturedPiece != ' ', isEnPassantCapture, enPassantCapturedPawnRow);

  if (isCastling)
    applyCastling(fromRow, fromCol, toRow, toCol, piece, isRemoteMove);

  updateCastlingRightsAfterMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece);

  if (capturedPiece != ' ') {
    if (!replaying) boardDriver->captureAnimation(toRow, toCol);
  } else {
    if (!replaying) confirmSquareCompletion(toRow, toCol);
  }

  if (chessEngine->isPawnPromotion(piece, toRow)) {
    promotion = promotion != ' ' && promotion != '\0' ? (ChessUtils::isWhitePiece(piece) ? toupper(promotion) : tolower(promotion)) : (ChessUtils::isWhitePiece(piece) ? 'Q' : 'q');
    board[toRow][toCol] = promotion;
    Serial.printf("Pawn promoted to %c\n", promotion);
    if (!replaying) boardDriver->promotionAnimation(toCol);
  }

  if (moveHistory && moveHistory->isRecording())
    moveHistory->addMove(fromRow, fromCol, toRow, toCol, promotion);
}

bool ChessGame::tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol) {
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
        boardDriver->blinkSquare(row, col, LedColors::Red, 2);
        continue;
      }

      Serial.printf("Piece pickup from %c%d\n", (char)('a' + col), 8 - row);

      // Generate possible moves
      int moveCount = 0;
      int moves[28][2];
      chessEngine->getPossibleMoves(board, row, col, moveCount, moves);

      // Light up current square and possible move squares
      boardDriver->setSquareLED(row, col, LedColors::Cyan);

      // Highlight possible move squares (different colors for empty vs capture)
      for (int i = 0; i < moveCount; i++) {
        int r = moves[i][0];
        int c = moves[i][1];

        bool isEnPassantCapture = ChessUtils::isEnPassantMove(row, col, r, c, piece, board[r][c]);
        if (board[r][c] == ' ' && !isEnPassantCapture) {
          boardDriver->setSquareLED(r, c, LedColors::White);
        } else {
          boardDriver->setSquareLED(r, c, LedColors::Red);
          if (isEnPassantCapture)
            boardDriver->setSquareLED(ChessUtils::getEnPassantCapturedPawnRow(r, piece), c, LedColors::Purple);
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
                boardDriver->setSquareLED(enPassantCapturedPawnRow, c2, LedColors::Off);
              // Blink the capture square to indicate waiting for piece placement
              boardDriver->blinkSquare(r2, c2, LedColors::Red, 1, false);
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

      boardDriver->clearAllLEDs();
      return true;
    }

  return false;
}

void ChessGame::advanceTurn() {
  chessEngine->incrementFullmoveClock(currentTurn);
  currentTurn = (currentTurn == 'w') ? 'b' : 'w';
  chessEngine->recordPosition(board, currentTurn);
}

void ChessGame::updateGameStatus() {
  advanceTurn();

  if (chessEngine->isCheckmate(board, currentTurn)) {
    char winnerColor = (currentTurn == 'w') ? 'b' : 'w';
    Serial.printf("CHECKMATE! %s wins!\n", ChessUtils::colorName(winnerColor));
    boardDriver->fireworkAnimation(ChessUtils::colorLed(winnerColor));
    gameOver = true;
    if (moveHistory) moveHistory->finishGame(RESULT_CHECKMATE, winnerColor);
    return;
  }

  if (chessEngine->isStalemate(board, currentTurn)) {
    Serial.println("STALEMATE! Game is a draw.");
    boardDriver->fireworkAnimation(LedColors::Cyan);
    gameOver = true;
    if (moveHistory) moveHistory->finishGame(RESULT_STALEMATE, 'd');
    return;
  }

  if (chessEngine->isFiftyMoveRule()) {
    Serial.println("DRAW by 50-move rule! No captures or pawn moves in the last 50 moves.");
    boardDriver->fireworkAnimation(LedColors::Cyan);
    gameOver = true;
    if (moveHistory) moveHistory->finishGame(RESULT_DRAW_50, 'd');
    return;
  }

  if (chessEngine->isThreefoldRepetition()) {
    Serial.println("DRAW by threefold repetition! Same position occurred 3 times.");
    boardDriver->fireworkAnimation(LedColors::Cyan);
    gameOver = true;
    if (moveHistory) moveHistory->finishGame(RESULT_DRAW_3FOLD, 'd');
    return;
  }

  if (chessEngine->isKingInCheck(board, currentTurn)) {
    Serial.printf("%s is in CHECK!\n", ChessUtils::colorName(currentTurn));
    boardDriver->clearAllLEDs(false);

    int kingRow = -1;
    int kingCol = -1;
    if (chessEngine->findKingPosition(board, currentTurn, kingRow, kingCol))
      boardDriver->blinkSquare(kingRow, kingCol, LedColors::Yellow);
  }

  Serial.printf("It's %s's turn !\n", ChessUtils::colorName(currentTurn));
}

void ChessGame::setBoardStateFromFEN(const String& fen) {
  ChessUtils::fenToBoard(fen, board, currentTurn, chessEngine);
  chessEngine->recordPosition(board, currentTurn);
  if (moveHistory && moveHistory->isRecording())
    moveHistory->addFen(fen);
  wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
  Serial.println("Board state set from FEN: " + fen);
  ChessUtils::printBoard(board);
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

void ChessGame::applyCastling(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece, bool waitForKingCompletion) {
  int deltaCol = kingToCol - kingFromCol;
  if (kingFromRow != kingToRow) return;
  if (deltaCol != 2 && deltaCol != -2) return;

  int rookFromCol = (deltaCol == 2) ? 7 : 0;
  int rookToCol = (deltaCol == 2) ? 5 : 3;
  char rookPiece = (kingPiece >= 'a' && kingPiece <= 'z') ? 'r' : 'R';

  // Update board state
  board[kingToRow][rookToCol] = rookPiece;
  board[kingToRow][rookFromCol] = ' ';

  // Skip all LED prompts and physical waits during replay
  if (replaying) return;

  if (waitForKingCompletion) {
    // Handle LED prompts and wait for king move
    Serial.printf("Castling: please move king from %c%d to %c%d\n", (char)('a' + kingFromCol), 8 - kingFromRow, (char)('a' + kingToCol), 8 - kingToRow);

    boardDriver->clearAllLEDs(false);
    boardDriver->setSquareLED(kingFromRow, kingFromCol, LedColors::Cyan);
    boardDriver->setSquareLED(kingToRow, kingToCol, LedColors::White);
    boardDriver->showLEDs();

    // Wait for king to be lifted from its original square
    while (boardDriver->getSensorState(kingFromRow, kingFromCol)) {
      boardDriver->readSensors();
      delay(SENSOR_READ_DELAY_MS);
    }

    // Wait for king to be placed on destination square
    boardDriver->clearAllLEDs(false);
    boardDriver->setSquareLED(kingToRow, kingToCol, LedColors::White);
    boardDriver->showLEDs();

    while (!boardDriver->getSensorState(kingToRow, kingToCol)) {
      boardDriver->readSensors();
      delay(SENSOR_READ_DELAY_MS);
    }

    boardDriver->clearAllLEDs();
  }

  // Handle LED prompts and wait for rook move
  Serial.printf("Castling: please move rook from %c%d to %c%d\n", (char)('a' + rookFromCol), 8 - kingToRow, (char)('a' + rookToCol), 8 - kingToRow);

  // Wait for rook to be lifted from its original square
  boardDriver->clearAllLEDs(false);
  boardDriver->setSquareLED(kingToRow, rookFromCol, LedColors::Cyan);
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::White);
  boardDriver->showLEDs();

  while (boardDriver->getSensorState(kingToRow, rookFromCol)) {
    boardDriver->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  // Wait for rook to be placed on destination square
  boardDriver->clearAllLEDs(false);
  boardDriver->setSquareLED(kingToRow, rookToCol, LedColors::White);
  boardDriver->showLEDs();

  while (!boardDriver->getSensorState(kingToRow, rookToCol)) {
    boardDriver->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  boardDriver->clearAllLEDs();
}

void ChessGame::confirmSquareCompletion(int row, int col) {
  boardDriver->blinkSquare(row, col, LedColors::Green, 1);
}