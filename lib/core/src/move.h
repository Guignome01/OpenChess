#ifndef LIBRECHESS_MOVE_H
#define LIBRECHESS_MOVE_H

// Move representation for the LibreChess chess library.
//
// Defines the compact Move struct (from/to/flags), move flag constants,
// ScoredMove (for move ordering), MoveList (fixed-capacity generation
// output), and MoveResult (UI feedback after a completed move).

#include <cstdint>

#include "square.h"
#include "types.h"

namespace LibreChess {

// ---------------------------------------------------------------------------
// Move flag bits — packed into a single uint8_t.
// ---------------------------------------------------------------------------

static constexpr uint8_t MOVE_CAPTURE   = 1 << 0;
static constexpr uint8_t MOVE_EP        = 1 << 1;
static constexpr uint8_t MOVE_CASTLING  = 1 << 2;
static constexpr uint8_t MOVE_PROMOTION = 1 << 3;
// Bits 4-5: promotion piece type (only meaningful when MOVE_PROMOTION is set).
static constexpr int MOVE_PROMO_SHIFT = 4;

// ---------------------------------------------------------------------------
// Compact move: from-square, to-square, flags byte. 3 bytes + 1 padding.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Move with an attached score for move ordering (MVV-LVA, killers, history).
// ---------------------------------------------------------------------------

struct ScoredMove {
  Move move;
  int16_t score = 0;
};

// ---------------------------------------------------------------------------
// Unified move list — used for both per-piece and full-position generation.
// Capacity 218 = theoretical max legal moves.
// ---------------------------------------------------------------------------

static constexpr int MAX_MOVES = 218;

struct MoveList {
  Move moves[MAX_MOVES];
  int count = 0;

  void add(Move m) { moves[count++] = m; }
  void clear() { count = 0; }

  // UI adapter — extract target coordinates for LED/sensor code.
  int targetRow(int i) const { return rowOf(moves[i].to); }
  int targetCol(int i) const { return colOf(moves[i].to); }
  Square target(int i) const { return static_cast<Square>(moves[i].to); }
};

// ---------------------------------------------------------------------------
// MoveResult — carries all information from a completed move so the
// hardware/UI layer can produce LED feedback, sounds, and network updates
// without re-reading game state.
// ---------------------------------------------------------------------------

struct MoveResult {
  bool valid;            // Was the move legal?
  bool isCapture;        // A piece was captured
  bool isEnPassant;      // En passant capture
  int epCapturedRow;     // Row of captured en passant pawn (-1 if N/A)
  bool isCastling;       // Castling move
  bool isPromotion;      // Pawn promotion occurred
  Piece promotedTo;      // Piece the pawn became (Piece::NONE if N/A)
  bool isCheck;          // Move puts opponent in check
  GameResult gameResult; // GameResult::IN_PROGRESS if game continues
  char winnerColor;      // 'w', 'b', 'd' (draw), ' ' (in progress)
};

// Factory for an invalid (rejected) MoveResult.
constexpr MoveResult invalidMoveResult() {
  return {false, false, false, -1, false, false, Piece::NONE, false, GameResult::IN_PROGRESS, ' '};
}

}  // namespace LibreChess

#endif  // LIBRECHESS_MOVE_H
