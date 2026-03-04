#ifndef CORE_RECORDER_H
#define CORE_RECORDER_H

#include <cstring>
#include <string>

#include "board.h"
#include "game_storage.h"
#include "logger.h"
#include "types.h"

// Pure recording orchestration — encodes moves, manages the in-memory header,
// delegates all persistence to IGameStorage.  No Arduino/hardware dependencies.
//
// Nullable: pass nullptr for storage to disable recording (e.g. Lichess mode).
class GameRecorder {
 public:
  GameRecorder(IGameStorage* storage, ILogger* logger = nullptr);

  // --- Recording lifecycle ---

  void startRecording(GameModeId mode, uint8_t playerColor = '?', uint8_t botDepth = 0);
  void recordMove(int fromRow, int fromCol, int toRow, int toCol, char promotion);
  void recordFen(const std::string& fen);
  void finishRecording(GameResult result, char winnerColor);
  void discardRecording();

  // --- State queries ---

  bool isRecording() const { return recording_; }
  bool hasActiveGame();
  bool getActiveGameInfo(GameModeId& mode, uint8_t& playerColor, uint8_t& botDepth);

  // --- Replay ---

  // Restore a saved game into a ChessBoard: loads the last FEN snapshot,
  // then replays all moves after it.  Uses beginBatch()/endBatch() to
  // suppress per-move callbacks.  Returns true on success.
  bool replayInto(ChessBoard& board);

 private:
  IGameStorage* storage_;
  ILogger* logger_;
  GameHeader header_;
  bool recording_;
  uint8_t movesSinceFlush_;
};

#endif  // CORE_RECORDER_H
