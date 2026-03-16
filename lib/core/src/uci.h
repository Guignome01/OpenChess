#ifndef LIBRECHESS_UCI_H
#define LIBRECHESS_UCI_H

// ---------------------------------------------------------------------------
// UCI protocol handler — transport-agnostic.
//
// UCIStream is the abstract I/O interface (Serial, string buffer, etc.).
// UCIHandler owns the engine state (Position, TT) and dispatches commands.
//
// Two usage modes:
//   1. loop(stream) — blocking read/dispatch loop for standalone UCI mode.
//   2. processCommand(line, out) — single-command dispatch for testing and
//      in-process integration (LibreChessProvider).
//
// All UCI output goes through UCIStream::writeLine(), never to stdout.
//
// Reference: https://www.chessprogramming.org/UCI
// ---------------------------------------------------------------------------

#include <atomic>
#include <string>
#include <vector>

#include "position.h"
#include "search.h"

namespace LibreChess {
namespace uci {

// ---------------------------------------------------------------------------
// UCIStream — abstract line-based I/O.
//
// Firmware implements this over Serial; tests use a string buffer.
// ---------------------------------------------------------------------------

class UCIStream {
 public:
  virtual ~UCIStream() = default;
  virtual std::string readLine() = 0;
  virtual void writeLine(const std::string& line) = 0;
};

// ---------------------------------------------------------------------------
// UCIHandler — engine controller that speaks the UCI protocol.
//
// Owns a Position, TranspositionTable, and stop flag.  The search runs
// synchronously inside processCommand("go ...") — the caller is responsible
// for threading if non-blocking behavior is needed.
// ---------------------------------------------------------------------------

class UCIHandler {
 public:
  // Construct with optional TT size (number of entries).
  explicit UCIHandler(int ttSize = search::DEFAULT_TT_SIZE);
  ~UCIHandler();

  // Blocking read/dispatch loop.  Returns when "quit" is received.
  void loop(UCIStream& stream);

  // Process a single UCI command line.  Returns false on "quit".
  bool processCommand(const std::string& line, UCIStream& out);

  // Read-only access to the internal position (for testing).
  const Position& position() const { return pos_; }

  // Set the time function (firmware passes millis(), tests pass a mock).
  void setTimeFunc(search::TimeFunc fn) { timeFunc_ = fn; }

  // Signal the engine to stop searching (thread-safe).
  void stop() { stop_.store(true, std::memory_order_relaxed); }

  // Wire an external stop flag (e.g. ctx->cancel in EngineProvider).
  // When set, handleGo() will use this flag for SearchLimits::stop
  // instead of the internal one.  The caller must ensure the pointer
  // outlives any in-progress search.
  void setExternalStop(std::atomic<bool>* flag) { externalStop_ = flag; }

 private:
  Position pos_;
  search::TranspositionTable tt_;
  std::atomic<bool> stop_{false};
  std::atomic<bool>* externalStop_ = nullptr;
  search::TimeFunc timeFunc_ = nullptr;
  search::SearchResult lastResult_;

  // Command handlers
  void handleUci(UCIStream& out);
  void handleIsReady(UCIStream& out);
  void handleNewGame();
  void handlePosition(const std::string& args);
  void handleGo(const std::string& args, UCIStream& out);

  // Apply a UCI coordinate move string (e.g. "e2e4", "e7e8q") to pos_.
  // Returns false if the move is illegal or unparseable.
  bool applyMoveString(const std::string& moveStr);
};

// ---------------------------------------------------------------------------
// StringUCIStream — in-memory stream for testing and in-process use.
// ---------------------------------------------------------------------------

class StringUCIStream : public UCIStream {
 public:
  void addInput(const std::string& line) { input_.push_back(line); }

  std::string readLine() override {
    if (readIdx_ < static_cast<int>(input_.size()))
      return input_[readIdx_++];
    return "quit";  // Auto-quit when input exhausted
  }

  void writeLine(const std::string& line) override {
    output_.push_back(line);
  }

  const std::vector<std::string>& output() const { return output_; }
  void clearOutput() { output_.clear(); }

 private:
  std::vector<std::string> input_;
  std::vector<std::string> output_;
  int readIdx_ = 0;
};

}  // namespace uci
}  // namespace LibreChess

#endif  // LIBRECHESS_UCI_H
