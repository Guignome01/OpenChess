#include "chess_rules.h"
#include "chess_iterator.h"
#include "chess_utils.h"
#include <cstring>

// ---------------------------
// ChessRules Implementation (stateless)
// ---------------------------

bool ChessRules::hasLegalEnPassantCapture(const Piece board[8][8], Color sideToMove, const PositionState& state) {
  if (state.epRow < 0 || state.epCol < 0) return false;
  int capturerRow = state.epRow - ChessPiece::pawnDirection(sideToMove);
  Piece capturerPawn = ChessPiece::makePiece(sideToMove, PieceType::PAWN);
  if (state.epCol > 0 && board[capturerRow][state.epCol - 1] == capturerPawn && !leavesInCheck(board, capturerRow, state.epCol - 1, state.epRow, state.epCol, state))
    return true;
  if (state.epCol < 7 && board[capturerRow][state.epCol + 1] == capturerPawn && !leavesInCheck(board, capturerRow, state.epCol + 1, state.epRow, state.epCol, state))
    return true;
  return false;
}

// Generate pseudo-legal moves (without check filtering)
void ChessRules::getPseudoLegalMoves(const Piece board[8][8], int row, int col, const PositionState& state, MoveList& moves, bool includeCastling) {
  moves.clear();
  Piece piece = board[row][col];

  if (piece == Piece::NONE)
    return;

  Color pieceColor = ChessPiece::pieceColor(piece);

  switch (ChessPiece::pieceType(piece)) {
    case PieceType::PAWN:
      addPawnMoves(board, row, col, pieceColor, state, moves);
      break;
    case PieceType::ROOK:
      addRookMoves(board, row, col, pieceColor, moves);
      break;
    case PieceType::KNIGHT:
      addKnightMoves(board, row, col, pieceColor, moves);
      break;
    case PieceType::BISHOP:
      addBishopMoves(board, row, col, pieceColor, moves);
      break;
    case PieceType::QUEEN:
      addQueenMoves(board, row, col, pieceColor, moves);
      break;
    case PieceType::KING:
      addKingMoves(board, row, col, pieceColor, state, moves, includeCastling);
      break;
    default:
      break;
  }
}

// Main move generation function (returns only legal moves)
void ChessRules::getPossibleMoves(const Piece board[8][8], int row, int col, const PositionState& state, MoveList& moves) {
  MoveList pseudoMoves;

  getPseudoLegalMoves(board, row, col, state, pseudoMoves, true);

  // Find king once for all leavesInCheck calls
  Piece piece = board[row][col];
  bool isKing = ChessPiece::pieceType(piece) == PieceType::KING;
  int kr, kc;
  if (isKing) {
    kr = row;
    kc = col;
  } else {
    Color color = ChessPiece::pieceColor(piece);
    Piece king = ChessPiece::makePiece(color, PieceType::KING);
    int kingPos[1][2];
    if (ChessIterator::findPiece(board, king, kingPos, 1) == 0) {
      moves.clear();
      return;
    }
    kr = kingPos[0][0];
    kc = kingPos[0][1];
  }

  moves.clear();
  for (int i = 0; i < pseudoMoves.count; i++) {
    int toRow = pseudoMoves.row(i);
    int toCol = pseudoMoves.col(i);
    // For king moves, pass destination as king position
    int checkRow = isKing ? toRow : kr;
    int checkCol = isKing ? toCol : kc;

    if (!leavesInCheck(board, row, col, toRow, toCol, state, checkRow, checkCol)) {
      moves.add(toRow, toCol);
    }
  }
}

// Pawn move generation
void ChessRules::addPawnMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& state, MoveList& moves) {
  int direction = ChessPiece::pawnDirection(pieceColor);

  // One square forward
  if (ChessUtils::isValidSquare(row + direction, col) && isSquareEmpty(board, row + direction, col)) {
    moves.add(row + direction, col);

    // Initial two-square move
    if (row == ChessPiece::homeRow(pieceColor) + direction)
      if (isSquareEmpty(board, row + 2 * direction, col)) {
        moves.add(row + 2 * direction, col);
      }
  }

  // Diagonal captures
  int captureColumns[] = {col - 1, col + 1};
  for (int i = 0; i < 2; i++) {
    int captureRow = row + direction;
    int captureCol = captureColumns[i];

    if (ChessUtils::isValidSquare(captureRow, captureCol) &&
        isSquareOccupiedByOpponent(board, captureRow, captureCol, pieceColor)) {
      moves.add(captureRow, captureCol);
    }
  }

  // En passant captures
  if (state.epRow >= 0 && state.epCol >= 0 && row == state.epRow - direction) {
    for (int i = 0; i < 2; i++) {
      int captureCol = captureColumns[i];
      if (captureCol == state.epCol) {
        moves.add(state.epRow, state.epCol);
      }
    }
  }
}

