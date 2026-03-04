#ifndef CORE_GAME_OBSERVER_H
#define CORE_GAME_OBSERVER_H

#include <string>

// Abstract observer interface — notified by ChessGame after every board
// mutation (makeMove, loadFEN, newGame, endGame).  Allows the core library to
// push state changes to the firmware layer (e.g. web UI) without depending on
// any concrete implementation.
//
// WiFiManagerESP32 implements this to keep the web dashboard in sync.
class IGameObserver {
 public:
  virtual ~IGameObserver() = default;

  // Called whenever the board state changes.
  virtual void onBoardStateChanged(const std::string& fen, float evaluation) = 0;
};

#endif  // CORE_GAME_OBSERVER_H
