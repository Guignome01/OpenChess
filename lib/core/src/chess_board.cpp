#include "chess_board.h"

#include <cctype>
#include <cmath>
#include <cstring>

#include "chess_history.h"
#include "chess_rules.h"

const char ChessBoard::INITIAL_BOARD[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},  // row 0 = rank 8
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},  // row 1 = rank 7
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 2 = rank 6
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 3 = rank 5
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 4 = rank 4
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},    // row 5 = rank 3
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},  // row 6 = rank 2
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}   // row 7 = rank 1
};

ChessBoard::ChessBoard()
    : currentTurn_('w'),
      gameOver_(false),
      gameResult_(GameResult::IN_PROGRESS),
      winnerColor_(' '),
      positionHistoryCount_(0),
      cachedEval_(0.0f),
      fenDirty_(true),
      evalDirty_(true) {
  memcpy(board_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessBoard::newGame() {
  memcpy(board_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
  currentTurn_ = 'w';
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';
  state_ = PositionState::initial();
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
}

bool ChessBoard::loadFEN(const std::string& fen) {
  // --- Basic FEN validation ---
  if (fen.empty()) return false;

  // Extract board part (before first space)
  size_t spacePos = fen.find(' ');
  std::string boardPart = (spacePos != std::string::npos) ? fen.substr(0, spacePos) : fen;

  // Must have exactly 7 '/' separators (8 ranks)
  int slashCount = 0;
  for (char c : boardPart) {
    if (c == '/') slashCount++;
  }
  if (slashCount != 7) return false;

  // Validate each rank sums to 8 and contains only valid characters
  int col = 0;
  for (char c : boardPart) {
    if (c == '/') {
      if (col != 8) return false;  // rank didn't sum to 8
      col = 0;
    } else if (c >= '1' && c <= '8') {
      col += c - '0';
    } else if (std::strchr("rnbqkpRNBQKP", c)) {
      col++;
    } else {
      return false;  // invalid character
    }
  }
  if (col != 8) return false;  // last rank didn't sum to 8

  // Validate turn field if present
  if (spacePos != std::string::npos && spacePos + 1 < fen.size()) {
    size_t turnEnd = fen.find(' ', spacePos + 1);
    std::string turnField = fen.substr(spacePos + 1, turnEnd - spacePos - 1);
    if (turnField != "w" && turnField != "b") return false;
  }

  // --- Validated: apply FEN ---
  ChessUtils::fenToBoard(fen, board_, currentTurn_, &state_);
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
  return true;
}

void ChessBoard::endGame(GameResult result, char winnerColor) {
  if (gameOver_) return;  // Guard against double-call
  gameOver_ = true;
  gameResult_ = result;
  winnerColor_ = winnerColor;
  invalidateCache();
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

MoveResult ChessBoard::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (gameOver_) return invalidMoveResult();

  // Validate coordinates
  if (fromRow < 0 || fromRow > 7 || fromCol < 0 || fromCol > 7 ||
      toRow < 0 || toRow > 7 || toCol < 0 || toCol > 7)
    return invalidMoveResult();

  char piece = board_[fromRow][fromCol];
  if (piece == ' ') return invalidMoveResult();

  // Validate turn
  if (ChessUtils::getPieceColor(piece) != currentTurn_) return invalidMoveResult();

  // Validate move legality via rules
  if (!ChessRules::isValidMove(board_, fromRow, fromCol, toRow, toCol, state_)) return invalidMoveResult();

  // Build result and apply
  MoveResult result;
  result.valid = true;
  applyMoveToBoard(fromRow, fromCol, toRow, toCol, promotion, result);

  // Advance turn and record position for repetition detection
  advanceTurn();
  recordPosition();
  char winner = ' ';
  GameResult endResult = detectGameEnd(winner);
  result.gameResult = endResult;
  result.winnerColor = winner;
  if (endResult != GameResult::IN_PROGRESS) {
    gameOver_ = true;
    gameResult_ = endResult;
    winnerColor_ = winner;
  }

  // Check detection (if game is not over)
  result.isCheck = false;
  if (endResult == GameResult::IN_PROGRESS && ChessRules::isKingInCheck(board_, currentTurn_))
    result.isCheck = true;

  invalidateCache();
  return result;
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

void ChessBoard::reverseMove(const MoveEntry& entry) {
  // Restore the piece to its original square (before promotion)
  board_[entry.fromRow][entry.fromCol] = entry.piece;

  // Clear destination
  board_[entry.toRow][entry.toCol] = ' ';

  // Restore captured piece
  if (entry.isEnPassant) {
    // En passant: captured pawn was on epCapturedRow, same col as destination
    board_[entry.epCapturedRow][entry.toCol] = entry.captured;
  } else if (entry.isCapture) {
    board_[entry.toRow][entry.toCol] = entry.captured;
  }

  // Reverse castling rook move
  if (entry.isCastling) {
    auto castle = ChessUtils::checkCastling(entry.fromRow, entry.fromCol,
                                             entry.toRow, entry.toCol, entry.piece);
    if (castle.isCastling) {
      // Move rook back: it's currently at rookToCol, restore to rookFromCol
      char rook = board_[entry.toRow][castle.rookToCol];
      board_[entry.toRow][castle.rookFromCol] = rook;
      board_[entry.toRow][castle.rookToCol] = ' ';
    }
  }

  // Restore position state from before the move
  state_ = entry.prevState;

  // Switch turn back
  currentTurn_ = ChessUtils::getPieceColor(entry.piece);

  // Pop last Zobrist position
  if (positionHistoryCount_ > 0)
    positionHistoryCount_--;

  // Clear game-over state (undoing means game is back in play)
  gameOver_ = false;
  gameResult_ = GameResult::IN_PROGRESS;
  winnerColor_ = ' ';

  invalidateCache();
}

MoveResult ChessBoard::applyMoveEntry(const MoveEntry& entry) {
  return makeMove(entry.fromRow, entry.fromCol, entry.toRow, entry.toCol,
                  entry.isPromotion ? entry.promotion : ' ');
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::string ChessBoard::getFen() const {
  if (fenDirty_) {
    cachedFen_ = ChessUtils::boardToFEN(board_, currentTurn_, &state_);
    fenDirty_ = false;
  }
  return cachedFen_;
}

float ChessBoard::getEvaluation() const {
  if (evalDirty_) {
    cachedEval_ = ChessUtils::evaluatePosition(board_);
    evalDirty_ = false;
  }
  return cachedEval_;
}

int ChessBoard::findPiece(char type, char color, int positions[][2], int maxPositions) const {
  int count = 0;
  char target = (color == 'w') ? toupper(type) : tolower(type);
  for (int r = 0; r < 8 && count < maxPositions; r++) {
    for (int c = 0; c < 8 && count < maxPositions; c++) {
      if (board_[r][c] == target) {
        positions[count][0] = r;
        positions[count][1] = c;
        count++;
      }
    }
  }
  return count;
}

bool ChessBoard::findKingPosition(char kingColor, int& kingRow, int& kingCol) const {
  int positions[1][2];
  int count = findPiece('K', kingColor, positions, 1);
  if (count > 0) {
    kingRow = positions[0][0];
    kingCol = positions[0][1];
    return true;
  }
  return false;
}

bool ChessBoard::isInsufficientMaterial() const {
  return hasInsufficientMaterialInternal();
}

bool ChessBoard::isThreefoldRepetition() const {
  // Minimum 5 half-moves for 3 occurrences of the same position
  if (positionHistoryCount_ < 5) return false;

  uint64_t current = positionHistory_[positionHistoryCount_ - 1];
  int count = 1;  // current position counts as 1

  // Scan backwards, skipping every other entry (only same side-to-move
  // can match). Backwards scan finds recent repetitions faster.
  for (int i = positionHistoryCount_ - 3; i >= 0; i -= 2) {
    if (positionHistory_[i] == current) {
      count++;
      if (count >= 3) return true;
    }
  }
  return false;
}

void ChessBoard::invalidateCache() {
  fenDirty_ = true;
  evalDirty_ = true;
}

// ---------------------------------------------------------------------------
// Internal: apply move to board
// ---------------------------------------------------------------------------

void ChessBoard::applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result) {
  char piece = board_[fromRow][fromCol];
  char capturedPiece = board_[toRow][toCol];

  // --- En passant analysis ---
  auto ep = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  result.isEnPassant = ep.isCapture;
  result.epCapturedRow = ep.capturedPawnRow;
  result.isPromotion = false;
  result.promotedTo = ' ';

  state_.epRow = ep.nextEpRow;
  state_.epCol = ep.nextEpCol;

  if (ep.isCapture) {
    capturedPiece = board_[ep.capturedPawnRow][toCol];
    board_[ep.capturedPawnRow][toCol] = ' ';
  }

  result.isCapture = (capturedPiece != ' ');

  // --- Castling analysis ---
  auto castle = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, piece);
  result.isCastling = castle.isCastling;

  // Update halfmove clock (reset on pawn move or capture, otherwise increment)
  if (toupper(piece) == 'P' || capturedPiece != ' ')
    state_.halfmoveClock = 0;
  else
    state_.halfmoveClock++;

  // Move the piece
  board_[toRow][toCol] = piece;
  board_[fromRow][fromCol] = ' ';

  // Apply castling rook move
  if (castle.isCastling) {
    char rookPiece = ChessUtils::isBlackPiece(piece) ? 'r' : 'R';
    board_[toRow][castle.rookToCol] = rookPiece;
    board_[toRow][castle.rookFromCol] = ' ';
  }

  // Update castling rights
  state_.castlingRights = ChessUtils::updateCastlingRights(
      state_.castlingRights, fromRow, fromCol, toRow, toCol, piece, capturedPiece);

  // Pawn promotion
  if (ChessRules::isPawnPromotion(piece, toRow)) {
    result.isPromotion = true;
    if (promotion != ' ' && promotion != '\0') {
      result.promotedTo = ChessUtils::isWhitePiece(piece) ? toupper(promotion) : tolower(promotion);
    } else {
      // Auto-queen
      result.promotedTo = ChessUtils::isWhitePiece(piece) ? 'Q' : 'q';
    }
    board_[toRow][toCol] = result.promotedTo;
  }
}



// ---------------------------------------------------------------------------
// Internal: turn advancement + position recording
// ---------------------------------------------------------------------------

void ChessBoard::advanceTurn() {
  if (currentTurn_ == 'b')
    state_.fullmoveClock++;
  currentTurn_ = ChessUtils::opponentColor(currentTurn_);
}

// ---------------------------------------------------------------------------
// Internal: insufficient material detection (FIDE dead position)
// ---------------------------------------------------------------------------

bool ChessBoard::hasInsufficientMaterialInternal() const {
  int whiteMinorCount = 0, blackMinorCount = 0;
  int whiteBishopSquareColor = -1, blackBishopSquareColor = -1;

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      char piece = board_[r][c];
      if (piece == ' ' || piece == 'K' || piece == 'k') continue;

      // Any pawn, rook, or queen means sufficient material
      if (piece == 'P' || piece == 'p' ||
          piece == 'R' || piece == 'r' ||
          piece == 'Q' || piece == 'q')
        return false;

      int squareColor = (r + c) % 2;
      if (piece == 'N' || piece == 'B') {
        whiteMinorCount++;
        if (piece == 'B') whiteBishopSquareColor = squareColor;
      } else { // 'n' or 'b'
        blackMinorCount++;
        if (piece == 'b') blackBishopSquareColor = squareColor;
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

// ---------------------------------------------------------------------------
// Internal: detect checkmate / stalemate / draw conditions
// ---------------------------------------------------------------------------

GameResult ChessBoard::detectGameEnd(char& winner) {
  if (ChessRules::isCheckmate(board_, currentTurn_, state_)) {
    winner = ChessUtils::opponentColor(currentTurn_);
    return GameResult::CHECKMATE;
  }
  if (ChessRules::isStalemate(board_, currentTurn_, state_)) {
    winner = 'd';
    return GameResult::STALEMATE;
  }
  if (state_.halfmoveClock >= 100) {
    winner = 'd';
    return GameResult::DRAW_50;
  }
  if (hasInsufficientMaterialInternal()) {
    winner = 'd';
    return GameResult::DRAW_INSUFFICIENT;
  }
  if (isThreefoldRepetition()) {
    winner = 'd';
    return GameResult::DRAW_3FOLD;
  }
  winner = ' ';
  return GameResult::IN_PROGRESS;
}

// ---------------------------------------------------------------------------
// Internal: Zobrist hashing (for threefold repetition detection)
// ---------------------------------------------------------------------------

uint64_t ChessBoard::computeZobristHash() const {
  uint64_t hash = 0;

  // Hash piece positions
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      char piece = board_[row][col];
      if (piece != ' ') {
        int idx = pieceToZobristIndex(piece);
        hash ^= ZOBRIST_TABLE[idx][row * 8 + col];
      }
    }

  // Hash castling rights
  hash ^= ZOBRIST_CASTLING[state_.castlingRights];

  // Hash en passant file only if a legal en passant capture exists
  if (ChessRules::hasLegalEnPassantCapture(board_, currentTurn_, state_)) {
    hash ^= ZOBRIST_EN_PASSANT[state_.epCol];
  }

  // Hash side to move
  if (currentTurn_ == 'b')
    hash ^= ZOBRIST_SIDE_TO_MOVE;

  return hash;
}

void ChessBoard::recordPosition() {
  if (state_.halfmoveClock == 0 && positionHistoryCount_ > 0)
    positionHistoryCount_ = 0;

  if (positionHistoryCount_ < MAX_POSITION_HISTORY) {
    positionHistory_[positionHistoryCount_++] = computeZobristHash();
  }
}