// Sliding piece move generation (shared by rook, bishop, queen)
void ChessRules::addSlidingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const int directions[][2], int dirCount, MoveList& moves) {
  for (int d = 0; d < dirCount; d++)
    for (int step = 1; step < 8; step++) {
      int newRow = row + step * directions[d][0];
      int newCol = col + step * directions[d][1];

      if ((unsigned)newRow >= 8 || (unsigned)newCol >= 8)
        break;

      Piece target = board[newRow][newCol];
      if (target == Piece::NONE) {
        moves.add(newRow, newCol);
      } else {
        if (ChessPiece::pieceColor(target) != pieceColor) {
          moves.add(newRow, newCol);
        }
        break;
      }
    }
}

// Rook move generation
void ChessRules::addRookMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves) {
  static const int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moves);
}

// Knight move generation
void ChessRules::addKnightMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves) {
  static constexpr int knightMoves[8][2] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + knightMoves[i][0];
    int newCol = col + knightMoves[i][1];

    if ((unsigned)newRow < 8 && (unsigned)newCol < 8) {
      Piece target = board[newRow][newCol];
      if (target == Piece::NONE || ChessPiece::pieceColor(target) != pieceColor) {
        moves.add(newRow, newCol);
      }
    }
  }
}

// Bishop move generation
void ChessRules::addBishopMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves) {
  static const int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  addSlidingMoves(board, row, col, pieceColor, directions, 4, moves);
}

// Queen move generation (combination of rook and bishop)
void ChessRules::addQueenMoves(const Piece board[8][8], int row, int col, Color pieceColor, MoveList& moves) {
  addRookMoves(board, row, col, pieceColor, moves);
  addBishopMoves(board, row, col, pieceColor, moves);
}

