#include "game_mode.h"
#include "board_menu.h"
#include "chess_game.h"
#include "chess_utils.h"
#include "system_utils.h"
#include "wifi_manager_esp32.h"

// ---------------------------
// Brightness progression for resign gesture (25%, 50%, 75%, 100%)
// Level 0 = flag raised (mid-lift), levels 1-3 = each king return to square
// ---------------------------
static constexpr float RESIGN_BRIGHTNESS_LEVELS[] = {0.25f, 0.50f, 0.75f, 1.0f};

GameMode::GameMode(BoardDriver* bd, WiFiManagerESP32* wm, ChessGame* gc)
    : boardDriver_(bd), wifiManager_(wm), controller_(gc) {}

bool GameMode::isGameOver() const { return controller_->isGameOver(); }

bool GameMode::tryResumeGame() {
  if (controller_->hasActiveGame()) {
    Serial.println("Resuming live game...");
    return controller_->resumeGame();
  }
  return false;
}

void GameMode::waitForBoardSetup(const char targetBoard[8][8]) {
  Serial.println("Set up the board in the required position...");

  {
    BoardDriver::LedGuard guard(boardDriver_);
    boardDriver_->clearAllLEDs(false);
    bool allCorrect = false;
    while (!allCorrect) {
      boardDriver_->readSensors();
      allCorrect = true;

      // Check every square
      for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
          bool shouldHavePiece = (targetBoard[row][col] != ' ');
          bool hasPiece = boardDriver_->getSensorState(row, col);
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
          bool hasPiece = boardDriver_->getSensorState(row, col);

          if (shouldHavePiece && !hasPiece) {
            // Need to place a piece here - show where pieces should go
            if (ChessUtils::isWhitePiece(targetBoard[row][col]))
              boardDriver_->setSquareLED(row, col, SystemUtils::colorLed('w'));
            else
              boardDriver_->setSquareLED(row, col, SystemUtils::colorLed('b'));
          } else if (!shouldHavePiece && hasPiece) {
            // Need to remove a piece from here - show in red
            boardDriver_->setSquareLED(row, col, LedColors::Red);
          } else {
            // Correct state - no LED
            boardDriver_->setSquareLED(row, col, LedColors::Off);
          }
        }
      }
      boardDriver_->showLEDs();

      delay(SENSOR_READ_DELAY_MS);
    }
  } // LedGuard released

  Serial.println("Board setup complete! Game starting...");
  boardDriver_->fireworkAnimation();
  boardDriver_->readSensors();
  boardDriver_->updateSensorPrev();
}

MoveResult GameMode::applyMove(int fromRow, int fromCol, int toRow, int toCol, char promotion, bool isRemoteMove) {
  // Capture piece type before the move for logging
  char piece = controller_->getSquare(fromRow, fromCol);

  MoveResult result = controller_->makeMove(fromRow, fromCol, toRow, toCol, promotion);
  if (!result.valid) return result;

  // --- Serial logging ---
  const char* moveType = result.isCastling ? "castling"
                         : result.isEnPassant ? "en passant"
                         : result.isCapture   ? "capture"
                                              : "move";
  Serial.printf("%s %s: %c %s -> %s\n",
                isRemoteMove ? "Remote" : "Player", moveType,
                piece,
                ChessUtils::squareName(fromRow, fromCol).c_str(),
                ChessUtils::squareName(toRow, toCol).c_str());

  // --- Hardware feedback ---

  // Remote move: guide the player to physically execute the move on the board
  if (isRemoteMove && !result.isCastling)
    waitForRemoteMoveCompletion(fromRow, fromCol, toRow, toCol,
                                result.isCapture, result.isEnPassant,
                                result.epCapturedRow);

  // Castling: guide the player to move the rook
  if (result.isCastling)
    applyCastlingHardware(fromRow, fromCol, toRow, toCol, piece, isRemoteMove);

  // Capture / normal-move confirmation
  if (result.isCapture)
    boardDriver_->captureAnimation(toRow, toCol);
  else
    confirmSquareCompletion(toRow, toCol);

  // Promotion
  if (result.isPromotion) {
    Serial.printf("Pawn promoted to %c\n", result.promotedTo);
    boardDriver_->promotionAnimation(toCol);
  }

  // --- Game-end / check feedback ---
  if (result.gameResult == GameResult::CHECKMATE) {
    Serial.printf("CHECKMATE! %s wins!\n", ChessUtils::colorName(result.winnerColor));
    boardDriver_->fireworkAnimation(SystemUtils::colorLed(result.winnerColor));
  } else if (result.gameResult == GameResult::STALEMATE) {
    Serial.println("STALEMATE! Game is a draw.");
    boardDriver_->fireworkAnimation(LedColors::Cyan);
  } else if (result.gameResult == GameResult::DRAW_50) {
    Serial.println("DRAW by 50-move rule! No captures or pawn moves in the last 50 moves.");
    boardDriver_->fireworkAnimation(LedColors::Cyan);
  } else if (result.gameResult == GameResult::DRAW_3FOLD) {
    Serial.println("DRAW by threefold repetition! Same position occurred 3 times.");
    boardDriver_->fireworkAnimation(LedColors::Cyan);
  } else {
    // Game continues — check for check
    if (result.isCheck) {
      Serial.printf("%s is in CHECK!\n", ChessUtils::colorName(controller_->currentTurn()));
      int kingPos[1][2];
      if (controller_->findPiece('K', controller_->currentTurn(), kingPos, 1) > 0)
        boardDriver_->blinkSquare(kingPos[0][0], kingPos[0][1], LedColors::Yellow, 3, true, true);
    }
    Serial.printf("It's %s's turn !\n", ChessUtils::colorName(controller_->currentTurn()));
  }

  return result;
}

