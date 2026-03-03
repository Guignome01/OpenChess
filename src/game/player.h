#ifndef PLAYER_H
#define PLAYER_H

#include "base.h"

class GameController;

// Human vs human local game mode
class ChessPlayer : public ChessGame {
 public:
  ChessPlayer(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc);
  void begin() override;
  void update() override;
};

#endif // PLAYER_H