// King move generation
void ChessRules::addKingMoves(const Piece board[8][8], int row, int col, Color pieceColor, const PositionState& state, MoveList& moves, bool includeCastling) {
  static constexpr int kingMoves[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

  for (int i = 0; i < 8; i++) {
    int newRow = row + kingMoves[i][0];
    int newCol = col + kingMoves[i][1];

    if ((unsigned)newRow < 8 && (unsigned)newCol < 8) {
      Piece target = board[newRow][newCol];
      if (target == Piece::NONE || ChessPiece::pieceColor(target) != pieceColor) {
        moves.add(newRow, newCol);
      }
    }
  }

  if (includeCastling)
    addCastlingMoves(board, row, col, pieceColor, state.castlingRights, moves);
}

void ChessRules::addCastlingMoves(const Piece board[8][8], int row, int col, Color pieceColor, uint8_t castlingRights, MoveList& moves) {
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
        moves.add(homeRow, 6);
      }

  // Queen-side castling (e -> c)
  if (ChessUtils::hasCastlingRight(castlingRights, pieceColor, false))
    if (board[homeRow][3] == Piece::NONE && board[homeRow][2] == Piece::NONE && board[homeRow][1] == Piece::NONE && board[homeRow][0] == rookPiece)
      if (!isSquareUnderAttack(board, homeRow, 3, pieceColor) && !isSquareUnderAttack(board, homeRow, 2, pieceColor)) {
        moves.add(homeRow, 2);
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
bool ChessRules::isValidMove(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state) {
  MoveList pseudoMoves;

  getPseudoLegalMoves(board, fromRow, fromCol, state, pseudoMoves, true);

  for (int i = 0; i < pseudoMoves.count; i++)
    if (pseudoMoves.row(i) == toRow && pseudoMoves.col(i) == toCol)
      return !leavesInCheck(board, fromRow, fromCol, toRow, toCol, state);

  return false;
}

// ---------------------------
// Check Detection Functions
// ---------------------------

bool ChessRules::isSquareUnderAttack(const Piece board[8][8], int row, int col, Color defendingColor) {
  Color attackingColor = ~defendingColor;

  // Pawn attacks: check the two squares that an enemy pawn would attack FROM.
  // An enemy pawn on row-pawnDir, col±1 attacks (row,col).
  int pawnDir = ChessPiece::pawnDirection(attackingColor);
  int pawnRow = row - pawnDir;
  Piece enemyPawn = ChessPiece::makePiece(attackingColor, PieceType::PAWN);
  if ((unsigned)pawnRow < 8) {
    if ((unsigned)(col - 1) < 8 && board[pawnRow][col - 1] == enemyPawn) return true;
    if ((unsigned)(col + 1) < 8 && board[pawnRow][col + 1] == enemyPawn) return true;
  }

  // Knight attacks: check 8 L-shaped squares for enemy knight.
  static constexpr int knightOffsets[8][2] = {{2, 1}, {1, 2}, {-1, 2}, {-2, 1}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}};
  Piece enemyKnight = ChessPiece::makePiece(attackingColor, PieceType::KNIGHT);
  for (int i = 0; i < 8; i++) {
    int r = row + knightOffsets[i][0], c = col + knightOffsets[i][1];
    if ((unsigned)r < 8 && (unsigned)c < 8 && board[r][c] == enemyKnight) return true;
  }

  // King attacks: check 8 adjacent squares for enemy king.
  static constexpr int kingOffsets[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  Piece enemyKing = ChessPiece::makePiece(attackingColor, PieceType::KING);
  for (int i = 0; i < 8; i++) {
    int r = row + kingOffsets[i][0], c = col + kingOffsets[i][1];
    if ((unsigned)r < 8 && (unsigned)c < 8 && board[r][c] == enemyKing) return true;
  }

  // Sliding attacks: walk outward along 8 rays.
  // Orthogonal rays (indices 0-3) hit rook or queen.
  // Diagonal rays (indices 4-7) hit bishop or queen.
  static constexpr int rayDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  Piece enemyRook = ChessPiece::makePiece(attackingColor, PieceType::ROOK);
  Piece enemyBishop = ChessPiece::makePiece(attackingColor, PieceType::BISHOP);
  Piece enemyQueen = ChessPiece::makePiece(attackingColor, PieceType::QUEEN);

  for (int d = 0; d < 8; d++) {
    for (int step = 1; step < 8; step++) {
      int r = row + step * rayDirs[d][0], c = col + step * rayDirs[d][1];
      if ((unsigned)r >= 8 || (unsigned)c >= 8) break;
      Piece p = board[r][c];
      if (p == Piece::NONE) continue;
      // First piece along this ray — check if it's the right attacker type.
      if (p == enemyQueen) return true;
      if (d < 4 && p == enemyRook) return true;
      if (d >= 4 && p == enemyBishop) return true;
      break;  // Blocked by a non-attacking piece.
    }
  }

  return false;
}

bool ChessRules::leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state) {
  Piece movingPiece = board[fromRow][fromCol];
  bool isKingMove = ChessPiece::pieceType(movingPiece) == PieceType::KING;

  if (isKingMove) {
    return leavesInCheck(board, fromRow, fromCol, toRow, toCol, state, toRow, toCol);
  }

  // Non-king move: find the king once
  Color movingColor = ChessPiece::pieceColor(movingPiece);
  Piece king = ChessPiece::makePiece(movingColor, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(board, king, kingPos, 1) == 0)
    return true;
  return leavesInCheck(board, fromRow, fromCol, toRow, toCol, state, kingPos[0][0], kingPos[0][1]);
}

bool ChessRules::leavesInCheck(const Piece board[8][8], int fromRow, int fromCol, int toRow, int toCol, const PositionState& state, int kingRow, int kingCol) {
  Piece testBoard[8][8];
  memcpy(testBoard, board, sizeof(testBoard));

  Piece movingPiece = testBoard[fromRow][fromCol];
  Color movingColor = ChessPiece::pieceColor(movingPiece);
  Piece targetPiece = testBoard[toRow][toCol];

  auto ep = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, movingPiece, targetPiece);
  auto castle = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, movingPiece);

  Piece capturedPiece;
  ChessUtils::applyBoardTransform(testBoard, fromRow, fromCol, toRow, toCol, ep, castle, capturedPiece);

  // For king moves, kingRow/kingCol is already the destination.
  // For non-king moves, kingRow/kingCol is the pre-found king position (unchanged).
  return isSquareUnderAttack(testBoard, kingRow, kingCol, movingColor);
}

