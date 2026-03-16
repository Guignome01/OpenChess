#include "uci.h"

#include <sstream>

#include "fen.h"
#include "movegen.h"
#include "notation.h"
#include "piece.h"

// ---------------------------------------------------------------------------
// UCI protocol handler implementation.
//
// Command parsing follows the UCI specification:
//   https://www.chessprogramming.org/UCI
//
// The search runs synchronously inside handleGo().  Threading is the
// caller's responsibility (e.g. FreeRTOS task in LibreChessProvider).
// ---------------------------------------------------------------------------

namespace LibreChess {
namespace uci {

using namespace piece;

// ===========================================================================
// Construction / destruction
// ===========================================================================

UCIHandler::UCIHandler(int ttSize) {
  tt_.resize(ttSize);
  pos_.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

UCIHandler::~UCIHandler() {
  tt_.free();
}

// ===========================================================================
// Main loop
// ===========================================================================

void UCIHandler::loop(UCIStream& stream) {
  while (true) {
    std::string line = stream.readLine();
    if (!processCommand(line, stream)) return;
  }
}

bool UCIHandler::processCommand(const std::string& line, UCIStream& out) {
  if (line.empty()) return true;

  // Extract the first token (command name)
  std::istringstream iss(line);
  std::string cmd;
  iss >> cmd;

  if (cmd == "uci")         handleUci(out);
  else if (cmd == "isready") handleIsReady(out);
  else if (cmd == "ucinewgame") handleNewGame();
  else if (cmd == "position") handlePosition(line.substr(cmd.size()));
  else if (cmd == "go")      handleGo(line.substr(cmd.size()), out);
  else if (cmd == "stop")    stop();
  else if (cmd == "quit")    return false;
  // Unknown commands are silently ignored per UCI spec.
  return true;
}

// ===========================================================================
// Command handlers
// ===========================================================================

void UCIHandler::handleUci(UCIStream& out) {
  out.writeLine("id name LibreChess");
  out.writeLine("id author LibreChess Contributors");
  // Options could be added here (e.g. Hash size, Threads)
  out.writeLine("uciok");
}

void UCIHandler::handleIsReady(UCIStream& out) {
  out.writeLine("readyok");
}

void UCIHandler::handleNewGame() {
  tt_.clear();
  pos_.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

// ---------------------------------------------------------------------------
// position startpos [moves e2e4 e7e5 ...]
// position fen <fen> [moves e2e4 e7e5 ...]
// ---------------------------------------------------------------------------

void UCIHandler::handlePosition(const std::string& args) {
  std::istringstream iss(args);
  std::string token;
  iss >> token;

  if (token == "startpos") {
    pos_.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    iss >> token;  // Consume "moves" if present
  } else if (token == "fen") {
    // Collect all FEN tokens (up to 6 parts) until "moves" or end
    std::string fenStr;
    while (iss >> token) {
      if (token == "moves") break;
      if (!fenStr.empty()) fenStr += ' ';
      fenStr += token;
    }
    pos_.loadFEN(fenStr);
    // If we hit "moves", token is already "moves" and we fall through
  }

  // Apply move list if "moves" was found
  if (token == "moves") {
    while (iss >> token) {
      applyMoveString(token);
    }
  }
}

// ---------------------------------------------------------------------------
// go [depth N] [movetime N] [wtime N btime N [winc N binc N]] [infinite]
// ---------------------------------------------------------------------------

void UCIHandler::handleGo(const std::string& args, UCIStream& out) {
  search::SearchLimits limits;
  stop_.store(false, std::memory_order_relaxed);
  // Use the external stop flag if wired (e.g. FreeRTOS ctx->cancel),
  // otherwise fall back to the internal flag.
  limits.stop = externalStop_ ? externalStop_ : &stop_;

  std::istringstream iss(args);
  std::string token;
  int wtime = 0, btime = 0, winc = 0, binc = 0;
  bool hasGameClock = false;

  while (iss >> token) {
    if (token == "depth") {
      int d = 0;
      iss >> d;
      limits.maxDepth = d;
    } else if (token == "movetime") {
      uint32_t t = 0;
      iss >> t;
      limits.maxTimeMs = t;
    } else if (token == "wtime") {
      iss >> wtime;
      hasGameClock = true;
    } else if (token == "btime") {
      iss >> btime;
      hasGameClock = true;
    } else if (token == "winc") {
      iss >> winc;
    } else if (token == "binc") {
      iss >> binc;
    } else if (token == "infinite") {
      limits.maxDepth = search::MAX_PLY;
      limits.maxTimeMs = 0;
    }
  }

  // Simple time management from game clock: use 1/30th of remaining time + increment
  if (hasGameClock && limits.maxTimeMs == 0) {
    int myTime = (pos_.sideToMove() == Color::WHITE) ? wtime : btime;
    int myInc  = (pos_.sideToMove() == Color::WHITE) ? winc : binc;
    if (myTime > 0) {
      limits.maxTimeMs = static_cast<uint32_t>(myTime / 30 + myInc / 2);
      if (limits.maxTimeMs < 10) limits.maxTimeMs = 10;
    }
  }

  // Info callback: emit "info depth ... score cp ... nodes ..."
  // Use a static pointer to the stream for the C-style callback.
  static UCIStream* infoStream = nullptr;
  static search::TimeFunc infoTimeFunc = nullptr;
  static uint32_t infoStartTime = 0;
  infoStream = &out;
  infoTimeFunc = timeFunc_;
  infoStartTime = timeFunc_ ? timeFunc_() : 0;

  auto infoCallback = [](const search::SearchResult& r) {
    if (!infoStream) return;
    std::string line = "info depth " + std::to_string(r.depth) +
                       " score cp " + std::to_string(r.score) +
                       " nodes " + std::to_string(r.nodes);
    if (infoTimeFunc) {
      uint32_t elapsed = infoTimeFunc() - infoStartTime;
      line += " time " + std::to_string(elapsed);
      if (elapsed > 0)
        line += " nps " + std::to_string(r.nodes * 1000ULL / elapsed);
    }
    infoStream->writeLine(line);
  };

  lastResult_ = search::findBestMove(pos_, limits, timeFunc_,
                                      infoCallback, &tt_);

  // Emit bestmove
  if (lastResult_.bestMove.from == 0 && lastResult_.bestMove.to == 0) {
    out.writeLine("bestmove 0000");  // No legal moves (shouldn't happen)
  } else {
    std::string moveStr = notation::toCoordinate(
        rowOf(lastResult_.bestMove.from), colOf(lastResult_.bestMove.from),
        rowOf(lastResult_.bestMove.to), colOf(lastResult_.bestMove.to));
    if (lastResult_.bestMove.isPromotion()) {
      static const char promoChars[] = {'n', 'b', 'r', 'q'};
      moveStr += promoChars[lastResult_.bestMove.promoIndex()];
    }
    out.writeLine("bestmove " + moveStr);
  }
}

// ===========================================================================
// Move application helper
// ===========================================================================

bool UCIHandler::applyMoveString(const std::string& moveStr) {
  int fromRow, fromCol, toRow, toCol;
  char promo = ' ';
  if (!notation::parseCoordinate(moveStr, fromRow, fromCol, toRow, toCol, promo))
    return false;

  // Find the matching legal move (to get correct flags like capture, EP, etc.)
  Square from = squareOf(fromRow, fromCol);
  Square to   = squareOf(toRow, toCol);

  MoveList moves;
  movegen::generateAllMoves(pos_.bitboards(), pos_.mailbox(),
                            pos_.sideToMove(), pos_.positionState(), moves);

  for (int i = 0; i < moves.count; ++i) {
    const Move& m = moves.moves[i];
    if (m.from == from && m.to == to) {
      // For promotions, match the promotion piece
      if (m.isPromotion()) {
        PieceType pt = m.promoTypeFromIndex(m.promoIndex());
        char promoChar = ' ';
        switch (pt) {
          case PieceType::KNIGHT: promoChar = 'n'; break;
          case PieceType::BISHOP: promoChar = 'b'; break;
          case PieceType::ROOK:   promoChar = 'r'; break;
          case PieceType::QUEEN:  promoChar = 'q'; break;
          default: break;
        }
        if (std::tolower(promo) != promoChar) continue;
      }
      pos_.make(m);
      return true;
    }
  }
  return false;  // No matching legal move found
}

}  // namespace uci
}  // namespace LibreChess
