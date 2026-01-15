#include "chess_moves.h"
#include "chess_common.h"
#include "chess_utils.h"
#include "led_colors.h"
#include <Arduino.h>

const char ChessMoves::INITIAL_BOARD[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'}, // row 0 = rank 8 (black pieces, top row)
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'}, // row 1 = rank 7 (black pawns)
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 2 = rank 6
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 3 = rank 5
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 4 = rank 4
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}, // row 5 = rank 3
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'}, // row 6 = rank 2 (white pawns)
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}  // row 7 = rank 1 (White pieces, bottom row)
};

ChessMoves::ChessMoves(BoardDriver* bd, ChessEngine* ce) : boardDriver(bd), chessEngine(ce) {}

void ChessMoves::begin() {
  Serial.println("Starting Chess Game Mode...");
  initializeBoard();
  currentTurn = 'w'; // White starts
  waitForBoardSetup();
  Serial.println("Chess game ready to start!");
  boardDriver->fireworkAnimation();
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
}

void ChessMoves::update() {
  boardDriver->readSensors();

  static constexpr unsigned long kPlacementDebounceMs = 200; // require stable contact before accepting placement
  static constexpr unsigned long kPlacementPollDelayMs = 20;

  // Look for a piece pickup
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (boardDriver->getSensorPrev(row, col) && !boardDriver->getSensorState(row, col)) {
        char piece = board[row][col];

        // Skip empty squares
        if (piece == ' ')
          continue;

        // Check if it's the correct player's turn
        char pieceColor = (piece >= 'a' && piece <= 'z') ? 'b' : 'w';
        if (pieceColor != currentTurn) {
          Serial.printf("Wrong turn! It's %s's turn to move.\n", (currentTurn == 'w') ? "White" : "Black");

          // Flash the square red to indicate wrong turn
          boardDriver->blinkSquare(row, col, LedColors::ErrorRed.r, LedColors::ErrorRed.g, LedColors::ErrorRed.b, 2);

          continue; // Skip this piece
        }

        Serial.printf("Piece lifted from %c%d\n", (char)('a' + col), row + 1);

        // Generate possible moves
        chessEngine->setCastlingRights(castlingRights);
        int moveCount = 0;
        int moves[28][2]; // up to 28 moves (maximum for a queen)
        chessEngine->getPossibleMoves(board, row, col, moveCount, moves);

        // Light up current square and possible move squares
        boardDriver->setSquareLED(row, col, LedColors::PickupCyan.r, LedColors::PickupCyan.g, LedColors::PickupCyan.b);

        // Highlight possible move squares (including captures)
        for (int i = 0; i < moveCount; i++) {
          int r = moves[i][0];
          int c = moves[i][1];

          // Different highlighting for empty squares vs capture squares
          if (board[r][c] == ' ') {
            boardDriver->setSquareLED(r, c, LedColors::MoveWhite.r, LedColors::MoveWhite.g, LedColors::MoveWhite.b);
          } else {
            boardDriver->setSquareLED(r, c, LedColors::AttackRed.r, LedColors::AttackRed.g, LedColors::AttackRed.b);
          }
        }
        boardDriver->showLEDs();

        // Wait for piece placement - handle both normal moves and captures
        int targetRow = -1, targetCol = -1;
        bool piecePlaced = false;
        bool captureInProgress = false;

        int pendingRow = -1;
        int pendingCol = -1;
        unsigned long pendingSinceMs = 0;

        // Wait for a piece placement on any square
        while (!piecePlaced) {
          boardDriver->readSensors();

          // Debounce: if a candidate square is briefly touched (e.g., sliding a pawn across squares),
          // don’t commit the move until the sensor stays occupied for kPlacementDebounceMs.
          if (pendingRow != -1) {
            if (boardDriver->getSensorState(pendingRow, pendingCol)) {
              if (millis() - pendingSinceMs >= kPlacementDebounceMs) {
                targetRow = pendingRow;
                targetCol = pendingCol;
                piecePlaced = true;
                break;
              }
            } else {
              pendingRow = -1;
              pendingCol = -1;
              pendingSinceMs = 0;
            }
            delay(kPlacementPollDelayMs);
            continue;
          }

          // First check if the original piece was placed back
          if (boardDriver->getSensorState(row, col)) {
            pendingRow = row;
            pendingCol = col;
            pendingSinceMs = millis();
            delay(kPlacementPollDelayMs);
            continue;
          }

          // Then check all squares for a regular move or capture initiation
          for (int r2 = 0; r2 < 8; r2++) {
            for (int c2 = 0; c2 < 8; c2++) {
              // Skip the original square which was already checked
              if (r2 == row && c2 == col)
                continue;

              // Check if this would be a legal move
              bool isLegalMove = false;
              for (int i = 0; i < moveCount; i++) {
                if (moves[i][0] == r2 && moves[i][1] == c2) {
                  isLegalMove = true;
                  break;
                }
              }

              // If not a legal move, no need to check further
              if (!isLegalMove)
                continue;

              // For capture moves: detect when the target piece is removed
              if (board[r2][c2] != ' ' && !boardDriver->getSensorState(r2, c2) && boardDriver->getSensorPrev(r2, c2)) {
                Serial.printf("Capture initiated at %c%d\n", (char)('a' + c2), r2 + 1);

                // Store the target square and wait for the capturing piece to be placed there
                targetRow = r2;
                targetCol = c2;
                captureInProgress = true;

                // Flash the capture square to indicate waiting for piece placement
                boardDriver->setSquareLED(r2, c2, LedColors::AttackRed.r, LedColors::AttackRed.g, LedColors::AttackRed.b, 100);
                boardDriver->showLEDs();

                // Wait for the capturing piece to be placed
                bool capturePiecePlaced = false;
                unsigned long capturePlaceSinceMs = 0;
                while (!capturePiecePlaced) {
                  boardDriver->readSensors();
                  if (boardDriver->getSensorState(r2, c2)) {
                    if (capturePlaceSinceMs == 0) {
                      capturePlaceSinceMs = millis();
                    } else if (millis() - capturePlaceSinceMs >= kPlacementDebounceMs) {
                      capturePiecePlaced = true;
                      piecePlaced = true;
                      break;
                    }
                  } else {
                    capturePlaceSinceMs = 0;
                  }
                  delay(kPlacementPollDelayMs);
                }
                break;
              }

              // For normal non-capture moves: detect when a piece is placed on an empty square
              else if (board[r2][c2] == ' ' && boardDriver->getSensorState(r2, c2)) {
                pendingRow = r2;
                pendingCol = c2;
                pendingSinceMs = millis();
                break;
              }
            }
            if (piecePlaced || captureInProgress)
              break;
          }

          delay(kPlacementPollDelayMs);
        }

        // Check if piece is replaced in the original spot
        if (targetRow == row && targetCol == col) {
          Serial.println("Piece replaced in original spot");

          // Clear all LED effects
          boardDriver->clearAllLEDs();

          continue; // Skip to next iteration
        }

        // Check if move is legal
        bool legalMove = false;
        bool isCapture = false;
        for (int i = 0; i < moveCount; i++) {
          if (moves[i][0] == targetRow && moves[i][1] == targetCol) {
            legalMove = true;
            // Check if this is a capture move
            if (board[targetRow][targetCol] != ' ') {
              isCapture = true;
            }
            break;
          }
        }

        if (legalMove) {
          Serial.printf("Legal move to %c%d\n", (char)('a' + targetCol), targetRow + 1);

          // Play capture animation if needed
          if (board[targetRow][targetCol] != ' ') {
            Serial.println("Performing capture animation");
            boardDriver->captureAnimation();
          }

          // Process the move
          processMove(row, col, targetRow, targetCol, piece);

          // Check for pawn promotion
          checkForPromotion(targetRow, targetCol, piece);

          // Confirmation: Flash destination square green
          ChessCommon::confirmSquareCompletion(boardDriver, targetRow, targetCol);
        } else {
          Serial.println("Illegal move, reverting");
        }

        // Clear any remaining LED effects
        boardDriver->clearAllLEDs();
      }
    }
  }

  // Update previous sensor state
  boardDriver->updateSensorPrev();
}

