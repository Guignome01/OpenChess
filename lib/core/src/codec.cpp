#include "codec.h"

#include <cctype>

namespace ChessCodec {

// --- UCI move encoding ---

std::string toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  std::string move;
  move += (char)('a' + fromCol);
  move += (char)('0' + (8 - fromRow));
  move += (char)('a' + toCol);
  move += (char)('0' + (8 - toRow));

  if (promotion != ' ' && promotion != '\0') {
    move += (char)tolower(promotion);
  }

  return move;
}

bool parseUCIMove(const std::string& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
  size_t len = move.length();
  if (len < 4 || len > 5) return false;

  char fromFile = move[0];
  char fromRank = move[1];
  char toFile = move[2];
  char toRank = move[3];

  if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') return false;
  if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') return false;

  fromCol = fromFile - 'a';
  fromRow = 8 - (fromRank - '0');
  toCol = toFile - 'a';
  toRow = 8 - (toRank - '0');

  if (fromRow < 0 || fromRow > 7 || fromCol < 0 || fromCol > 7 || toRow < 0 || toRow > 7 || toCol < 0 || toCol > 7) return false;

  // From and to squares must differ
  if (fromRow == toRow && fromCol == toCol) return false;

  // Promotion (optional 5th char) must be q, r, b, or n
  promotion = ' ';
  if (len == 5) {
    char promo = tolower(move[4]);
    if (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n') return false;
    promotion = move[4];
  }

  return true;
}

// --- Castling rights ---

std::string castlingRightsToString(uint8_t rights) {
  std::string s;
  if (rights & 0x01) s += 'K';
  if (rights & 0x02) s += 'Q';
  if (rights & 0x04) s += 'k';
  if (rights & 0x08) s += 'q';
  if (s.empty()) s = "-";
  return s;
}

uint8_t castlingRightsFromString(const std::string& rightsStr) {
  uint8_t rights = 0;
  for (size_t i = 0; i < rightsStr.length(); i++) {
    char c = rightsStr[i];
    switch (c) {
      case 'K':
        rights |= 0x01;
        break;
      case 'Q':
        rights |= 0x02;
        break;
      case 'k':
        rights |= 0x04;
        break;
      case 'q':
        rights |= 0x08;
        break;
      default:
        break;
    }
  }
  return rights;
}

// --- Compact 2-byte move encoding ---

static uint8_t promoCharToCode(char p) {
  switch (tolower(p)) {
    case 'q': return 1;
    case 'r': return 2;
    case 'b': return 3;
    case 'n': return 4;
    default:  return 0;
  }
}

static char promoCodeToChar(uint8_t code) {
  switch (code) {
    case 1: return 'q';
    case 2: return 'r';
    case 3: return 'b';
    case 4: return 'n';
    default: return ' ';
  }
}

uint16_t encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  uint8_t from = (uint8_t)(fromRow * 8 + fromCol);
  uint8_t to = (uint8_t)(toRow * 8 + toCol);
  uint8_t promo = promoCharToCode(promotion);
  return (uint16_t)((from << 10) | (to << 4) | promo);
}

void decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
  uint8_t from = (encoded >> 10) & 0x3F;
  uint8_t to = (encoded >> 4) & 0x3F;
  uint8_t promo = encoded & 0x0F;
  fromRow = from / 8;
  fromCol = from % 8;
  toRow = to / 8;
  toCol = to % 8;
  promotion = promoCodeToChar(promo);
}

}  // namespace ChessCodec
