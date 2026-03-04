#include "history.h"

ChessHistory::ChessHistory() : moveCount_(0), positionHistoryCount_(0) {}

void ChessHistory::clear() {
  moveCount_ = 0;
  positionHistoryCount_ = 0;
}

// ---------------------------------------------------------------------------
// Move log
// ---------------------------------------------------------------------------

void ChessHistory::addMove(const MoveEntry& entry) {
  if (moveCount_ < MAX_MOVES) {
    moves_[moveCount_++] = entry;
  }
}

bool ChessHistory::popMove(MoveEntry& entry) {
  if (moveCount_ <= 0) return false;
  entry = moves_[--moveCount_];
  return true;
}

const MoveEntry& ChessHistory::getMove(int index) const {
  return moves_[index];
}

const MoveEntry& ChessHistory::lastMove() const {
  return moves_[moveCount_ - 1];
}

// ---------------------------------------------------------------------------
// Position hash tracking (Zobrist-based threefold repetition)
// ---------------------------------------------------------------------------

void ChessHistory::recordPosition(uint64_t hash) {
  if (positionHistoryCount_ < MAX_POSITION_HISTORY) {
    positionHistory_[positionHistoryCount_++] = hash;
  }
}

void ChessHistory::resetPositionHistory() {
  positionHistoryCount_ = 0;
}

bool ChessHistory::isThreefoldRepetition() const {
  // Minimum 5 half-moves for 3 occurrences of the same position
  if (positionHistoryCount_ < 5) return false;

  uint64_t current = positionHistory_[positionHistoryCount_ - 1];
  int count = 1;  // current position counts as 1

  // Scan backwards, skipping every other entry (only same side-to-move
  // can match). Backwards scan finds recent repetitions faster.
  for (int i = positionHistoryCount_ - 3; i >= 0; i -= 2) {
    if (positionHistory_[i] == current) {
      count++;
      if (count >= 3) return true;
    }
  }
  return false;
}
