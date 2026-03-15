#ifndef CORE_HISTORY_H
#define CORE_HISTORY_H

#include <cstdint>
#include <string>

#include "storage.h"
#include "logger.h"
#include "move.h"
#include "piece.h"
#include "types.h"

class Position;  // forward declaration for replayInto()

// ---------------------------------------------------------------------------
// MoveEntry — a single move record in the game history.
// Stores enough information to query the move log, support undo/redo,
// and reconstruct board state.
// ---------------------------------------------------------------------------
struct MoveEntry {
  int fromRow, fromCol;
  int toRow, toCol;
  Piece piece;           // piece that moved (original, before any promotion)
  Piece captured;        // piece captured (Piece::NONE if none)
  Piece promotion;       // piece promoted to (Piece::NONE if not a promotion)
  bool isCapture;
  bool isEnPassant;
  int epCapturedRow;     // en passant captured pawn row (-1 if N/A)
  bool isCastling;
  bool isPromotion;
  bool isCheck;
  PositionState prevState;  // position state before the move (enables undo)

  // Build a MoveEntry from move coordinates and result.
  static MoveEntry build(int fromRow, int fromCol, int toRow, int toCol,
                         Piece piece, Piece targetPiece,
                         const MoveResult& result,
                         const PositionState& prevState) {
    using namespace LibreChess::piece;
    Piece captured = Piece::NONE;
    if (result.isEnPassant)
      captured = makePiece(~pieceColor(piece), PieceType::PAWN);
    else if (result.isCapture)
      captured = targetPiece;

    MoveEntry entry;
    entry.fromRow = fromRow;
    entry.fromCol = fromCol;
    entry.toRow = toRow;
    entry.toCol = toCol;
    entry.piece = piece;
    entry.captured = captured;
    entry.promotion = result.isPromotion ? result.promotedTo : Piece::NONE;
    entry.isCapture = result.isCapture;
    entry.isEnPassant = result.isEnPassant;
    entry.epCapturedRow = result.epCapturedRow;
    entry.isCastling = result.isCastling;
    entry.isPromotion = result.isPromotion;
    entry.isCheck = result.isCheck;
    entry.prevState = prevState;
    return entry;
  }
};

// ---------------------------------------------------------------------------
// History — in-memory game history with optional persistent recording.
//
// Two concerns, unified under a single API:
//   1. Move log: ordered list of all moves with cursor-based undo/redo
//   2. Recording: persistent game storage (when IGameStorage is provided)
//
// Recording is automatic: if an IGameStorage* is provided and a header has
// been set (via setHeader), addMove() persists encoded moves transparently.
//
// Undo/redo: a cursor (currentIndex_) tracks the "current" position in the
// move log.  undoMove() steps back, redoMove() steps forward.  addMove()
// wipes any moves after the cursor (branch point) before appending.
//
// Nullable storage: pass nullptr to disable recording.
// ---------------------------------------------------------------------------
class History {
 public:
  History(IGameStorage* storage = nullptr, ILogger* logger = nullptr);

  // Reset in-memory history (move log + cursor).
  void clear();

  // --- Move log with undo/redo ---

  void addMove(const MoveEntry& entry);
  const MoveEntry* undoMove();
  const MoveEntry* redoMove();

  bool canUndo() const { return currentIndex_ >= 0; }
  bool canRedo() const { return currentIndex_ < moveCount_ - 1; }
  int currentMoveIndex() const { return currentIndex_; }

  // --- Move log queries ---

  int moveCount() const { return moveCount_; }
  bool empty() const { return moveCount_ == 0; }
  const MoveEntry& getMove(int index) const;
  const MoveEntry& lastMove() const;

  // --- Recording (persistent storage) ---

  void setHeader(const GameHeader& header);
  void snapshotPosition(const std::string& fen);
  void save(GameResult result, char winnerColor);
  void discard();
  bool isRecording() const { return recordingActive_; }

  // --- State queries (persistent storage) ---

  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Header accessors ---

  GameResult headerResult() const { return header_.result; }
  char headerWinnerColor() const { return static_cast<char>(header_.winnerColor); }

  // --- Replay ---

  bool replayInto(Position& board);
  const std::string& replayFen() const { return replayFen_; }

  // --- Compact 2-byte move encoding (binary storage format) ---

  static uint16_t encodeMove(int fromRow, int fromCol, int toRow, int toCol,
                              char promotion);
  static void decodeMove(uint16_t encoded, int& fromRow, int& fromCol,
                          int& toRow, int& toCol, char& promotion);

  // --- Constants ---

  static constexpr int MAX_MOVES = 300;

 private:
  MoveEntry moves_[MAX_MOVES];
  int moveCount_;
  int currentIndex_;

  IGameStorage* storage_;
  Log logger_;
  GameHeader header_;
  bool recordingActive_;
  bool headerInitialized_;
  uint8_t movesSinceFlush_;

  void persistMove(const MoveEntry& entry);
  std::string replayFen_;
};

#endif  // CORE_HISTORY_H
