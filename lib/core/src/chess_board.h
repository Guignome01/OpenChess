#ifndef CORE_CHESS_BOARD_H
#define CORE_CHESS_BOARD_H

#include <cstring>
#include <string>

#include "chess_movegen.h"
#include "chess_bitboard.h"
#include "chess_evaluation.h"
#include "chess_hash.h"
#include "chess_iterator.h"
#include "chess_rules.h"
#include "chess_utils.h"
#include "types.h"

struct MoveEntry;  // forward declaration for reverseMove/applyMoveEntry

// Board representation and position-level chess logic.
// Owns a BitboardSet (12 piece bitboards + color aggregates + occupancy),
// a parallel Piece mailbox[64] for O(1) piece lookup, current turn, position
// state (castling, en passant, clocks), and Zobrist hash history.
//
// Handles move validation, application, and game-end detection — all end
// conditions (checkmate, stalemate, 50-move, insufficient material, threefold)
// are detected uniformly via ChessRules::isGameOver() and returned via MoveResult.
//
// Pure position container: no lifecycle state (game-over flag, result, winner).
// Lifecycle authority lives in ChessGame.
// Pure logic, no hardware dependencies, natively compilable for unit tests.
//
class ChessBoard {
 public:
  ChessBoard();

  // --- Lifecycle ---

  void newGame();
  bool loadFEN(const std::string& fen);

  // --- Moves ---

  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');
  void reverseMove(const MoveEntry& entry);
  MoveResult applyMoveEntry(const MoveEntry& entry);

  // --- Queries ---

  Piece getSquare(int row, int col) const {
    return mailbox_[ChessBitboard::squareOf(row, col)];
  }
  Color currentTurn() const { return currentTurn_; }

  int kingRow(Color c) const { return ChessBitboard::rowOf(kingSquare_[ChessPiece::raw(c)]); }
  int kingCol(Color c) const { return ChessBitboard::colOf(kingSquare_[ChessPiece::raw(c)]); }

  uint8_t getCastlingRights() const { return state_.castlingRights; }
  const PositionState& positionState() const { return state_; }

  std::string getFen() const;
  int getEvaluation() const;

  // Expose bitboard and mailbox for core-internal callers (ChessNotation, etc.)
  const ChessBitboard::BitboardSet& bitboards() const { return bb_; }
  const Piece* mailbox() const { return mailbox_; }

  // --- Convenience wrappers (delegate to ChessRules / ChessUtils) ---

  void getPossibleMoves(int row, int col, MoveList& moves) const {
    ChessRules::getPossibleMoves(bb_, mailbox_, row, col, state_, moves);
  }

  bool isCheck(Color kingColor) const {
    return ChessRules::isSquareUnderAttack(bb_, kingSquare_[ChessPiece::raw(kingColor)], kingColor);
  }

  bool inCheck() const {
    return ChessRules::isSquareUnderAttack(bb_, kingSquare_[ChessPiece::raw(currentTurn_)], currentTurn_);
  }

  bool isCheckmate() const {
    return ChessRules::isCheckmate(bb_, mailbox_, currentTurn_, state_);
  }

  bool isStalemate() const {
    return ChessRules::isStalemate(bb_, mailbox_, currentTurn_, state_);
  }

  bool isInsufficientMaterial() const {
    return ChessRules::isInsufficientMaterial(bb_);
  }

  bool isFiftyMoves() const {
    return ChessRules::isFiftyMoveRule(state_);
  }

  bool isThreefoldRepetition() const {
    return ChessRules::isThreefoldRepetition(hashHistory_);
  }

  bool isDraw() const {
    return ChessRules::isDraw(bb_, mailbox_, currentTurn_, state_, hashHistory_);
  }

  bool isAttacked(int row, int col, Color byColor) const {
    Color defendingColor = ~byColor;
    return ChessRules::isSquareUnderAttack(bb_, row, col, defendingColor);
  }

  int findPiece(Piece target, int positions[][2], int maxPositions) const {
    return ChessIterator::findPiece(bb_, target, positions, maxPositions);
  }

  std::string boardToText() const { return ChessUtils::boardToText(mailbox_); }

  int moveNumber() const { return state_.fullmoveClock; }

  ChessUtils::EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol,
                                      mailbox_[ChessBitboard::squareOf(fromRow, fromCol)],
                                      mailbox_[ChessBitboard::squareOf(toRow, toCol)]);
  }

  ChessUtils::CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol,
                                     mailbox_[ChessBitboard::squareOf(fromRow, fromCol)]);
  }

  // --- Constants ---

  static const Piece INITIAL_BOARD[8][8];

 private:
  ChessBitboard::BitboardSet bb_;  // 12 piece bitboards + color + occupancy
  Piece mailbox_[64];              // parallel flat array for O(1) getSquare()
  Color currentTurn_;
  PositionState state_;

  // Cached king positions (LERF square indices, one per color).
  ChessBitboard::Square kingSquare_[2];

  // Incremental Zobrist hash — updated in applyMoveToBoard, verified by computeHash.
  uint64_t hash_;

  // Zobrist position history (for threefold repetition detection)
  HashHistory hashHistory_;

  // FEN / evaluation cache (mutable: updated lazily from const getters)
  mutable std::string cachedFen_;
  mutable int cachedEval_;
  mutable bool fenDirty_;
  mutable bool evalDirty_;
  void invalidateCache();

  void recordPosition();
  void applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result);
  void advanceTurn();
};

#endif  // CORE_CHESS_BOARD_H
