#include "board.h"

#include <cctype>
#include <cmath>
#include <cstring>

#include "codec.h"
#include "rules.h"
#include "zobrist_keys.h"

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
      gameResult_(RESULT_IN_PROGRESS),
      winnerColor_(' '),
      cachedEval_(0.0f),
      fenDirty_(true),
      evalDirty_(true),
      batchDepth_(0),
      batchDirty_(false),
      positionHistoryCount_(0) {
  memcpy(board_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessBoard::newGame() {
  memcpy(board_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
  currentTurn_ = 'w';
  gameOver_ = false;
  gameResult_ = RESULT_IN_PROGRESS;
  winnerColor_ = ' ';
  state_ = PositionState{};
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
  fireCallback();
}

void ChessBoard::loadFEN(const std::string& fen) {
  ChessUtils::fenToBoard(fen, board_, currentTurn_, &state_);
  positionHistoryCount_ = 0;
  recordPosition();
  gameOver_ = false;
  gameResult_ = RESULT_IN_PROGRESS;
  winnerColor_ = ' ';
  invalidateCache();
  fireCallback();
}

void ChessBoard::endGame(GameResult result, char winnerColor) {
  gameOver_ = true;
  gameResult_ = result;
  winnerColor_ = winnerColor;
  invalidateCache();
  fireCallback();
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

  // Advance turn and detect end conditions
  advanceTurn();
  char winner = ' ';
  GameResult endResult = detectGameEnd(winner);
  result.gameResult = endResult;
  result.winnerColor = winner;
  if (endResult != RESULT_IN_PROGRESS) {
    gameOver_ = true;
    gameResult_ = endResult;
    winnerColor_ = winner;
  }

  // Check detection (if game is not over)
  result.isCheck = false;
  if (endResult == RESULT_IN_PROGRESS && ChessRules::isKingInCheck(board_, currentTurn_))
    result.isCheck = true;

  invalidateCache();
  fireCallback();
  return result;
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

void ChessBoard::invalidateCache() {
  fenDirty_ = true;
  evalDirty_ = true;
}

// ---------------------------------------------------------------------------
// Batching
// ---------------------------------------------------------------------------

void ChessBoard::beginBatch() {
  ++batchDepth_;
}

void ChessBoard::endBatch() {
  if (batchDepth_ > 0) --batchDepth_;
  if (batchDepth_ == 0 && batchDirty_) {
    batchDirty_ = false;
    if (stateCallback_) stateCallback_();
  }
}

// ---------------------------------------------------------------------------
// Internal: apply move to board
// ---------------------------------------------------------------------------

void ChessBoard::applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result) {
  char piece = board_[fromRow][fromCol];
  char capturedPiece = board_[toRow][toCol];

  result.isCastling = ChessUtils::isCastlingMove(fromRow, fromCol, toRow, toCol, piece);
  result.isEnPassant = ChessUtils::isEnPassantMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  result.epCapturedRow = -1;
  result.isPromotion = false;
  result.promotedTo = ' ';

  // En passant target for next move
  if (toupper(piece) == 'P' && abs(toRow - fromRow) == 2) {
    state_.epRow = (fromRow + toRow) / 2;
    state_.epCol = fromCol;
  } else {
    state_.epRow = -1;
    state_.epCol = -1;
  }

  // Handle en passant capture
  if (result.isEnPassant) {
    int epCapturedRow = ChessUtils::getEnPassantCapturedPawnRow(toRow, piece);
    result.epCapturedRow = epCapturedRow;
    capturedPiece = board_[epCapturedRow][toCol];
    board_[epCapturedRow][toCol] = ' ';
  }

  result.isCapture = (capturedPiece != ' ');

  // Update halfmove clock (reset on pawn move or capture, otherwise increment)
  if (toupper(piece) == 'P' || capturedPiece != ' ')
    state_.halfmoveClock = 0;
  else
    state_.halfmoveClock++;

  // Move the piece
  board_[toRow][toCol] = piece;
  board_[fromRow][fromCol] = ' ';

  // Apply castling rook move
  if (result.isCastling)
    applyCastling(fromRow, fromCol, toRow, toCol, piece);

  // Update castling rights
  updateCastlingRightsAfterMove(fromRow, fromCol, toRow, toCol, piece, capturedPiece);

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
// Internal: castling rook movement (board-only, no hardware)
// ---------------------------------------------------------------------------

void ChessBoard::applyCastling(int fromRow, int fromCol, int toRow, int toCol, char kingPiece) {
  int deltaCol = toCol - fromCol;
  if (fromRow != toRow) return;
  if (deltaCol != 2 && deltaCol != -2) return;

  int rookFromCol = (deltaCol == 2) ? 7 : 0;
  int rookToCol = (deltaCol == 2) ? 5 : 3;
  char rookPiece = ChessUtils::isBlackPiece(kingPiece) ? 'r' : 'R';

  board_[toRow][rookToCol] = rookPiece;
  board_[toRow][rookFromCol] = ' ';
}

// ---------------------------------------------------------------------------
// Internal: castling rights bookkeeping
// ---------------------------------------------------------------------------

void ChessBoard::updateCastlingRightsAfterMove(int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece) {
  uint8_t rights = state_.castlingRights;

  // King moved — lose both rights for that color
  if (movedPiece == 'K')
    rights &= ~(0x01 | 0x02);
  else if (movedPiece == 'k')
    rights &= ~(0x04 | 0x08);

  // Rook moved from corner — lose that side's right
  if (movedPiece == 'R') {
    if (fromRow == 7 && fromCol == 7) rights &= ~0x01;
    if (fromRow == 7 && fromCol == 0) rights &= ~0x02;
  } else if (movedPiece == 'r') {
    if (fromRow == 0 && fromCol == 7) rights &= ~0x04;
    if (fromRow == 0 && fromCol == 0) rights &= ~0x08;
  }

  // Rook captured on corner — lose that side's right
  if (capturedPiece == 'R') {
    if (toRow == 7 && toCol == 7) rights &= ~0x01;
    if (toRow == 7 && toCol == 0) rights &= ~0x02;
  } else if (capturedPiece == 'r') {
    if (toRow == 0 && toCol == 7) rights &= ~0x04;
    if (toRow == 0 && toCol == 0) rights &= ~0x08;
  }

  state_.castlingRights = rights;
}

// ---------------------------------------------------------------------------
// Internal: turn advancement + position recording
// ---------------------------------------------------------------------------

void ChessBoard::advanceTurn() {
  if (currentTurn_ == 'b')
    state_.fullmoveClock++;
  currentTurn_ = (currentTurn_ == 'w') ? 'b' : 'w';
  recordPosition();
}

// ---------------------------------------------------------------------------
// Internal: detect checkmate / stalemate / draw conditions
// ---------------------------------------------------------------------------

GameResult ChessBoard::detectGameEnd(char& winner) {
  if (ChessRules::isCheckmate(board_, currentTurn_, state_)) {
    winner = (currentTurn_ == 'w') ? 'b' : 'w';
    return RESULT_CHECKMATE;
  }
  if (ChessRules::isStalemate(board_, currentTurn_, state_)) {
    winner = 'd';
    return RESULT_STALEMATE;
  }
  if (state_.halfmoveClock >= 100) {
    winner = 'd';
    return RESULT_DRAW_50;
  }
  if (isThreefoldRepetition()) {
    winner = 'd';
    return RESULT_DRAW_3FOLD;
  }
  winner = ' ';
  return RESULT_IN_PROGRESS;
}

// ---------------------------------------------------------------------------
// Internal: callback dispatch
// ---------------------------------------------------------------------------

void ChessBoard::fireCallback() {
  if (batchDepth_ > 0) {
    batchDirty_ = true;
    return;
  }
  if (stateCallback_) stateCallback_();
}

// ---------------------------------------------------------------------------
// Internal: position history (Zobrist hash-based threefold repetition)
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

  // https://lichess.org/forum/general-chess-discussion/3-fold-repetition-and-castling-rights-sarana-martinez
  // Hash castling rights (unlike en-passant, castling rights always matter
  // for repetition and are only lost AFTER a rook or king moves)
  hash ^= ZOBRIST_CASTLING[state_.castlingRights];

  // Hash en passant file only if a legal en passant capture exists.
  // Per FIDE rules, the en passant square only matters for repetition if an
  // opposing pawn can actually make the capture (including not leaving the
  // king in check).
  if (ChessRules::hasLegalEnPassantCapture(board_, currentTurn_, state_)) {
    hash ^= ZOBRIST_EN_PASSANT[state_.epCol];
  }

  // Hash side to move
  if (currentTurn_ == 'b')
    hash ^= ZOBRIST_SIDE_TO_MOVE;

  return hash;
}

void ChessBoard::recordPosition() {
  // Clear history on irreversible moves (pawn move or capture reset
  // halfmoveClock to 0). Positions from before an irreversible move can
  // never recur, so this is safe and keeps memory usage bounded by the
  // 50-move rule (~100 entries max).
  if (state_.halfmoveClock == 0)
    positionHistoryCount_ = 0;

  if (positionHistoryCount_ < MAX_POSITION_HISTORY)
    positionHistory_[positionHistoryCount_++] = computeZobristHash();
}

bool ChessBoard::isThreefoldRepetition() const {
  // Minimum 5 half-moves for 3 occurrences of same position
  if (positionHistoryCount_ < 5)
    return false;

  uint64_t current = positionHistory_[positionHistoryCount_ - 1];
  int count = 1;  // Current position counts as 1
  // Scan backwards, skipping every other entry (only same side-to-move can
  // match). Backwards scan finds recent repetitions faster for early exit.
  for (int i = positionHistoryCount_ - 3; i >= 0; i -= 2) {
    if (positionHistory_[i] == current) {
      count++;
      if (count >= 3)
        return true;
    }
  }
  return false;
}
