#ifndef CORE_HISTORY_H
#define CORE_HISTORY_H

#include <cstdint>
#include <string>

#include "game_storage.h"
#include "logger.h"
#include "types.h"

class ChessBoard;  // forward declaration for replayInto()

// A single move record in the game history.
// Stores enough information to query the move log and support future undo.
struct MoveEntry {
  int fromRow, fromCol;
  int toRow, toCol;
  char piece;           // piece that moved (original, before any promotion)
  char captured;        // piece captured (' ' if none)
  char promotion;       // piece promoted to (' ' if not a promotion)
  bool isCapture;
  bool isEnPassant;
  int epCapturedRow;    // en passant captured pawn row (-1 if N/A)
  bool isCastling;
  bool isPromotion;
  bool isCheck;
  PositionState prevState;  // position state before the move (enables undo)
};

// In-memory game history with optional persistent recording.
//
// Two concerns:
//   1. Move log: ordered list of all moves played in the game
//   2. Recording: persistent game storage (when IGameStorage is provided)
//
// Position tracking (Zobrist hashes for threefold repetition) lives in
// ChessBoard, which detects all end conditions uniformly.
//
// Nullable storage: pass nullptr to disable recording (e.g. Lichess mode,
// stand-alone ChessGame without persistence).
class ChessHistory {
 public:
  ChessHistory(IGameStorage* storage = nullptr, ILogger* logger = nullptr);

  // Reset in-memory history (move log only).
  // Does NOT affect recording state — use discardRecording() for that.
  void clear();

  // --- Move log ---

  // Append a move entry to the log.
  void addMove(const MoveEntry& entry);

  // Remove and return the last move (for undo). Returns false if empty.
  bool popMove(MoveEntry& entry);

  // Number of half-moves recorded.
  int moveCount() const { return moveCount_; }

  // True when no moves have been recorded.
  bool empty() const { return moveCount_ == 0; }

  // Access the move at the given index (0 = first move).
  // Caller must ensure 0 <= index < moveCount().
  const MoveEntry& getMove(int index) const;

  // Access the most recent move.
  // Caller must ensure !empty().
  const MoveEntry& lastMove() const;

  // --- Recording lifecycle (persistent storage) ---

  // Begin recording a new game.  Discards any leftover live game first.
  void startRecording(GameModeId mode, uint8_t playerColor = '?', uint8_t botDepth = 0);

  // Encode and persist a move to storage.
  void recordMove(int fromRow, int fromCol, int toRow, int toCol, char promotion);

  // Persist a FEN snapshot to storage (e.g. initial position, mid-game load).
  void recordFen(const std::string& fen);

  // Finalize recording — set result/winner, close storage files.
  void finishRecording(GameResult result, char winnerColor);

  // Discard current recording without finalizing.
  void discardRecording();

  bool isRecording() const { return recording_; }

  // --- State queries (persistent storage) ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Replay ---

  // Restore a saved game into a ChessBoard: loads the last FEN snapshot,
  // then replays all moves after it.  Returns true on success.
  bool replayInto(ChessBoard& board);

  // --- Constants ---

  static constexpr int MAX_MOVES = 300;

 private:
  // In-memory move log
  MoveEntry moves_[MAX_MOVES];
  int moveCount_;

  // Recording (persistent storage)
  IGameStorage* storage_;
  ILogger* logger_;
  GameHeader header_;
  bool recording_;
  uint8_t movesSinceFlush_;
};

#endif  // CORE_HISTORY_H
