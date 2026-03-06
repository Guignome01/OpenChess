#ifndef CORE_CHESS_BOARD_H
#define CORE_CHESS_BOARD_H

#include <cctype>
#include <cstring>
#include <string>

#include "types.h"
#include "chess_rules.h"
#include "chess_utils.h"
#include "zobrist_keys.h"

struct MoveEntry;  // forward declaration for reverseMove/applyMoveEntry

// Board representation and position-level chess logic.
// Owns the 8x8 board array, current turn, position state (castling, en passant,
// clocks), and Zobrist position history for threefold repetition.
// Handles move validation, application, and game-end detection — all end
// conditions (checkmate, stalemate, 50-move, insufficient material, threefold)
// are detected uniformly inside detectGameEnd() and returned via MoveResult.
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

  const char (&getBoard() const)[8][8] { return board_; }
  char getSquare(int row, int col) const { return board_[row][col]; }
  char currentTurn() const { return currentTurn_; }

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
  bool isCheck(char kingColor) const {
    return ChessRules::isCheck(board_, kingColor);
  }

  // Is the side to move in check?
  bool inCheck() const {
    return ChessRules::isCheck(board_, currentTurn_);
  }

  // Is the side to move checkmated?
  bool isCheckmate() const {
    return ChessRules::isCheckmate(board_, currentTurn_, state_);
  }

  // Is the side to move stalemated?
  bool isStalemate() const {
    return ChessRules::isStalemate(board_, currentTurn_, state_);
  }

  // Is the position drawn due to insufficient material?
  bool isInsufficientMaterial() const;

  // Has the 50-move rule been reached (100 half-moves)?
  bool isFiftyMoves() const { return state_.halfmoveClock >= 100; }

  // Has the current position occurred three or more times?
  bool isThreefoldRepetition() const;

  // Is the position drawn by any automatic draw rule?
  // (50-move, insufficient material, threefold, stalemate)
  bool isDraw() const;

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

  // --- Constants ---

  static const char INITIAL_BOARD[8][8];

  // Maximum stored positions (sufficient for any legal game)
  static constexpr int MAX_POSITION_HISTORY = 128;

 private:
  char board_[8][8];
  char currentTurn_;
  PositionState state_;  // castling rights, en passant, clocks

  // Zobrist position history (for threefold repetition detection)
  uint64_t positionHistory_[MAX_POSITION_HISTORY];
  int positionHistoryCount_;

  // FEN / evaluation cache (mutable: updated lazily from const getters)
  mutable std::string cachedFen_;
  mutable float cachedEval_;
  mutable bool fenDirty_;
  mutable bool evalDirty_;
  void invalidateCache();

  bool hasInsufficientMaterialInternal() const;

  // Zobrist hashing
  static inline int pieceToZobristIndex(char piece) {
    const char* pieces = "PNBRQKpnbrqk";
    const char* p = std::strchr(pieces, piece);
    return p ? static_cast<int>(p - pieces) : -1;
  }
  uint64_t computeZobristHash() const;
  void recordPosition();

  // Pure chess logic extracted from GameMode::applyMove / updateGameStatus
  void applyMoveToBoard(int fromRow, int fromCol, int toRow, int toCol, char promotion, MoveResult& result);
  void advanceTurn();
  GameResult detectGameEnd(char& winner);
};

#endif  // CORE_CHESS_BOARD_H
