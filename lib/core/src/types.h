#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstdint>

#include "chess_bitboard.h"

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

// Move flag bits — packed into a single uint8_t.
static constexpr uint8_t MOVE_CAPTURE   = 1 << 0;
static constexpr uint8_t MOVE_EP        = 1 << 1;
static constexpr uint8_t MOVE_CASTLING  = 1 << 2;
static constexpr uint8_t MOVE_PROMOTION = 1 << 3;
// Bits 4-5: promotion piece type (only meaningful when MOVE_PROMOTION is set).
static constexpr int MOVE_PROMO_SHIFT = 4;

// Compact move: from-square, to-square, flags byte. 3 bytes + 1 padding.
struct Move {
  uint8_t from;
  uint8_t to;
  uint8_t flags;

  constexpr Move() : from(0), to(0), flags(0) {}
  constexpr Move(uint8_t f, uint8_t t, uint8_t fl = 0) : from(f), to(t), flags(fl) {}

  constexpr bool isCapture()   const { return flags & MOVE_CAPTURE; }
  constexpr bool isEP()        const { return flags & MOVE_EP; }
  constexpr bool isCastling()  const { return flags & MOVE_CASTLING; }
  constexpr bool isPromotion() const { return flags & MOVE_PROMOTION; }

  // Promotion piece encoding: 0=Knight, 1=Bishop, 2=Rook, 3=Queen.
  constexpr uint8_t promoIndex() const { return (flags >> MOVE_PROMO_SHIFT) & 0x03; }

  // Build promotion flags: MOVE_PROMOTION | (index << MOVE_PROMO_SHIFT).
  static constexpr uint8_t promoFlags(uint8_t index) {
    return MOVE_PROMOTION | (index << MOVE_PROMO_SHIFT);
  }

  // Map PieceType (KNIGHT..QUEEN) to 2-bit promotion index.
  static constexpr uint8_t promoIndexFromType(PieceType pt) {
    return static_cast<uint8_t>(pt) - static_cast<uint8_t>(PieceType::KNIGHT);
  }

  // Map 2-bit promotion index back to PieceType.
  static constexpr PieceType promoTypeFromIndex(uint8_t idx) {
    return static_cast<PieceType>(idx + static_cast<uint8_t>(PieceType::KNIGHT));
  }

  constexpr bool operator==(const Move& o) const {
    return from == o.from && to == o.to && flags == o.flags;
  }
};

// Move with an attached score for move ordering (MVV-LVA, killers, history).
struct ScoredMove {
  Move move;
  int16_t score = 0;
};

// Unified move list — used for both per-piece and full-position generation.
// Stores Move structs with from/to/flags. Capacity 218 = theoretical max legal moves.
static constexpr int MAX_MOVES = 218;

struct MoveList {
  Move moves[MAX_MOVES];
  int count = 0;

  void add(Move m) { moves[count++] = m; }
  void clear() { count = 0; }

  // UI adapter — extract target coordinates for LED/sensor code
  int targetRow(int i) const { return ChessBitboard::rowOf(moves[i].to); }
  int targetCol(int i) const { return ChessBitboard::colOf(moves[i].to); }
  ChessBitboard::Square target(int i) const { return static_cast<ChessBitboard::Square>(moves[i].to); }
};

// Fixed-capacity Zobrist hash history for threefold repetition detection.
// Replaces the coupled (uint64_t*, int) parameter pattern.
struct HashHistory {
  static constexpr int MAX_SIZE = 512;

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
