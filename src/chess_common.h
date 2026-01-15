#ifndef CHESS_COMMON_H
#define CHESS_COMMON_H

#include "board_driver.h"
#include "chess_engine.h"
#include "led_colors.h"
#include <Arduino.h>

namespace ChessCommon {

inline const char* colorName(char color) {
  return (color == 'w') ? "White" : "Black";
}

void copyBoard(const char src[8][8], char dst[8][8]);

uint8_t recomputeCastlingRightsFromBoard(const char board[8][8]);

void updateCastlingRightsAfterMove(uint8_t& castlingRights, int fromRow, int fromCol, int toRow, int toCol, char movedPiece, char capturedPiece);

bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol, char piece);

void applyCastling(BoardDriver* boardDriver, char board[8][8], int kingFromRow, int kingFromCol, int kingToRow, int kingToCol, char kingPiece);

bool applyPawnPromotionIfNeeded(ChessEngine* engine, char board[8][8], int toRow, int toCol, char movedPiece, char& promotedPieceOut);

// Applies user-facing side effects (Serial + LEDs/animations) based on game state.
// Returns true if the game is over (checkmate or stalemate).
bool handleGameState(BoardDriver* boardDriver, ChessEngine* engine, const char board[8][8], char colorToMove);

// Flash confirmation on a square (green, 2 times)
void confirmSquareCompletion(BoardDriver* boardDriver, int row, int col);

} // namespace ChessCommon

#endif // CHESS_COMMON_H
