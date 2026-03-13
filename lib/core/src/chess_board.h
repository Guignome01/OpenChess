#ifndef CORE_CHESS_BOARD_H
#define CORE_CHESS_BOARD_H

#include <cstring>
#include <string>

#include "types.h"
#include "chess_hash.h"
#include "chess_iterator.h"
#include "chess_rules.h"
#include "chess_utils.h"

struct MoveEntry;  // forward declaration for reverseMove/applyMoveEntry

// Board representation and position-level chess logic.
// Owns the 8x8 board array, current turn, position state (castling, en passant,
// clocks), and Zobrist position history for threefold repetition.
// Handles move validation, application, and game-end detection — all end
// conditions (checkmate, stalemate, 50-move, insufficient material, threefold)
// are detected uniformly via ChessRules::isGameOver() and returned via MoveResult.
// Pure position container: no lifecycle state (game-over flag, result, winner).
// Lifecycle authority lives in ChessGame.
// Pure logic, no hardware dependencies, natively compilable for unit tests.
//
// Note: move history, observer notification, batching, and game-over state live in ChessGame
// (the game-level orchestrator that composes ChessBoard).
//
// Usage:
//   ChessBoard cb;
//   cb.newGame();                       // standard starting position
//   MoveResult r = cb.makeMove(6,4, 4,4);  // e2e4
//   if (!r.valid) { /* illegal */ }
//   cb.loadFEN("...");                  // load arbitrary position
//
class ChessBoard {
 public:
  ChessBoard();

  // --- Lifecycle ---

  // Reset to standard initial position.
  void newGame();

  // Load arbitrary position from FEN.
  // Returns false (and leaves board unchanged) if the FEN is malformed.
  bool loadFEN(const std::string& fen);

  // --- Moves ---

  // Validate and execute a move.  Returns full MoveResult.
  // Promotion char: 'q','r','b','n' or ' ' for auto-queen.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // Reverse a previously applied move using its MoveEntry.
  // Restores board, turn, position state, and Zobrist history.
  void reverseMove(const MoveEntry& entry);

  // Re-apply a move from a MoveEntry (for redo).  Returns the MoveResult.
  // Internally calls makeMove with the stored coordinates and promotion.
  MoveResult applyMoveEntry(const MoveEntry& entry);

  // --- Queries ---

  const Piece (&getBoard() const)[8][8] { return squares_; }
  Piece getSquare(int row, int col) const { return squares_[row][col]; }
  Color currentTurn() const { return currentTurn_; }

  // Cached king position for the given color.
  int kingRow(Color c) const { return kingSquare_[ChessPiece::raw(c)][0]; }
  int kingCol(Color c) const { return kingSquare_[ChessPiece::raw(c)][1]; }

  // Position state accessors
  uint8_t getCastlingRights() const { return state_.castlingRights; }
  const PositionState& positionState() const { return state_; }

  // Current position as FEN string.
  std::string getFen() const;

  // Material evaluation (positive = white advantage).
  float getEvaluation() const;

  // --- Convenience wrappers (delegate to ChessRules / ChessUtils) ---

  // Legal moves for the piece at (row, col).
  void getPossibleMoves(int row, int col, MoveList& moves) const {
    ChessRules::getPossibleMoves(squares_, row, col, state_, moves);
  }

  // Is the given color's king in check?
  bool isCheck(Color kingColor) const {
    return ChessRules::isSquareUnderAttack(squares_, kingRow(kingColor), kingCol(kingColor), kingColor);
  }

  // Is the side to move in check?
  bool inCheck() const {
    return ChessRules::isSquareUnderAttack(squares_, kingRow(currentTurn_), kingCol(currentTurn_), currentTurn_);
  }

  // Is the side to move checkmated?
  bool isCheckmate() const {
    return ChessRules::isCheckmate(squares_, currentTurn_, state_);
  }

  // Is the side to move stalemated?
  bool isStalemate() const {
    return ChessRules::isStalemate(squares_, currentTurn_, state_);
  }

  // Is the position drawn due to insufficient material?
  bool isInsufficientMaterial() const {
    return ChessRules::isInsufficientMaterial(squares_);
  }

  // Has the 50-move rule been reached (100 half-moves)?
  bool isFiftyMoves() const {
    return ChessRules::isFiftyMoveRule(state_);
  }

  // Has the current position occurred three or more times?
  bool isThreefoldRepetition() const {
    return ChessRules::isThreefoldRepetition(hashHistory_);
  }

  // Is the position drawn by any automatic draw rule?
  // (stalemate, 50-move, insufficient material, threefold)
  bool isDraw() const {
    return ChessRules::isDraw(squares_, currentTurn_, state_, hashHistory_);
  }

  // Is the given square attacked by pieces of the specified color?
  bool isAttacked(int row, int col, Color byColor) const {
    Color defendingColor = ~byColor;
    return ChessRules::isSquareUnderAttack(squares_, row, col, defendingColor);
  }

  // Find all pieces matching the given Piece value.
  // Returns count; fills positions[][2] with [row, col] pairs.
  int findPiece(Piece target, int positions[][2], int maxPositions) const {
    return ChessIterator::findPiece(squares_, target, positions, maxPositions);
  }

  // Format the board as a human-readable text block.
  std::string boardToText() const { return ChessUtils::boardToText(squares_); }

  // Current full-move number (starts at 1, increments after black's move).
  int moveNumber() const { return state_.fullmoveClock; }

  // En passant analysis for a move on this board.
  ChessUtils::EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol,
                                      squares_[fromRow][fromCol], squares_[toRow][toCol]);
  }

  // Castling analysis for a move on this board.
  ChessUtils::CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol,
                                     squares_[fromRow][fromCol]);
  }

  // --- Constants ---

  static const Piece INITIAL_BOARD[8][8];

 private:
  Piece squares_[8][8];
  Color currentTurn_;
  PositionState state_;  // castling rights, en passant, clocks

  // Cached king positions indexed by Color (0=WHITE, 1=BLACK).
  // Updated by newGame(), loadFEN(), applyMoveToBoard(), reverseMove().
  int kingSquare_[2][2];  // [color][0]=row, [color][1]=col

  // Zobrist position history (for threefold repetition detection)
  HashHistory hashHistory_;

  // FEN / evaluation cache (mutable: updated lazily from const getters)
  mutable std::string cachedFen_;
  mutable float cachedEval_;
  mutable bool fenDirty_;
  mutable bool evalDirty_;
  void invalidateCache();

  void recordPosition();

  // Pure chess logic extracted from GameMode::applyMove / updateGameStatus
  void applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result);
  void advanceTurn();
};

#endif  // CORE_CHESS_BOARD_H
