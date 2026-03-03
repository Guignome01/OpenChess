#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include "base.h"

class GameController;

// Human vs human local game mode
class ChessPlayer : public ChessGame {
 public:
  ChessPlayer(BoardDriver* bd, WiFiManagerESP32* wm, GameController* gc);
  void begin() override;
  void update() override;
};

#endif // GAME_PLAYER_H
