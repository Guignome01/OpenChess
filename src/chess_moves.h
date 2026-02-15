#ifndef CHESS_MOVES_H
#define CHESS_MOVES_H

#include "chess_game.h"

class MoveHistory;

// ---------------------------
// Chess Game Mode Class
// ---------------------------
class ChessMoves : public ChessGame {
 public:
  ChessMoves(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, MoveHistory* mh);
  void begin() override;
  void update() override;
};

#endif // CHESS_MOVES_H
