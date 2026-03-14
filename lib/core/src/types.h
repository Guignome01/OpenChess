#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstdint>

#include "chess_bitboard.h"
#include "chess_piece.h"

// Complete position state for chess operations.
// ChessRules is stateless; the caller supplies a PositionState for
// position-dependent queries (castling rights, en passant target).
// ChessBoard owns the authoritative instance and also uses
// halfmoveClock / fullmoveClock for FEN serialization and draw detection.
struct PositionState {
  uint8_t castlingRights = 0x0F;  // KQkq bitmask (bits 0-3)
  int epRow = -1;                 // en passant target row (-1 if none)
  int epCol = -1;                 // en passant target col (-1 if none)
  int halfmoveClock = 0;          // half-move clock (50-move rule)
  int fullmoveClock = 1;          // full-move counter (starts at 1)

  // Standard starting position state (identical to default construction).
  static PositionState initial() { return {}; }
};

// Game result codes — stored in game recording binary format.
// Values 0–5 match the existing on-disk header (FORMAT_VERSION 1).
// New values are appended so older files remain readable.
enum class GameResult : uint8_t {
  IN_PROGRESS = 0,
  CHECKMATE = 1,
  STALEMATE = 2,
  DRAW_50 = 3,
  DRAW_3FOLD = 4,
  RESIGNATION = 5,
  DRAW_INSUFFICIENT = 6,
  DRAW_AGREEMENT = 7,
  TIMEOUT = 8,
  ABORTED = 9
};

// Carries all information from a completed move so the hardware/UI layer
// can produce LED feedback, sounds, and network updates without re-reading
// game state.
struct MoveResult {
  bool valid;           // Was the move legal?
  bool isCapture;       // A piece was captured
  bool isEnPassant;     // En passant capture
  int epCapturedRow;    // Row of captured en passant pawn (-1 if N/A)
  bool isCastling;      // Castling move
  bool isPromotion;     // Pawn promotion occurred
  Piece promotedTo;     // Piece the pawn became (Piece::NONE if N/A)
  bool isCheck;         // Move puts opponent in check
  GameResult gameResult; // GameResult::IN_PROGRESS if game continues
  char winnerColor;     // 'w', 'b', 'd' (draw), ' ' (in progress)
};

// Factory for an invalid (rejected) MoveResult.
constexpr MoveResult invalidMoveResult() {
  return {false, false, false, -1, false, false, Piece::NONE, false, GameResult::IN_PROGRESS, ' '};
}

// ---------------------------------------------------------------------------
// Game recording types — used by ChessHistory and IGameStorage.
// ---------------------------------------------------------------------------

// Game mode identifiers stored in the binary game header.
enum class GameModeId : uint8_t {
  NONE = 0,
  CHESS_MOVES = 1,
  BOT = 2,
  LICHESS = 3
};

// Binary file header for recorded games (on-disk format).
struct __attribute__((packed)) GameHeader {
  uint8_t version;        // Format version (currently 1)
  GameModeId mode;          // Game mode identifier
  GameResult result;      // Game outcome
  uint8_t winnerColor;    // 'w', 'b', 'd' (draw), '?' (in-progress)
  uint8_t playerColor;    // For bot mode: human's color ('w'/'b'), '?' for ChessMoves
  uint8_t botDepth;       // For bot mode: Stockfish depth, 0 for ChessMoves
  uint16_t moveCount;     // Number of 2-byte entries (incl. FEN markers)
  uint16_t fenEntryCnt;   // Number of FEN table entries
  uint16_t lastFenOffset; // Byte offset of the last FEN entry within the FEN table
  uint32_t timestamp;     // Unix epoch (from NTP, 0 if unavailable)
};
static_assert(sizeof(GameHeader) == 16, "GameHeader must be 16 bytes");

// Recording constants
static constexpr uint8_t FORMAT_VERSION = 1;
static constexpr uint16_t FEN_MARKER = 0xFFFF;
static constexpr int MAX_GAMES = 50;
static constexpr float MAX_USAGE_PERCENT = 0.80f;

// Maximum number of pseudo-legal moves a single piece can have.
// Queen on open board: 7+7+7+7 = 28 (4 diagonals + 4 straights, max 7 each).
static constexpr int MAX_MOVES_PER_PIECE = 28;

// Fixed-capacity list of target squares for a single piece's moves.
// Stores LERF square indices internally; exposes row(i)/col(i) for
// firmware compatibility and square(i) for internal bitboard code.
struct MoveList {
  int count = 0;

  void add(int r, int c) {
    targets_[count++] = ChessBitboard::squareOf(r, c);
  }
  void addSquare(ChessBitboard::Square sq) {
    targets_[count++] = sq;
  }
  int row(int i) const { return ChessBitboard::rowOf(targets_[i]); }
  int col(int i) const { return ChessBitboard::colOf(targets_[i]); }
  ChessBitboard::Square square(int i) const { return targets_[i]; }
  void clear() { count = 0; }

 private:
  ChessBitboard::Square targets_[MAX_MOVES_PER_PIECE];
};

// Fixed-capacity Zobrist hash history for threefold repetition detection.
// Replaces the coupled (uint64_t*, int) parameter pattern.
struct HashHistory {
  static constexpr int MAX_SIZE = 256;

  uint64_t keys[MAX_SIZE];
  int count = 0;
};

// Move notation format identifiers — used by ChessGame::getHistory().
enum class MoveFormat : uint8_t {
  COORDINATE = 0,  // "e2e4", "e7e8q"  (UCI protocol notation)
  SAN = 1,         // "e4", "Nxf3", "O-O", "e8=Q+"  (Standard Algebraic)
  LAN = 2          // "e2-e4", "Ng1xf3", "O-O", "e7-e8=Q+"  (Long Algebraic)
};

#endif  // CORE_TYPES_H