MoveResult GameMode::applyMove(const std::string& move) {
  int fromRow, fromCol, toRow, toCol;
  char promotion = ' ';
  if (!ChessGame::parseCoordinate(move, fromRow, fromCol, toRow, toCol, promotion)) {
    Serial.printf("Failed to parse move: %s\n", move.c_str());
    return invalidMoveResult();
  }
  return applyMove(fromRow, fromCol, toRow, toCol, promotion, true);
}

bool GameMode::tryPlayerMove(char playerColor, int& fromRow, int& fromCol, int& toRow, int& toCol) {
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      // Continue if nothing was picked up from this square
      if (!boardDriver_->getSensorPrev(row, col) || boardDriver_->getSensorState(row, col))
        continue;

      char piece = controller_->getSquare(row, col);

      // Skip empty squares
      if (piece == ' ')
        continue;

      // Check if it's the correct player's piece
      if (ChessUtils::getPieceColor(piece) != playerColor) {
        Serial.printf("Wrong turn! It's %s's turn to move.\n", ChessUtils::colorName(playerColor));
        showIllegalMoveFeedback(row, col);
        continue;
      }

      Serial.printf("Piece pickup from %c%d\n", (char)('a' + col), 8 - row);

      // Generate possible moves
      int moveCount = 0;
      int moves[MAX_MOVES_PER_PIECE][2];
      controller_->getPossibleMoves(row, col, moveCount, moves);

      // Drain any stale queued animations so highlights appear on a clean strip
      boardDriver_->waitForAnimationQueueDrain();

      // Light up current square and possible move squares
      {
        BoardDriver::LedGuard guard(boardDriver_);
        boardDriver_->setSquareLED(row, col, LedColors::Cyan);

      // Highlight possible move squares (different colors for empty vs capture)
      for (int i = 0; i < moveCount; i++) {
        int r = moves[i][0];
        int c = moves[i][1];

        auto ei = controller_->checkEnPassant(row, col, r, c);
        if (controller_->getSquare(r, c) == ' ' && !ei.isCapture) {
          boardDriver_->setSquareLED(r, c, LedColors::White);
        } else {
          boardDriver_->setSquareLED(r, c, LedColors::Red);
          if (ei.isCapture)
            boardDriver_->setSquareLED(ei.capturedPawnRow, c, LedColors::Purple);
        }
      }
      boardDriver_->showLEDs();
      } // LedGuard released

      // Wait for piece placement - handle both normal moves and captures
      int targetRow = -1, targetCol = -1;
      bool piecePlaced = false;
      bool isKing = (toupper(piece) == 'K');
      unsigned long liftTimestamp = millis();
      bool resignTransitioned = false; // True once 3s hold switches LEDs to dim orange
      unsigned long resignFlagTimestamp = 0; // When the resign flag was raised

      while (!piecePlaced) {
        boardDriver_->readSensors();

        // --- King resign hold detection ---
        // If a king is off its square for RESIGN_HOLD_MS, transition LEDs from
        // valid-move highlights to a dim orange on the king's origin square.
        if (isKing && !resignTransitioned && (millis() - liftTimestamp >= RESIGN_HOLD_MS)) {
          resignTransitioned = true;
          resignFlagTimestamp = millis();
          Serial.println("King held off square for 3s — resign gesture initiated");
          showResignProgress(row, col, 0);
        }

        // First check if the original piece was placed back
        if (boardDriver_->getSensorState(row, col)) {
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
            auto epInfo = controller_->checkEnPassant(row, col, r2, c2);
            int enPassantCapturedPawnRow = epInfo.capturedPawnRow;
            auto isCapturedPiecePickedUp = [&]() -> bool {
              if (epInfo.isCapture)
                return !boardDriver_->getSensorState(enPassantCapturedPawnRow, c2);
              else
                return !boardDriver_->getSensorState(r2, c2);
            };
            if ((controller_->getSquare(r2, c2) != ' ' || epInfo.isCapture) && isCapturedPiecePickedUp()) {
              Serial.printf("Capture initiated at %c%d\n", (char)('a' + c2), 8 - r2);
              // Store the target square and wait for the capturing piece to be placed there
              targetRow = r2;
              targetCol = c2;
              piecePlaced = true;
              if (epInfo.isCapture) {
                BoardDriver::LedGuard guard(boardDriver_);
                boardDriver_->setSquareLED(enPassantCapturedPawnRow, c2, LedColors::Off);
                boardDriver_->showLEDs();
              }
              // Blink the capture square to indicate waiting for piece placement
              boardDriver_->blinkSquare(r2, c2, LedColors::Red, 1, false);
              // Wait for the capturing piece to be placed (or returned to origin to cancel)
              while (!boardDriver_->getSensorState(r2, c2)) {
                boardDriver_->readSensors();
                // Allow cancellation by placing the piece back to its original position
                if (boardDriver_->getSensorState(row, col)) {
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
            if ((controller_->getSquare(r2, c2) == ' ' && !epInfo.isCapture) && boardDriver_->getSensorState(r2, c2)) {
              targetRow = r2;
              targetCol = c2;
              piecePlaced = true;
              break;
            }
          }
        }

        delay(SENSOR_READ_DELAY_MS);
      }

      // Clear highlights (single cleanup for all exit paths)
      // When resign was triggered and king returned, defer clear to showResignProgress to avoid flash.
      if (!(resignTransitioned && targetRow == row && targetCol == col)) {
        BoardDriver::LedGuard guard(boardDriver_);
        boardDriver_->clearAllLEDs();
      }

      if (targetRow == row && targetCol == col) {
        // King put back — check if the 3s resign hold was completed
        if (resignTransitioned) {
          // If king was returned too late, silently cancel
          if (millis() - resignFlagTimestamp > RESIGN_LIFT_WINDOW_MS) {
            BoardDriver::LedGuard guard(boardDriver_);
            boardDriver_->clearAllLEDs();
          } else {
            // First landing: brighten to 50%
            showResignProgress(row, col, 1, true);
            // Run remaining 2 quick lifts inline (blocking)
            continueResignGesture(row, col, ChessUtils::getPieceColor(piece));
          }
        } else {
          Serial.println("Pickup cancelled");
        }
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
        return false;
      }

      fromRow = row;
      fromCol = col;
      toRow = targetRow;
      toCol = targetCol;

      return true;
    }

  return false;
}

void GameMode::setBoardStateFromFEN(const std::string& fen) {
  controller_->loadFEN(fen);
  Serial.println("Board state set from FEN: " + String(fen.c_str()));
  SystemUtils::printBoard(controller_->getBoard());
}

// --- Hardware-only castling interactions ---
// Board already updated by ChessBoard — this only handles LED prompts and sensor waits.

void GameMode::applyCastlingHardware(int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece, bool waitForKingCompletion) {
  auto ci = ChessUtils::checkCastling(kingFromRow, kingFromCol, kingToRow, kingToCol, kingPiece);
  if (!ci.isCastling) return;

  BoardDriver::LedGuard guard(boardDriver_);

  if (waitForKingCompletion) {
    Serial.printf("Castling: please move king from %c%d to %c%d\n", (char)('a' + kingFromCol), 8 - kingFromRow, (char)('a' + kingToCol), 8 - kingToRow);

    boardDriver_->clearAllLEDs(false);
    boardDriver_->setSquareLED(kingFromRow, kingFromCol, LedColors::Cyan);
    boardDriver_->setSquareLED(kingToRow, kingToCol, LedColors::White);
    boardDriver_->showLEDs();

    while (boardDriver_->getSensorState(kingFromRow, kingFromCol)) {
      boardDriver_->readSensors();
      delay(SENSOR_READ_DELAY_MS);
    }

    boardDriver_->clearAllLEDs(false);
    boardDriver_->setSquareLED(kingToRow, kingToCol, LedColors::White);
    boardDriver_->showLEDs();

    while (!boardDriver_->getSensorState(kingToRow, kingToCol)) {
      boardDriver_->readSensors();
      delay(SENSOR_READ_DELAY_MS);
    }

    boardDriver_->clearAllLEDs();
  }

  Serial.printf("Castling: please move rook from %c%d to %c%d\n", (char)('a' + ci.rookFromCol), 8 - kingToRow, (char)('a' + ci.rookToCol), 8 - kingToRow);

  boardDriver_->clearAllLEDs(false);
  boardDriver_->setSquareLED(kingToRow, ci.rookFromCol, LedColors::Cyan);
  boardDriver_->setSquareLED(kingToRow, ci.rookToCol, LedColors::White);
  boardDriver_->showLEDs();

  while (boardDriver_->getSensorState(kingToRow, ci.rookFromCol)) {
    boardDriver_->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  boardDriver_->clearAllLEDs(false);
  boardDriver_->setSquareLED(kingToRow, ci.rookToCol, LedColors::White);
  boardDriver_->showLEDs();

  while (!boardDriver_->getSensorState(kingToRow, ci.rookToCol)) {
    boardDriver_->readSensors();
    delay(SENSOR_READ_DELAY_MS);
  }

  boardDriver_->clearAllLEDs();
} // LedGuard released

void GameMode::confirmSquareCompletion(int row, int col) {
  boardDriver_->blinkSquare(row, col, LedColors::Green, 1);
}

// ---------------------------
// Resign Feature
// ---------------------------

void GameMode::showIllegalMoveFeedback(int row, int col) {
  boardDriver_->blinkSquare(row, col, LedColors::Red, 2);
}

void GameMode::showResignProgress(int row, int col, int level, bool clearFirst) {
  BoardDriver::LedGuard guard(boardDriver_);
  if (clearFirst) boardDriver_->clearAllLEDs(false);
  boardDriver_->setSquareLED(row, col, LedColors::scaleColor(LedColors::Orange, RESIGN_BRIGHTNESS_LEVELS[level]));
  boardDriver_->showLEDs();
}

void GameMode::clearResignFeedback(int row, int col) {
  BoardDriver::LedGuard guard(boardDriver_);
  boardDriver_->setSquareLED(row, col, LedColors::Off);
  boardDriver_->showLEDs();
}

bool GameMode::processResign() {
  if (!resignPending_) return false;
  resignPending_ = false;
  handleResign(controller_->currentTurn());
  boardDriver_->updateSensorPrev();
  return true;
}

bool GameMode::continueResignGesture(int row, int col, char color) {
  // Called inline from tryPlayerMove after king returned (50% showing).
  // Need 2 more lift-and-return cycles. Each return brightens: 75% then 100%.

  for (int lift = 1; lift <= 2; lift++) {
    unsigned long waitStart = millis();
    bool lifted = false;
    while (millis() - waitStart < RESIGN_LIFT_WINDOW_MS) {
      boardDriver_->readSensors();
      if (!boardDriver_->getSensorState(row, col)) {
        lifted = true;
        break;
      }
      delay(SENSOR_READ_DELAY_MS);
    }

    if (!lifted) {
      clearResignFeedback(row, col);
      return false;
    }

    waitStart = millis();
    bool returned = false;
    while (millis() - waitStart < RESIGN_LIFT_WINDOW_MS) {
      boardDriver_->readSensors();
      if (boardDriver_->getSensorState(row, col)) {
        returned = true;
        break;
      }
      delay(SENSOR_READ_DELAY_MS);
    }

    if (!returned) {
      clearResignFeedback(row, col);
      return false;
    }

    showResignProgress(row, col, lift + 1);
  }

  Serial.printf("Resign gesture completed by %s\n", ChessUtils::colorName(color));
  delay(500);
  clearResignFeedback(row, col);
  return handleResign(color);
}

bool GameMode::handleResign(char resignColor) {
  onBeforeResignConfirm();

  bool flipped = isFlipped();
  Serial.printf("Resign confirmation for %s...\n", ChessUtils::colorName(resignColor));

  if (!boardConfirm(boardDriver_, flipped)) {
    Serial.println("Resign cancelled");
    onResignCancelled();
    return false;
  }

  onResignConfirmed(resignColor);

  char winnerColor = ChessUtils::opponentColor(resignColor);
  Serial.printf("RESIGNATION! %s resigns. %s wins!\n", ChessUtils::colorName(resignColor), ChessUtils::colorName(winnerColor));

  boardDriver_->fireworkAnimation(SystemUtils::colorLed(winnerColor));
  controller_->endGame(GameResult::RESIGNATION, winnerColor);
  return true;
}
