#ifndef CORE_BOARD_H
#define CORE_BOARD_H

#include <cctype>
#include <cstring>
#include <functional>
#include <string>

#include "history.h"
#include "types.h"
#include "rules.h"
#include "utils.h"

// Complete chess game state manager — the "chess.js" of this project.
// Owns the board, turn, rules, and game-over state.
// Pure logic, no hardware dependencies, natively compilable for unit tests.
//
// Usage:
//   ChessBoard cb;
//   cb.newGame();                       // standard starting position
//   MoveResult r = cb.makeMove(6,4, 4,4);  // e2e4
//   if (!r.valid) { /* illegal */ }
//   cb.loadFEN("...");                  // load arbitrary position
//   cb.endGame(GameResult::RESIGNATION, 'b'); // manual termination
//
class ChessBoard {
 public:
  // State-change callback — fired after every mutating operation
  // (makeMove, loadFEN, newGame, endGame) unless batching is active.
  using StateCallback = std::function<void()>;

  ChessBoard();

  // --- Lifecycle ---

  // Reset to standard initial position.
  void newGame();

  // Load arbitrary position from FEN.  Resets game-over state.
  // Returns false (and leaves board unchanged) if the FEN is malformed.
  bool loadFEN(const std::string& fen);

  // Manually end the game (resignation, timeout, abort, etc.).
  void endGame(GameResult result, char winnerColor);

  // --- Moves ---

  // Validate and execute a move.  Returns full MoveResult.
  // Promotion char: 'q','r','b','n' or ' ' for auto-queen.
  MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

  // --- Queries ---

  const char (&getBoard() const)[8][8] { return board_; }
  char getSquare(int row, int col) const { return board_[row][col]; }
  char currentTurn() const { return currentTurn_; }
  bool isGameOver() const { return gameOver_; }
  GameResult gameResult() const { return gameResult_; }
  char winnerColor() const { return winnerColor_; }

  // Position state accessors
  uint8_t getCastlingRights() const { return state_.castlingRights; }
  const PositionState& positionState() const { return state_; }

  // Current position as FEN string.
  std::string getFen() const;

  // Material evaluation (positive = white advantage).
  float getEvaluation() const;

  // --- Convenience wrappers (delegate to ChessRules / ChessUtils) ---

  // Legal moves for the piece at (row, col). Returns moveCount; fills moves[][2].
  void getPossibleMoves(int row, int col, int& moveCount, int moves[][2]) const {
    ChessRules::getPossibleMoves(board_, row, col, state_, moveCount, moves);
  }

  // Is the given color's king in check?
  bool isKingInCheck(char kingColor) const {
    return ChessRules::isKingInCheck(board_, kingColor);
  }

  // Is the side to move in check?
  bool inCheck() const {
    return ChessRules::isKingInCheck(board_, currentTurn_);
  }

  // Is the side to move checkmated?
  bool isCheckmate() const {
    return ChessRules::isCheckmate(board_, currentTurn_, state_);
  }

  // Is the side to move stalemated?
  bool isStalemate() const {
    return ChessRules::isStalemate(board_, currentTurn_, state_);
  }

  // Is the position a draw (50-move, threefold repetition, or insufficient material)?
  bool isDraw() const;

  // Has the current position occurred three or more times?
  bool isThreefoldRepetition() const;

  // Is the position drawn due to insufficient material?
  bool isInsufficientMaterial() const;

  // Is the given square attacked by pieces of the specified color?
  // "byColor" is the attacking side: 'w' or 'b'.
  bool isAttacked(int row, int col, char byColor) const {
    // ChessRules defines "defendingColor" as the side that owns the square.
    // Attacked-by 'w' means squares defended by 'w' = attacked from 'b' perspective.
    char defendingColor = (byColor == 'w') ? 'b' : 'w';
    return ChessRules::isSquareUnderAttack(board_, row, col, defendingColor);
  }

  // Find all pieces of the given type and color.
  // type: uppercase piece letter ('P','N','B','R','Q','K').
  // color: 'w' or 'b'.
  // Returns count; fills positions[][2] with [row, col] pairs.
  int findPiece(char type, char color, int positions[][2], int maxPositions) const;

  // Locate the king of the given color. Returns false if not found.
  bool findKingPosition(char kingColor, int& kingRow, int& kingCol) const;

  // Current full-move number (starts at 1, increments after black's move).
  int moveNumber() const { return state_.fullmoveClock; }

  // En passant analysis for a move on this board.
  ChessUtils::EnPassantInfo checkEnPassant(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkEnPassant(fromRow, fromCol, toRow, toCol,
                                      board_[fromRow][fromCol], board_[toRow][toCol]);
  }

  // Castling analysis for a move on this board.
  ChessUtils::CastlingInfo checkCastling(int fromRow, int fromCol, int toRow, int toCol) const {
    return ChessUtils::checkCastling(fromRow, fromCol, toRow, toCol,
                                     board_[fromRow][fromCol]);
  }

  // --- History ---

  // Access the in-game move/position history.
  const ChessHistory& history() const { return history_; }

  // --- Batching (suppress callbacks during replay) ---

  void beginBatch();
  void endBatch();  // fires one callback on exit if changes occurred

  // --- Callback ---

  void onStateChanged(StateCallback cb) { stateCallback_ = cb; }

  // --- Constants ---

  static const char INITIAL_BOARD[8][8];

 private:
  char board_[8][8];
  char currentTurn_;
  bool gameOver_;
  GameResult gameResult_;
  char winnerColor_;
  PositionState state_;  // castling rights, en passant, clocks
  ChessHistory history_;  // move log + position hash tracking

  // FEN / evaluation cache (mutable: updated lazily from const getters)
  mutable std::string cachedFen_;
  mutable float cachedEval_;
  mutable bool fenDirty_;
  mutable bool evalDirty_;
  void invalidateCache();

  // Batching
  int batchDepth_;
  bool batchDirty_;

  StateCallback stateCallback_;

  // --- Zobrist hashing (computed here, stored in history_) ---

  static inline int pieceToZobristIndex(char piece) {
    const char* pieces = "PNBRQKpnbrqk";
    const char* p = std::strchr(pieces, piece);
    return p ? static_cast<int>(p - pieces) : -1;
  }

  uint64_t computeZobristHash() const;
  void recordPosition();
  bool hasInsufficientMaterialInternal() const;

  // Pure chess logic extracted from GameMode::applyMove / updateGameStatus
  void applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result);
  void advanceTurn();
  GameResult detectGameEnd(char& winner);

  void fireCallback();
};

#endif  // CORE_BOARD_H