void ChessMoves::initializeBoard() {
  ChessCommon::copyBoard(INITIAL_BOARD, board);
  castlingRights = 0x0F;
  chessEngine->setCastlingRights(castlingRights);
}

void ChessMoves::waitForBoardSetup() {
  Serial.println("Waiting for pieces to be placed...");
  while (!boardDriver->checkInitialBoard(INITIAL_BOARD)) {
    boardDriver->updateSetupDisplay(INITIAL_BOARD);
    delay(250);
  }
}

void ChessMoves::processMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
  char capturedPiece = board[toRow][toCol];
  bool isCastling = ChessCommon::isCastlingMove(fromRow, fromCol, toRow, toCol, piece);

  // Update board state
  board[toRow][toCol] = piece;
  board[fromRow][fromCol] = ' ';

  // If this was castling, also move the rook (and wait for physical rook move)
  if (isCastling)
    ChessCommon::applyCastlingRookInternal(boardDriver, board, fromRow, fromCol, toRow, toCol, piece);

  ChessCommon::updateCastlingRightsAfterMove(castlingRights, fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  chessEngine->setCastlingRights(castlingRights);

  // Switch turns
  currentTurn = (currentTurn == 'w') ? 'b' : 'w';

  // Check for check, checkmate, or stalemate
  (void)ChessCommon::handleGameState(boardDriver, chessEngine, board, currentTurn);
}