bool ChessRules::isCheck(const Piece board[8][8], Color kingColor) {
  Piece king = ChessPiece::makePiece(kingColor, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(board, king, kingPos, 1) == 0)
    return false;

  return isSquareUnderAttack(board, kingPos[0][0], kingPos[0][1], kingColor);
}

bool ChessRules::isCheck(const Piece board[8][8], Color kingColor, int kingRow, int kingCol) {
  return isSquareUnderAttack(board, kingRow, kingCol, kingColor);
}

bool ChessRules::hasLegalMove(const Piece board[8][8], int row, int col, const PositionState& state, int kingRow, int kingCol) {
  MoveList pseudoMoves;
  getPseudoLegalMoves(board, row, col, state, pseudoMoves, true);

  bool isKing = ChessPiece::pieceType(board[row][col]) == PieceType::KING;
  for (int i = 0; i < pseudoMoves.count; i++) {
    int toRow = pseudoMoves.row(i);
    int toCol = pseudoMoves.col(i);
    int checkRow = isKing ? toRow : kingRow;
    int checkCol = isKing ? toCol : kingCol;
    if (!leavesInCheck(board, row, col, toRow, toCol, state, checkRow, checkCol))
      return true;
  }
  return false;
}

bool ChessRules::hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& state) {
  // Find king once for all pieces
  Piece king = ChessPiece::makePiece(color, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(board, king, kingPos, 1) == 0)
    return false;
  return hasAnyLegalMove(board, color, state, kingPos[0][0], kingPos[0][1]);
}

bool ChessRules::hasAnyLegalMove(const Piece board[8][8], Color color, const PositionState& state, int kingRow, int kingCol) {
  return ChessIterator::somePiece(board, [&](int r, int c, Piece piece) {
    if (ChessPiece::pieceColor(piece) != color) return false;
    return hasLegalMove(board, r, c, state, kingRow, kingCol);
  });
}

bool ChessRules::isCheckmate(const Piece board[8][8], Color kingColor, const PositionState& state) {
  return isCheck(board, kingColor) && !hasAnyLegalMove(board, kingColor, state);
}

bool ChessRules::isStalemate(const Piece board[8][8], Color colorToMove, const PositionState& state) {
  return !isCheck(board, colorToMove) && !hasAnyLegalMove(board, colorToMove, state);
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

bool ChessRules::isThreefoldRepetition(const HashHistory& hashes) {
  if (hashes.count < 5) return false;

  uint64_t current = hashes.keys[hashes.count - 1];
  int count = 1;

  for (int i = hashes.count - 3; i >= 0; i -= 2) {
    if (hashes.keys[i] == current) {
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
                        const HashHistory& hashes) {
  return isStalemate(board, colorToMove, state)
      || isFiftyMoveRule(state)
      || isInsufficientMaterial(board)
      || isThreefoldRepetition(hashes);
}

GameResult ChessRules::isGameOver(const Piece board[8][8], Color colorToMove, const PositionState& state,
                                  const HashHistory& hashes, char& winner) {
  // Find king once — reuse for both check detection and legal move search
  Piece king = ChessPiece::makePiece(colorToMove, PieceType::KING);
  int kingPos[1][2];
  if (ChessIterator::findPiece(board, king, kingPos, 1) == 0) {
    winner = ' ';
    return GameResult::IN_PROGRESS;
  }
  int kr = kingPos[0][0], kc = kingPos[0][1];

  bool inCheck = isSquareUnderAttack(board, kr, kc, colorToMove);
  bool hasLegal = hasAnyLegalMove(board, colorToMove, state, kr, kc);

  if (!hasLegal) {
    if (inCheck) {
      winner = ChessPiece::colorToChar(~colorToMove);
      return GameResult::CHECKMATE;
    } else {
      winner = 'd';
      return GameResult::STALEMATE;
    }
  }
  if (isFiftyMoveRule(state)) {
    winner = 'd';
    return GameResult::DRAW_50;
  }
  if (isInsufficientMaterial(board)) {
    winner = 'd';
    return GameResult::DRAW_INSUFFICIENT;
  }
  if (isThreefoldRepetition(hashes)) {
    winner = 'd';
    return GameResult::DRAW_3FOLD;
  }
  winner = ' ';
  return GameResult::IN_PROGRESS;
}
