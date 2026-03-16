#include "librechess_provider.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

// ---------------------------------------------------------------------------
// LibreChessProvider — on-board chess engine using the core search library.
//
// Each requestMove() spawns a FreeRTOS task that:
//   1. Creates a UCIHandler with an appropriately-sized TT.
//   2. Sends "position fen <fen>" + "go depth N" (or "go movetime N").
//   3. Reads the "bestmove" response from the string buffer.
//   4. Sets the result and marks ready.
//
// The task is cooperative-cancellable via ctx->cancel → SearchLimits.stop.
// ---------------------------------------------------------------------------

LibreChessProvider::LibreChessProvider(int depth, uint32_t moveTimeMs,
                                       char playerColor, ILogger* logger)
    : EngineProvider(logger),
      depth_(depth),
      moveTimeMs_(moveTimeMs),
      playerColor_(playerColor) {}

bool LibreChessProvider::initialize(EngineInitResult& result) {
  logger_.info("LibreChessProvider: initializing on-board engine");
  logger_.infof("  depth=%d, moveTimeMs=%u", depth_, moveTimeMs_);
  result.playerColor = playerColor_;
  result.fen = "";  // Starting position
  result.gameModeId = GameModeId::BOT;
  result.depth = static_cast<uint8_t>(depth_);
  result.canResume = true;
  return true;
}

void LibreChessProvider::requestMove(const std::string& fen) {
  auto* ctx = new TaskContext();
  ctx->fen = fen;
  ctx->depth = depth_;
  ctx->moveTimeMs = moveTimeMs_;
  // 16 KiB stack: search uses ~2 KiB for MAX_MOVES arrays per ply,
  // plus ~16 KiB for SearchState (killers + history).
  spawnTask(ctx, "lcTask", taskFunction, 16384);
}

bool LibreChessProvider::checkResult(EngineResult& result) {
  if (!peekResult(result)) return false;
  if (result.type == EngineResult::MOVE)
    currentEvaluation_ = result.evaluation;
  finishTask();
  return true;
}

int LibreChessProvider::getEvaluation() {
  return currentEvaluation_;
}

// ---------------------------------------------------------------------------
// FreeRTOS task — runs the search in the background.
// ---------------------------------------------------------------------------

void LibreChessProvider::taskFunction(void* param) {
  auto* ctx = static_cast<TaskContext*>(param);

  // Size the TT based on available heap, capped at 128 KiB.
  // Each TTEntry is ≤16 bytes.  Leave at least 32 KiB free for other tasks.
  static constexpr size_t MAX_TT_BYTES = 128 * 1024;
  static constexpr size_t MIN_FREE_HEAP = 32 * 1024;
  static constexpr size_t ENTRY_SIZE = sizeof(LibreChess::search::TTEntry);

  size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t ttBytes = 0;
  if (freeHeap > MIN_FREE_HEAP) {
    ttBytes = (freeHeap - MIN_FREE_HEAP) / 4;
    if (ttBytes > MAX_TT_BYTES) ttBytes = MAX_TT_BYTES;
  }
  int ttEntries = static_cast<int>(ttBytes / ENTRY_SIZE);
  if (ttEntries < 64) ttEntries = 64;  // Minimum viable TT

  ctx->logger.infof("LibreChess: TT %d entries (%u bytes), free heap %u",
                     ttEntries, ttEntries * ENTRY_SIZE, freeHeap);

  // Create an in-process UCI handler
  LibreChess::uci::UCIHandler handler(ttEntries);
  handler.setTimeFunc([]() -> uint32_t { return millis(); });

  // Wire the cancellation flag: when ctx->cancel is set by the main
  // thread (via EngineProvider::cancelRequest), the search will see it
  // via SearchLimits::stop and unwind cooperatively.
  handler.setExternalStop(&ctx->cancel);

  LibreChess::uci::StringUCIStream stream;

  // Set up the position
  stream.addInput("position fen " + ctx->fen);

  // Build "go" command
  std::string goCmd = "go";
  if (ctx->depth > 0) goCmd += " depth " + std::to_string(ctx->depth);
  if (ctx->moveTimeMs > 0) goCmd += " movetime " + std::to_string(ctx->moveTimeMs);
  if (ctx->depth <= 0 && ctx->moveTimeMs <= 0) goCmd += " depth 6";
  stream.addInput(goCmd);
  stream.addInput("quit");

  // Run the UCI loop (processes position, go, quit)
  handler.loop(stream);

  // Extract bestmove from output
  for (const auto& line : stream.output()) {
    if (line.substr(0, 9) == "bestmove ") {
      std::string move = line.substr(9);
      // Trim any trailing text (e.g., " ponder ...")
      size_t sp = move.find(' ');
      if (sp != std::string::npos) move = move.substr(0, sp);

      ctx->result.type = EngineResult::MOVE;
      ctx->result.move = move;
      ctx->result.evaluation = 0;  // Could parse from info lines

      // Parse evaluation from the last info line
      for (const auto& info : stream.output()) {
        if (info.find("info depth") != std::string::npos) {
          size_t cpPos = info.find("score cp ");
          if (cpPos != std::string::npos) {
            ctx->result.evaluation = std::atoi(info.c_str() + cpPos + 9);
          }
        }
      }
      break;
    }
  }

  if (ctx->result.type == EngineResult::NONE) {
    ctx->logger.error("LibreChess: no bestmove found in output");
  }

  ctx->ready.store(true);
  vTaskDelete(nullptr);
}
