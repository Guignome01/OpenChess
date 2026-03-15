#include "chess_board.h"

#include <cstring>

#include "chess_fen.h"
#include "chess_history.h"

using namespace ChessBitboard;

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
      hash_(0),
      cachedEval_(0),
      fenDirty_(true),
      evalDirty_(true) {
  ChessMovegen::initAttacks();
  bb_.clear();
  memset(mailbox_, 0, sizeof(mailbox_));
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col) {
      Piece p = INITIAL_BOARD[row][col];
      if (p != Piece::NONE) {
        Square sq = squareOf(row, col);
        bb_.setPiece(sq, p);
        mailbox_[sq] = p;
      }
    }
  kingSquare_[0] = squareOf(7, 4);  // White king at e1
  kingSquare_[1] = squareOf(0, 4);  // Black king at e8
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ChessBoard::newGame() {
  bb_.clear();
  memset(mailbox_, 0, sizeof(mailbox_));
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col) {
      Piece p = INITIAL_BOARD[row][col];
      if (p != Piece::NONE) {
        Square sq = squareOf(row, col);
        bb_.setPiece(sq, p);
        mailbox_[sq] = p;
      }
    }
  currentTurn_ = Color::WHITE;
  state_ = PositionState::initial();
  hashHistory_.count = 0;
  kingSquare_[0] = squareOf(7, 4);
  kingSquare_[1] = squareOf(0, 4);
  hash_ = ChessHash::computeHash(bb_, mailbox_, currentTurn_, state_);
  invalidateCache();
  recordPosition();
}

bool ChessBoard::loadFEN(const std::string& fen) {
  if (!ChessFEN::validateFEN(fen)) return false;

  ChessFEN::fenToBoard(fen, bb_, mailbox_, currentTurn_, &state_);
  hashHistory_.count = 0;

  // Locate king positions from bitboards
  int wkIdx = ChessPiece::pieceZobristIndex(Piece::W_KING);
  int bkIdx = ChessPiece::pieceZobristIndex(Piece::B_KING);
  kingSquare_[0] = bb_.byPiece[wkIdx] ? lsb(bb_.byPiece[wkIdx]) : SQ_NONE;
  kingSquare_[1] = bb_.byPiece[bkIdx] ? lsb(bb_.byPiece[bkIdx]) : SQ_NONE;

  hash_ = ChessHash::computeHash(bb_, mailbox_, currentTurn_, state_);
  invalidateCache();
  recordPosition();
  return true;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

MoveResult ChessBoard::makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (!ChessUtils::isValidSquare(fromRow, fromCol) || !ChessUtils::isValidSquare(toRow, toCol))
    return invalidMoveResult();

  Piece piece = getSquare(fromRow, fromCol);
  if (piece == Piece::NONE) return invalidMoveResult();
  if (ChessPiece::pieceColor(piece) != currentTurn_) return invalidMoveResult();

  Square from = squareOf(fromRow, fromCol);
  Square to = squareOf(toRow, toCol);
  if (!ChessRules::isValidMove(bb_, mailbox_, from, to, state_, kingSquare_[ChessPiece::raw(currentTurn_)]))
    return invalidMoveResult();

  MoveResult result;
  result.valid = true;
  applyMoveToBoard(fromRow, fromCol, toRow, toCol, promotion, result);

  advanceTurn();
  recordPosition();
  char winner = ' ';
  GameResult endResult = ChessRules::isGameOver(
      bb_, mailbox_, currentTurn_, state_, hashHistory_, winner);
  result.gameResult = endResult;
  result.winnerColor = winner;

  result.isCheck = false;
  if (endResult == GameResult::CHECKMATE) {
    result.isCheck = true;
  } else if (endResult == GameResult::IN_PROGRESS &&
             ChessRules::isSquareUnderAttack(bb_, kingSquare_[ChessPiece::raw(currentTurn_)], currentTurn_)) {
    result.isCheck = true;
  }

  invalidateCache();
  return result;
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

void ChessBoard::reverseMove(const MoveEntry& entry) {
  Square from = squareOf(entry.fromRow, entry.fromCol);
  Square to = squareOf(entry.toRow, entry.toCol);

  // Get what's currently at destination (may be promoted piece)
  Piece currentAtTo = mailbox_[to];

  // Remove piece from destination
  bb_.removePiece(to, currentAtTo);
  mailbox_[to] = Piece::NONE;

  // Restore original piece at source
  bb_.setPiece(from, entry.piece);
  mailbox_[from] = entry.piece;

  // Restore captured piece
  if (entry.isEnPassant) {
    Square epSq = squareOf(entry.epCapturedRow, entry.toCol);
    bb_.setPiece(epSq, entry.captured);
    mailbox_[epSq] = entry.captured;
  } else if (entry.isCapture) {
    bb_.setPiece(to, entry.captured);
    mailbox_[to] = entry.captured;
  }

  // Reverse castling rook
  if (entry.isCastling) {
    auto castle = ChessUtils::checkCastling(entry.fromRow, entry.fromCol,
                                             entry.toRow, entry.toCol, entry.piece);
    if (castle.isCastling) {
      Square rookTo = squareOf(entry.toRow, castle.rookToCol);
      Square rookFrom = squareOf(entry.toRow, castle.rookFromCol);
      Piece rook = mailbox_[rookTo];
      bb_.movePiece(rookTo, rookFrom, rook);
      mailbox_[rookFrom] = rook;
      mailbox_[rookTo] = Piece::NONE;
    }
  }

  // Restore state
  state_ = entry.prevState;
  currentTurn_ = ChessPiece::pieceColor(entry.piece);

  if (ChessPiece::pieceType(entry.piece) == PieceType::KING)
    kingSquare_[ChessPiece::raw(currentTurn_)] = from;

  // Pop hash and recompute (reverseMove is not a hot path)
  if (hashHistory_.count > 0)
    hashHistory_.count--;
  hash_ = ChessHash::computeHash(bb_, mailbox_, currentTurn_, state_);

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
    cachedFen_ = ChessFEN::boardToFEN(mailbox_, currentTurn_, &state_);
    fenDirty_ = false;
  }
  return cachedFen_;
}

