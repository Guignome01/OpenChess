#include "chess_rules.h"
#include "chess_iterator.h"
#include "chess_utils.h"
#include <cstring>

// ---------------------------
// ChessRules Implementation (stateless)
// ---------------------------

bool ChessRules::hasLegalEnPassantCapture(const Piece board[8][8], Color sideToMove, const PositionState& flags) {
  if (flags.epRow < 0 || flags.epCol < 0) return false;
  int capturerRow = flags.epRow - ChessPiece::pawnDirection(sideToMove);
  Piece capturerPawn = ChessPiece::makePiece(sideToMove, PieceType::PAWN);
  if (flags.epCol > 0 && board[capturerRow][flags.epCol - 1] == capturerPawn && !leavesInCheck(board, capturerRow, flags.epCol - 1, flags.epRow, flags.epCol, flags))
    return true;
  if (flags.epCol < 7 && board[capturerRow][flags.epCol + 1] == capturerPawn && !leavesInCheck(board, capturerRow, flags.epCol + 1, flags.epRow, flags.epCol, flags))
    return true;
  return false;
}

// Generate pseudo-legal moves (without check filtering)
void ChessRules::getPseudoLegalMoves(const Piece board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling) {
  moveCount = 0;
  Piece piece = board[row][col];

  if (piece == Piece::NONE)
    return;

  Color pieceColor = ChessPiece::pieceColor(piece);

  switch (ChessPiece::pieceType(piece)) {
    case PieceType::PAWN:
      addPawnMoves(board, row, col, pieceColor, flags, moveCount, moves);
      break;
    case PieceType::ROOK:
      addRookMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case PieceType::KNIGHT:
      addKnightMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case PieceType::BISHOP:
      addBishopMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case PieceType::QUEEN:
      addQueenMoves(board, row, col, pieceColor, moveCount, moves);
      break;
    case PieceType::KING:
      addKingMoves(board, row, col, pieceColor, flags, moveCount, moves, includeCastling);
      break;
    default:
      break;
  }
}

// Main move generation function (returns only legal moves)
void ChessRules::getPossibleMoves(const Piece board[8][8], int row, int col, const PositionState& flags, int& moveCount, int moves[][2]) {
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
void ChessRules::addPawnMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& flags, int& moveCount, int moves[][2]) {
  int direction = ChessPiece::pawnDirection(pieceColor);

  // One square forward
  if (ChessUtils::isValidSquare(row + direction, col) && isSquareEmpty(board, row + direction, col)) {
    moves[moveCount][0] = row + direction;
    moves[moveCount][1] = col;
    moveCount++;

    // Initial two-square move
    if (row == ChessPiece::homeRow(pieceColor) + direction)
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

    if (ChessUtils::isValidSquare(captureRow, captureCol) &&
        isSquareOccupiedByOpponent(board, captureRow, captureCol, pieceColor)) {
      moves[moveCount][0] = captureRow;
      moves[moveCount][1] = captureCol;
      moveCount++;
    }
  }

  // En passant captures
  if (flags.epRow >= 0 && flags.epCol >= 0 && row == flags.epRow - direction) {
    for (int i = 0; i < 2; i++) {
      int captureCol = captureColumns[i];
      if (captureCol == flags.epCol) {
        moves[moveCount][0] = flags.epRow;
        moves[moveCount][1] = flags.epCol;
        moveCount++;
      }
    }
  }
}

// Sliding piece move generation (shared by rook, bishop, queen)
void ChessRules::addSlidingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const int directions[][2], int dirCount, int& moveCount, int moves[][2]) {
  for (int d = 0; d < dirCount; d++)
    for (int step = 1; step < 8; step++) {
      int newRow = row + step * directions[d][0];
      int newCol = col + step * directions[d][1];

      if (!ChessUtils::isValidSquare(newRow, newCol))
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
void ChessRules::addRookMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]) {
  static const int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moveCount, moves);
}

// Knight move generation
void ChessRules::addKnightMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]) {
  int knightMoves[8][2] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + knightMoves[i][0];
    int newCol = col + knightMoves[i][1];

    if (ChessUtils::isValidSquare(newRow, newCol))
      if (isSquareEmpty(board, newRow, newCol) ||
          isSquareOccupiedByOpponent(board, newRow, newCol, pieceColor)) {
        moves[moveCount][0] = newRow;
        moves[moveCount][1] = newCol;
        moveCount++;
      }
  }
}

// Bishop move generation
void ChessRules::addBishopMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]) {
  static const int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moveCount, moves);
}

// Queen move generation (combination of rook and bishop)
void ChessRules::addQueenMoves(const Piece board[8][8], int row, int col, Color pieceColor, int& moveCount, int moves[][2]) {
  addRookMoves(board, row, col, pieceColor, moveCount, moves);
  addBishopMoves(board, row, col, pieceColor, moveCount, moves);
}

