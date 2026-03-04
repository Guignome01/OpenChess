#ifndef CORE_CHESS_CODEC_H
#define CORE_CHESS_CODEC_H

#include <cstdint>
#include <string>

// Encoding / decoding helpers for chess data:
//   - UCI move strings  ↔  array coordinates
//   - Castling rights bitmask  ↔  FEN string fragment
//   - Compact 2-byte move encoding (used by game recording binary format)
//
// Pure functions, no hardware dependencies.

namespace ChessCodec {

// --- UCI move encoding ---

// Convert array coordinates to a UCI move string (e.g. "e2e4", "e7e8q").
std::string toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

// Parse a UCI move string into array coordinates.
// Returns true if valid; fills promotion with the promotion char (or ' ').
bool parseUCIMove(const std::string& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

// --- Castling rights ---

// Convert castling rights bitmask (0x01=K, 0x02=Q, 0x04=k, 0x08=q) to FEN string.
std::string castlingRightsToString(uint8_t rights);

// Parse FEN castling-rights field into bitmask.
uint8_t castlingRightsFromString(const std::string& rightsStr);

// --- Compact 2-byte move encoding ---
// Layout: bits 15..10 = from (row*8+col), bits 9..4 = to (row*8+col), bits 3..0 = promo code.

uint16_t encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion);
void decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion);

}  // namespace ChessCodec

#endif  // CORE_CHESS_CODEC_H
