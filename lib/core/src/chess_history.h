#ifndef CORE_CHESS_HISTORY_H
#define CORE_CHESS_HISTORY_H

#include <cstdint>
#include <string>

#include "game_storage.h"
#include "logger.h"
#include "types.h"

class ChessBoard;  // forward declaration for replayInto()

// A single move record in the game history.
// Stores enough information to query the move log, support undo/redo,
// and reconstruct board state.
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
// Two concerns, unified under a single API:
//   1. Move log: ordered list of all moves with cursor-based undo/redo
//   2. Recording: persistent game storage (when IGameStorage is provided)
//
// Recording is automatic: if an IGameStorage* is provided and a header has
// been set (via setHeader), addMove() persists encoded moves transparently.
// No explicit startRecording/recordMove calls needed from the caller.
//
// Undo/redo: a cursor (currentIndex_) tracks the "current" position in the
// move log.  undoMove() steps back, redoMove() steps forward.  addMove()
// wipes any moves after the cursor (branch point) before appending.
//
// Nullable storage: pass nullptr to disable recording (e.g. Lichess mode,
// stand-alone ChessGame without persistence).
class ChessHistory {
 public:
  ChessHistory(IGameStorage* storage = nullptr, ILogger* logger = nullptr);

  // Reset in-memory history (move log + cursor).
  // Does NOT start or affect recording state.
  void clear();

  // --- Move log with undo/redo ---

  // Append a move entry to the log.  If the cursor is not at the end
  // (after undo), wipes all moves after the cursor before appending
  // (branch point).  Also persists to storage if recording is active.
  void addMove(const MoveEntry& entry);

  // Step back one move.  Returns pointer to the undone MoveEntry
  // (caller uses it to reverse the move on ChessBoard).
  // Returns nullptr if at the beginning (canUndo() is false).
  const MoveEntry* undoMove();

  // Step forward one move.  Returns pointer to the MoveEntry to re-apply
  // (caller uses it to re-apply the move on ChessBoard).
  // Returns nullptr if at the end (canRedo() is false).
  const MoveEntry* redoMove();

  // Can undo (cursor > start)?
  bool canUndo() const { return currentIndex_ >= 0; }

  // Can redo (moves exist after cursor)?
  bool canRedo() const { return currentIndex_ < moveCount_ - 1; }

  // Current cursor position (-1 = before any move, 0 = after first move, etc.)
  int currentMoveIndex() const { return currentIndex_; }

  // --- Move log queries ---

  // Total number of moves in the log.
  int moveCount() const { return moveCount_; }

  // True when no moves have been recorded.
  bool empty() const { return moveCount_ == 0; }

  // Access the move at the given index (0 = first move).
  // Caller must ensure 0 <= index < moveCount().
  const MoveEntry& getMove(int index) const;

  // Access the move at the current cursor position.
  // Caller must ensure !empty() && currentIndex_ >= 0.
  const MoveEntry& lastMove() const;

  // --- Recording (persistent storage) ---

  // Set game header metadata and begin recording.
  // Creates the live file in storage.  No-op if storage is nullptr.
  // If called again while already recording, discards the previous
  // recording and starts fresh.
  void setHeader(const GameHeader& header);

  // Persist a FEN snapshot to storage (e.g. initial position, mid-game load).
  // No-op if not recording.
  void snapshotPosition(const std::string& fen);

  // Finalize recording — set result/winner, close storage files.
  // No-op if not recording.
  void save(GameResult result, char winnerColor);

  // Discard current recording without finalizing.
  // No-op if not recording.
  void discard();

  // Is a recording session currently active?
  bool isRecording() const { return recordingActive_; }

  // --- State queries (persistent storage) ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Header accessors ---

  // Result stored in the recording header (reflects last save/replay).
  GameResult headerResult() const { return header_.result; }
  char headerWinnerColor() const { return static_cast<char>(header_.winnerColor); }

  // --- Replay ---

  // Restore a saved game into a ChessBoard: loads the last FEN snapshot,
  // then replays all moves after it.  Populates the in-memory move log
  // with cursor at the end.  Returns true on success.
  bool replayInto(ChessBoard& board);

  // The FEN loaded during replayInto() (the position before replayed moves).
  // Empty if no replay has occurred.
  const std::string& replayFen() const { return replayFen_; }

  // --- Compact 2-byte move encoding (binary storage format) ---
  // Layout: bits 15..10 = from (row*8+col), bits 9..4 = to (row*8+col), bits 3..0 = promo code.

  static uint16_t encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion);
  static void decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

  // --- Constants ---

  static constexpr int MAX_MOVES = 300;

 private:
  // In-memory move log
  MoveEntry moves_[MAX_MOVES];
  int moveCount_;
  int currentIndex_;  // cursor: -1 = before any move

  // Recording (persistent storage)
  IGameStorage* storage_;
  ILogger* logger_;
  GameHeader header_;
  bool recordingActive_;
  bool headerInitialized_;   // true after first setHeader in this session
  uint8_t movesSinceFlush_;

  // Encode and persist a single move to storage.
  void persistMove(const MoveEntry& entry);

  // FEN loaded during the last replayInto() call.
  std::string replayFen_;
};

#endif  // CORE_CHESS_HISTORY_H