int ChessBoard::getEvaluation() const {
  if (evalDirty_) {
    cachedEval_ = ChessEval::evaluatePosition(bb_);
    evalDirty_ = false;
  }
  return cachedEval_;
}

void ChessBoard::invalidateCache() {
  fenDirty_ = true;
  evalDirty_ = true;
}

// ---------------------------------------------------------------------------
// Internal: apply move to board with incremental Zobrist hash
// ---------------------------------------------------------------------------

void ChessBoard::applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol,
                                   char promotion, MoveResult& result) {
  Square from = squareOf(fromRow, fromCol);
  Square to = squareOf(toRow, toCol);
  Piece piece = mailbox_[from];
  Piece capturedPiece = mailbox_[to];

  // --- Analyze ---
  auto ep = ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol, piece, capturedPiece);
  result.isEnPassant = ep.isCapture;
  result.epCapturedRow = ep.capturedPawnRow;
  result.isPromotion = false;
  result.promotedTo = Piece::NONE;

  auto castle = ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol, piece);
  result.isCastling = castle.isCastling;

  // --- Hash: remove old state keys ---
  hash_ ^= ChessHash::KEYS.castling[state_.castlingRights];
  if (state_.epRow >= 0 && state_.epCol >= 0 &&
      ChessRules::hasLegalEnPassantCapture(bb_, mailbox_, currentTurn_, state_))
    hash_ ^= ChessHash::KEYS.enPassant[state_.epCol];

  // --- Update EP state ---
  state_.epRow = ep.nextEpRow;
  state_.epCol = ep.nextEpCol;

  // --- Halfmove clock ---
  if (ChessPiece::pieceType(piece) == PieceType::PAWN || capturedPiece != Piece::NONE || ep.isCapture)
    state_.halfmoveClock = 0;
  else
    state_.halfmoveClock++;

  // --- Apply board transform ---
  Piece actualCapture;
  ChessUtils::applyBoardTransform(bb_, mailbox_, from, to, ep, castle, actualCapture);
  result.isCapture = (actualCapture != Piece::NONE);

  // --- Hash: piece movements ---
  int pieceIdx = ChessPiece::pieceZobristIndex(piece);
  hash_ ^= ChessHash::KEYS.pieces[pieceIdx][from];
  hash_ ^= ChessHash::KEYS.pieces[pieceIdx][to];

  if (actualCapture != Piece::NONE) {
    int capIdx = ChessPiece::pieceZobristIndex(actualCapture);
    Square capSq = ep.isCapture ? squareOf(ep.capturedPawnRow, colOf(to)) : to;
    hash_ ^= ChessHash::KEYS.pieces[capIdx][capSq];
  }

  if (castle.isCastling) {
    Piece rook = ChessPiece::makePiece(ChessPiece::pieceColor(piece), PieceType::ROOK);
    int rookIdx = ChessPiece::pieceZobristIndex(rook);
    Square rookFrom = squareOf(fromRow, castle.rookFromCol);
    Square rookTo = squareOf(fromRow, castle.rookToCol);
    hash_ ^= ChessHash::KEYS.pieces[rookIdx][rookFrom];
    hash_ ^= ChessHash::KEYS.pieces[rookIdx][rookTo];
  }

  // --- Update castling rights ---
  state_.castlingRights = ChessUtils::updateCastlingRights(
      state_.castlingRights, fromRow, fromCol, toRow, toCol, piece, actualCapture);

  // --- Promotion ---
  if (ChessPiece::isPromotion(piece, toRow)) {
    result.isPromotion = true;
    Color pieceColor = ChessPiece::pieceColor(piece);
    Piece promotedTo;
    if (promotion != ' ' && promotion != '\0')
      promotedTo = ChessPiece::makePiece(pieceColor, ChessPiece::charToPieceType(promotion));
    else
      promotedTo = ChessPiece::makePiece(pieceColor, PieceType::QUEEN);
    result.promotedTo = promotedTo;

    bb_.removePiece(to, piece);
    bb_.setPiece(to, promotedTo);
    mailbox_[to] = promotedTo;

    // Hash: swap pawn → promoted piece at destination
    hash_ ^= ChessHash::KEYS.pieces[pieceIdx][to];
    hash_ ^= ChessHash::KEYS.pieces[ChessPiece::pieceZobristIndex(promotedTo)][to];
  }

  // --- Hash: add new state keys ---
  hash_ ^= ChessHash::KEYS.castling[state_.castlingRights];

  // Toggle side to move
  hash_ ^= ChessHash::KEYS.sideToMove;

  // New EP key (checking with the next side to move = opponent)
  Color nextSide = ~currentTurn_;
  if (state_.epRow >= 0 && state_.epCol >= 0 &&
      ChessRules::hasLegalEnPassantCapture(bb_, mailbox_, nextSide, state_))
    hash_ ^= ChessHash::KEYS.enPassant[state_.epCol];

  // --- Update king cache ---
  if (ChessPiece::pieceType(piece) == PieceType::KING)
    kingSquare_[ChessPiece::raw(ChessPiece::pieceColor(piece))] = to;
}

// ---------------------------------------------------------------------------
// Internal: turn advancement + position recording
// ---------------------------------------------------------------------------

void ChessBoard::advanceTurn() {
  if (currentTurn_ == Color::BLACK)
    state_.fullmoveClock++;
  currentTurn_ = ~currentTurn_;
}

void ChessBoard::recordPosition() {
  if (state_.halfmoveClock == 0 && hashHistory_.count > 0)
    hashHistory_.count = 0;

  if (hashHistory_.count < HashHistory::MAX_SIZE)
    hashHistory_.keys[hashHistory_.count++] = hash_;
}