// King move generation
void ChessRules::addKingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& flags, int& moveCount, int moves[][2], bool includeCastling) {
  int kingMoves[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + kingMoves[i][0];
    int newCol = col + kingMoves[i][1];

    if (ChessUtils::isValidSquare(newRow, newCol))
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

void ChessRules::addCastlingMoves(const Piece board[8][8], int row, int col, Color pieceColor, uint8_t castlingRights, int& moveCount, int moves[][2]) {
  int homeRow = ChessPiece::homeRow(pieceColor);
  Piece kingPiece = ChessPiece::makePiece(pieceColor, PieceType::KING);
  Piece rookPiece = ChessPiece::makePiece(pieceColor, PieceType::ROOK);

  if (row != homeRow || col != 4) return;
  if (board[row][col] != kingPiece) return;

  // King cannot castle while in check.
  if (isSquareUnderAttack(board, row, col, pieceColor)) return;

  // King-side castling (e -> g)
  if (ChessUtils::hasCastlingRight(castlingRights, pieceColor, true))
    if (board[homeRow][5] == Piece::NONE && board[homeRow][6] == Piece::NONE && board[homeRow][7] == rookPiece)
      if (!isSquareUnderAttack(board, homeRow, 5, pieceColor) && !isSquareUnderAttack(board, homeRow, 6, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 6;
        moveCount++;
      }

  // Queen-side castling (e -> c)
  if (ChessUtils::hasCastlingRight(castlingRights, pieceColor, false))
    if (board[homeRow][3] == Piece::NONE && board[homeRow][2] == Piece::NONE && board[homeRow][1] == Piece::NONE && board[homeRow][0] == rookPiece)
      if (!isSquareUnderAttack(board, homeRow, 3, pieceColor) && !isSquareUnderAttack(board, homeRow, 2, pieceColor)) {
        moves[moveCount][0] = homeRow;
        moves[moveCount][1] = 2;
        moveCount++;
      }
}

// Helper functions
bool ChessRules::isSquareOccupiedByOpponent(const Piece board[8][8], int row, int col, Color pieceColor) {
  Piece target = board[row][col];
  if (target == Piece::NONE) return false;
  return ChessPiece::pieceColor(target) != pieceColor;
}

bool ChessRules::isSquareEmpty(const Piece board[8][8], int row, int col) {
  return board[row][col] == Piece::NONE;
}

// Move validation
bool ChessRules::isValidMove(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags) {
  int moveCount = 0;
  int moves[MAX_MOVES_PER_PIECE][2];

  getPossibleMoves(board, fromRow, fromCol, flags, moveCount, moves);

  for (int i = 0; i < moveCount; i++)
    if (moves[i][0] == toRow && moves[i][1] == toCol)
      return true;

  return false;
}

// Check if a pawn move results in promotion
bool ChessRules::isPromotion(Piece piece, int targetRow) {
  if (ChessPiece::pieceType(piece) != PieceType::PAWN) return false;
  return targetRow == ChessPiece::promotionRow(ChessPiece::pieceColor(piece));
}

// ---------------------------
// Check Detection Functions
// ---------------------------

bool ChessRules::isSquareUnderAttack(const Piece board[8][8], int row, int col, Color defendingColor) {
  Color attackingColor = ~defendingColor;

  // Empty flags — attack detection doesn't need castling or en passant.
  PositionState emptyFlags{0, -1, -1, 0, 0};

  return ChessIterator::somePiece(board, [&](int r, int c, Piece piece) {
    if (ChessPiece::pieceColor(piece) != attackingColor) return false;

    // Pawns are special: their attack pattern differs from their move pattern.
    if (ChessPiece::pieceType(piece) == PieceType::PAWN) {
      int direction = ChessPiece::pawnDirection(attackingColor);
      return (r + direction == row && (c - 1 == col || c + 1 == col));
    }

    int moveCount = 0;
    int moves[MAX_MOVES_PER_PIECE][2];
    // IMPORTANT: for attack detection, do NOT include castling moves
    getPseudoLegalMoves(board, r, c, emptyFlags, moveCount, moves, false);

    for (int i = 0; i < moveCount; i++)
      if (moves[i][0] == row && moves[i][1] == col)
        return true;
    return false;
  });
}

bool ChessRules::leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& flags) {
  Piece testBoard[8][8];
  memcpy(testBoard, board, sizeof(testBoard));

  Piece movingPiece = testBoard[fromRow][fromCol];
  Color movingColor = ChessPiece::pieceColor(movingPiece);

  Piece capturedPiece;
  ChessUtils::applyBoardTransform(testBoard, fromRow, fromCol, toRow, toCol, flags, capturedPiece);

  Piece king = ChessPiece::makePiece(movingColor, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(testBoard, king, kingPos, 1) == 0)
    return true;

  return isSquareUnderAttack(testBoard, kingPos[0][0], kingPos[0][1], movingColor);
}

bool ChessRules::isCheck(const Piece board[8][8], Color kingColor) {
  Piece king = ChessPiece::makePiece(kingColor, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(board, king, kingPos, 1) == 0)
    return false;

  return isSquareUnderAttack(board, kingPos[0][0], kingPos[0][1], kingColor);
}

bool ChessRules::hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& flags) {
  return ChessIterator::somePiece(board, [&](int r, int c, Piece piece) {
    if (ChessPiece::pieceColor(piece) != color) return false;
    int moveCount = 0;
    int moves[MAX_MOVES_PER_PIECE][2];
    getPossibleMoves(board, r, c, flags, moveCount, moves);
    return moveCount > 0;
  });
}

bool ChessRules::isCheckmate(const Piece board[8][8], Color kingColor, const PositionState& flags) {
  return isCheck(board, kingColor) && !hasAnyLegalMove(board, kingColor, flags);
}

bool ChessRules::isStalemate(const Piece board[8][8], Color colorToMove, const PositionState& flags) {
  return !isCheck(board, colorToMove) && !hasAnyLegalMove(board, colorToMove, flags);
}

bool ChessRules::isInsufficientMaterial(const Piece board[8][8]) {
  int whiteMinorCount = 0, blackMinorCount = 0;
  int whiteBishopSquareColor = -1, blackBishopSquareColor = -1;

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      Piece piece = board[r][c];
      if (piece == Piece::NONE) continue;

      PieceType type = ChessPiece::pieceType(piece);
      if (type == PieceType::KING) continue;

      // Any pawn, rook, or queen means sufficient material
      if (type == PieceType::PAWN || type == PieceType::ROOK || type == PieceType::QUEEN)
        return false;

      int squareColor = (r + c) % 2;
      if (ChessPiece::isWhite(piece)) {
        whiteMinorCount++;
        if (type == PieceType::BISHOP) whiteBishopSquareColor = squareColor;
      } else {
        blackMinorCount++;
        if (type == PieceType::BISHOP) blackBishopSquareColor = squareColor;
      }

      // Two minor pieces on the same side is sufficient (K+B+N, K+N+N, etc.)
      if (whiteMinorCount > 1 || blackMinorCount > 1) return false;
    }
  }

  // K vs K
  if (whiteMinorCount == 0 && blackMinorCount == 0) return true;

  // K+B vs K  or  K+N vs K
  if ((whiteMinorCount == 1 && blackMinorCount == 0) ||
      (whiteMinorCount == 0 && blackMinorCount == 1))
    return true;

  // K+B vs K+B with same-color bishops
  if (whiteMinorCount == 1 && blackMinorCount == 1 &&
      whiteBishopSquareColor >= 0 && blackBishopSquareColor >= 0 &&
      whiteBishopSquareColor == blackBishopSquareColor)
    return true;

  return false;
}

