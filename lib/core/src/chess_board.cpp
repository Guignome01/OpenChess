#include "chess_board.h"

#include <cstring>

#include "chess_fen.h"
#include "chess_history.h"
#include "chess_iterator.h"
#include "chess_rules.h"

const Piece ChessBoard::INITIAL_BOARD[8][8] = {
    {Piece::B_ROOK, Piece::B_KNIGHT, Piece::B_BISHOP, Piece::B_QUEEN, Piece::B_KING, Piece::B_BISHOP, Piece::B_KNIGHT, Piece::B_ROOK},
    {Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN, Piece::B_PAWN},
    {Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE},
    {Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE},
    {Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE},
    {Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE, Piece::NONE},
    {Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN, Piece::W_PAWN},
    {Piece::W_ROOK, Piece::W_KNIGHT, Piece::W_BISHOP, Piece::W_QUEEN, Piece::W_KING, Piece::W_BISHOP, Piece::W_KNIGHT, Piece::W_ROOK}
};

ChessBoard::ChessBoard()
    : currentTurn_(Color::WHITE),
      positionHistoryCount_(0),
      cachedEval_(0.0f),
      fenDirty_(true),
      evalDirty_(true) {
  memcpy(squares_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessBoard::newGame() {
  memcpy(squares_, INITIAL_BOARD, sizeof(INITIAL_BOARD));
  currentTurn_ = Color::WHITE;
  state_ = PositionState::initial();
  positionHistoryCount_ = 0;
  invalidateCache();
  recordPosition();
}

bool ChessBoard::loadFEN(const std::string& fen) {
  if (!ChessFEN::validateFEN(fen)) return false;

  ChessFEN::fenToBoard(fen, squares_, currentTurn_, &state_);
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

  Piece piece = squares_[fromRow][fromCol];
  if (piece == Piece::NONE) return invalidMoveResult();

  // Validate turn
  if (ChessPiece::pieceColor(piece) != currentTurn_) return invalidMoveResult();

  // Validate move legality via rules
  if (!ChessRules::isValidMove(squares_, fromRow, fromCol, toRow, toCol, state_)) return invalidMoveResult();

  // Build result and apply
  MoveResult result;
  result.valid = true;
  applyMoveToBoard(fromRow, fromCol, toRow, toCol, promotion, result);

  // Advance turn and record position for repetition detection
  advanceTurn();
  recordPosition();
  char winner = ' ';
  GameResult endResult = ChessRules::isGameOver(
      squares_, currentTurn_, state_, positionHistory_, positionHistoryCount_, winner);
  result.gameResult = endResult;
  result.winnerColor = winner;

  // Check detection: checkmate implies check; otherwise test explicitly
  result.isCheck = false;
  if (endResult == GameResult::CHECKMATE) {
    result.isCheck = true;
  } else if (endResult == GameResult::IN_PROGRESS &&
             ChessRules::isCheck(squares_, currentTurn_)) {
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
  squares_[entry.fromRow][entry.fromCol] = entry.piece;

  // Clear destination
  squares_[entry.toRow][entry.toCol] = Piece::NONE;

  // Restore captured piece
  if (entry.isEnPassant) {
    // En passant: captured pawn was on epCapturedRow, same col as destination
    squares_[entry.epCapturedRow][entry.toCol] = entry.captured;
  } else if (entry.isCapture) {
    squares_[entry.toRow][entry.toCol] = entry.captured;
  }

  // Reverse castling rook move
  if (entry.isCastling) {
    auto castle = ChessUtils::checkCastling(entry.fromRow, entry.fromCol,
                                             entry.toRow, entry.toCol, entry.piece);
    if (castle.isCastling) {
      // Move rook back: it's currently at rookToCol, restore to rookFromCol
      Piece rook = squares_[entry.toRow][castle.rookToCol];
      squares_[entry.toRow][castle.rookFromCol] = rook;
      squares_[entry.toRow][castle.rookToCol] = Piece::NONE;
    }
  }

  // Restore position state from before the move
  state_ = entry.prevState;

  // Switch turn back
  currentTurn_ = ChessPiece::pieceColor(entry.piece);

  // Pop last Zobrist position
  if (positionHistoryCount_ > 0)
    positionHistoryCount_--;

  invalidateCache();
}

MoveResult ChessBoard::applyMoveEntry(const MoveEntry& entry) {
  return makeMove(entry.fromRow, entry.fromCol, entry.toRow, entry.toCol,
                  entry.isPromotion ? ChessPiece::pieceToChar(entry.promotion) : ' ');
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::string ChessBoard::getFen() const {
  if (fenDirty_) {
    cachedFen_ = ChessFEN::boardToFEN(squares_, currentTurn_, &state_);
    fenDirty_ = false;
  }
  return cachedFen_;
}

float ChessBoard::getEvaluation() const {
  if (evalDirty_) {
    cachedEval_ = ChessUtils::evaluatePosition(squares_);
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
  Piece piece = squares_[fromRow][fromCol];
  Piece capturedPiece = squares_[toRow][toCol];

  // --- En passant analysis (for state update + result) ---
  auto ep = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  result.isEnPassant = ep.isCapture;
  result.epCapturedRow = ep.capturedPawnRow;
  result.isPromotion = false;
  result.promotedTo = Piece::NONE;

  state_.epRow = ep.nextEpRow;
  state_.epCol = ep.nextEpCol;

  // --- Castling analysis (for result) ---
  auto castle = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, piece);
  result.isCastling = castle.isCastling;

  // Update halfmove clock (reset on pawn move or capture, otherwise increment)
  if (ChessPiece::pieceType(piece) == PieceType::PAWN || capturedPiece != Piece::NONE || ep.isCapture)
    state_.halfmoveClock = 0;
  else
    state_.halfmoveClock++;

  // --- Apply the physical board transform (piece move + castling rook + EP removal) ---
  Piece actualCapture;
  ChessUtils::applyBoardTransform(squares_, fromRow, fromCol, toRow, toCol, state_, actualCapture);
  result.isCapture = (actualCapture != Piece::NONE);

  // Update castling rights
  state_.castlingRights = ChessUtils::updateCastlingRights(
      state_.castlingRights, fromRow, fromCol, toRow, toCol, piece, actualCapture);

  // Pawn promotion
  if (ChessRules::isPromotion(piece, toRow)) {
    result.isPromotion = true;
    Color pieceColor = ChessPiece::pieceColor(piece);
    if (promotion != ' ' && promotion != '\0') {
      result.promotedTo = ChessPiece::makePiece(pieceColor, ChessPiece::charToPieceType(promotion));
    } else {
      // Auto-queen
      result.promotedTo = ChessPiece::makePiece(pieceColor, PieceType::QUEEN);
    }
    squares_[toRow][toCol] = result.promotedTo;
  }
}



// ---------------------------------------------------------------------------
// Internal: turn advancement + position recording
// ---------------------------------------------------------------------------

void ChessBoard::advanceTurn() {
  if (currentTurn_ == Color::BLACK)
    state_.fullmoveClock++;
  currentTurn_ = ~currentTurn_;
}

// ---------------------------------------------------------------------------
// Internal: position recording (Zobrist hashing for threefold repetition)
// ---------------------------------------------------------------------------

void ChessBoard::recordPosition() {
  if (state_.halfmoveClock == 0 && positionHistoryCount_ > 0)
    positionHistoryCount_ = 0;

  if (positionHistoryCount_ < MAX_POSITION_HISTORY) {
    positionHistory_[positionHistoryCount_++] =
        ChessHash::computeHash(squares_, currentTurn_, state_);
  }
}
