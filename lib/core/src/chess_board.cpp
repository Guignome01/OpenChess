#include "chess_board.h"

#include <cctype>
#include <cstring>

#include "chess_fen.h"
#include "chess_history.h"
#include "chess_iterator.h"
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
  state_ = PositionState::initial();
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
}

bool ChessBoard::loadFEN(const std::string& fen) {
  if (!ChessFEN::validateFEN(fen)) return false;

  ChessFEN::fenToBoard(fen, board_, currentTurn_, &state_);
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
  return true;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

MoveResult ChessBoard::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  // Validate coordinates
  if (!ChessUtils::isValidSquare(fromRow, fromCol) || !ChessUtils::isValidSquare(toRow, toCol))
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
  GameResult endResult = ChessRules::isGameOver(
      board_, currentTurn_, state_, positionHistory_, positionHistoryCount_, winner);
  result.gameResult = endResult;
  result.winnerColor = winner;

  // Check detection: checkmate implies check; otherwise test explicitly
  result.isCheck = false;
  if (endResult == GameResult::CHECKMATE) {
    result.isCheck = true;
  } else if (endResult == GameResult::IN_PROGRESS &&
             ChessRules::isCheck(board_, currentTurn_)) {
    result.isCheck = true;
  }

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
    cachedFen_ = ChessFEN::boardToFEN(board_, currentTurn_, &state_);
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
// Internal: apply move to board
// ---------------------------------------------------------------------------

void ChessBoard::applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result) {
  char piece = board_[fromRow][fromCol];
  char capturedPiece = board_[toRow][toCol];

  // --- En passant analysis (for state update + result) ---
  auto ep = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  result.isEnPassant = ep.isCapture;
  result.epCapturedRow = ep.capturedPawnRow;
  result.isPromotion = false;
  result.promotedTo = ' ';

  state_.epRow = ep.nextEpRow;
  state_.epCol = ep.nextEpCol;

  // --- Castling analysis (for result) ---
  auto castle = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, piece);
  result.isCastling = castle.isCastling;

  // Update halfmove clock (reset on pawn move or capture, otherwise increment)
  if (toupper(piece) == 'P' || capturedPiece != ' ' || ep.isCapture)
    state_.halfmoveClock = 0;
  else
    state_.halfmoveClock++;

  // --- Apply the physical board transform (piece move + castling rook + EP removal) ---
  char actualCapture;
  ChessUtils::applyBoardTransform(board_, fromRow, fromCol, toRow, toCol, state_, actualCapture);
  result.isCapture = (actualCapture != ' ');

  // Update castling rights
  state_.castlingRights = ChessUtils::updateCastlingRights(
      state_.castlingRights, fromRow, fromCol, toRow, toCol, piece, actualCapture);

  // Pawn promotion
  if (ChessRules::isPromotion(piece, toRow)) {
    result.isPromotion = true;
    if (promotion != ' ' && promotion != '\0') {
      result.promotedTo = ChessUtils::makePiece(promotion, ChessUtils::getPieceColor(piece));
    } else {
      // Auto-queen
      result.promotedTo = ChessUtils::makePiece('Q', ChessUtils::getPieceColor(piece));
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
// Internal: Zobrist hashing (for threefold repetition detection)
// ---------------------------------------------------------------------------

uint64_t ChessBoard::computeZobristHash() const {
  uint64_t hash = 0;

  // Hash piece positions
  ChessIterator::forEachPiece(board_, [&](int row, int col, char piece) {
    int idx = pieceToZobristIndex(piece);
    hash ^= ZOBRIST_TABLE[idx][row * 8 + col];
  });

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