bool ChessRules::isThreefoldRepetition(const uint64_t* positionHistory, int positionHistoryCount) {
  if (positionHistoryCount < 5) return false;

  uint64_t current = positionHistory[positionHistoryCount - 1];
  int count = 1;

  for (int i = positionHistoryCount - 3; i >= 0; i -= 2) {
    if (positionHistory[i] == current) {
      count++;
      if (count >= 3) return true;
    }
  }
  return false;
}

bool ChessRules::isFiftyMoveRule(const PositionState& state) {
  return state.halfmoveClock >= 100;
}

bool ChessRules::isDraw(const Piece board[8][8], Color colorToMove, const PositionState& state,
                        const uint64_t* positionHistory, int positionHistoryCount) {
  return isStalemate(board, colorToMove, state)
      || isFiftyMoveRule(state)
      || isInsufficientMaterial(board)
      || isThreefoldRepetition(positionHistory, positionHistoryCount);
}

GameResult ChessRules::isGameOver(const Piece board[8][8], Color colorToMove, const PositionState& state,
                                  const uint64_t* positionHistory, int positionHistoryCount, char& winner) {
  if (isCheckmate(board, colorToMove, state)) {
    winner = ChessPiece::colorToChar(~colorToMove);
    return GameResult::CHECKMATE;
  }
  if (isStalemate(board, colorToMove, state)) {
    winner = 'd';
    return GameResult::STALEMATE;
  }
  if (isFiftyMoveRule(state)) {
    winner = 'd';
    return GameResult::DRAW_50;
  }
  if (isInsufficientMaterial(board)) {
    winner = 'd';
    return GameResult::DRAW_INSUFFICIENT;
  }
  if (isThreefoldRepetition(positionHistory, positionHistoryCount)) {
    winner = 'd';
    return GameResult::DRAW_3FOLD;
  }
  winner = ' ';
  return GameResult::IN_PROGRESS;
}