void ChessMoves::checkForPromotion(int targetRow, int targetCol, char piece) {
  char promotedPiece = ' ';
  if (ChessCommon::applyPawnPromotionIfNeeded(chessEngine, board, targetRow, targetCol, piece, promotedPiece)) {
    Serial.println(String(piece == 'P' ? "White" : "Black") + " pawn promoted to Queen at " + String(((char)('a' + targetCol))) + String(targetRow + 1));
    boardDriver->promotionAnimation(targetCol);
    handlePromotion(targetRow, targetCol, piece);
  }
}

void ChessMoves::handlePromotion(int targetRow, int targetCol, char piece) {
  Serial.println("Please replace the pawn with a queen piece");

  // First wait for the pawn to be removed
  while (boardDriver->getSensorState(targetRow, targetCol)) {
    // Blink the square to indicate action needed
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b, 50);
    boardDriver->showLEDs();
    delay(250);
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b, 0);
    boardDriver->showLEDs();
    delay(250);

    // Read sensors
    boardDriver->readSensors();
  }

  Serial.println("Pawn removed, please place a queen");

  // Then wait for the queen to be placed
  while (!boardDriver->getSensorState(targetRow, targetCol)) {
    // Blink the square to indicate action needed
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b, 50);
    boardDriver->showLEDs();
    delay(250);
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b, 0);
    boardDriver->showLEDs();
    delay(250);

    // Read sensors
    boardDriver->readSensors();
  }

  Serial.println("Queen placed, promotion complete");

  // Final confirmation blink
  for (int i = 0; i < 3; i++) {
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b, 50);
    boardDriver->showLEDs();
    delay(100);
    boardDriver->setSquareLED(targetRow, targetCol, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b, 0);
    boardDriver->showLEDs();
    delay(100);
  }
}

bool ChessMoves::isActive() {
  return true; // Simple implementation for now
}

void ChessMoves::getBoardState(char boardState[8][8]) {
  ChessCommon::copyBoard(board, boardState);
}

void ChessMoves::setBoardState(char newBoardState[8][8]) {
  Serial.println("Board state updated via WiFi edit");
  ChessCommon::copyBoard(newBoardState, board);
  castlingRights = ChessCommon::recomputeCastlingRightsFromBoard(board);
  chessEngine->setCastlingRights(castlingRights);
  // Update sensor previous state to match new board
  boardDriver->readSensors();
  boardDriver->updateSensorPrev();
  ChessUtils::printBoard(board);
}