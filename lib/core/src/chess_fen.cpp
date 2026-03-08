#include "chess_fen.h"

#include <cctype>
#include <cstring>
#include <string>

#include "chess_iterator.h"
#include "chess_utils.h"

namespace ChessFEN {

// Extract the next space-delimited token from `remaining`, advance past it.
static std::string nextToken(std::string& remaining) {
  if (remaining.empty()) return "";
  size_t sp = remaining.find(' ');
  std::string token = (sp != std::string::npos) ? remaining.substr(0, sp) : remaining;
  remaining = (sp != std::string::npos) ? remaining.substr(sp + 1) : "";
  return token;
}

std::string boardToFEN(const char board[8][8], char currentTurn, const PositionState* state) {
  std::string fen;

  // Board position — rank 8 first (row 0), rank 1 last (row 7).
  int emptyCount = 0;
  ChessIterator::forEachSquare(board, [&](int row, int col, char piece) {
    if (col == 0 && row > 0) {
      if (emptyCount > 0) {
        fen += std::to_string(emptyCount);
        emptyCount = 0;
      }
      fen += '/';
    }
    if (piece == ' ') {
      emptyCount++;
    } else {
      if (emptyCount > 0) {
        fen += std::to_string(emptyCount);
        emptyCount = 0;
      }
      fen += piece;
    }
  });
  if (emptyCount > 0)
    fen += std::to_string(emptyCount);

  // Active color
  fen += ' ';
  fen += currentTurn;

  // Castling availability
  if (state != nullptr)
    fen += " " + ChessUtils::castlingRightsToString(state->castlingRights);
  else
    fen += " KQkq";

  // En passant target square
  if (state != nullptr && state->epRow >= 0 && state->epCol >= 0) {
    fen += ' ';
    fen += ChessUtils::fileChar(state->epCol);
    fen += ChessUtils::rankChar(state->epRow);
  } else {
    fen += " -";
  }

  // Halfmove clock
  if (state != nullptr)
    fen += " " + std::to_string(state->halfmoveClock);
  else
    fen += " 0";

  // Fullmove number
  if (state != nullptr)
    fen += " " + std::to_string(state->fullmoveClock);
  else
    fen += " 1";

  return fen;
}

void fenToBoard(const std::string& fen, char board[8][8], char& currentTurn, PositionState* state) {
  std::string remaining = fen;
  std::string boardPart = nextToken(remaining);

  // Clear board
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      board[row][col] = ' ';

  // Parse ranks (rank 8 first, rank 1 last)
  int row = 0;
  int col = 0;
  for (size_t i = 0; i < boardPart.length() && row < 8; i++) {
    char c = boardPart[i];
    if (c == '/') {
      row++;
      col = 0;
    } else if (c >= '1' && c <= '8') {
      col += c - '0';
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      if (row >= 0 && row < 8 && col >= 0 && col < 8) {
        board[row][col] = c;
        col++;
      }
    }
  }
  if (state != nullptr)
    *state = PositionState{};

  // Active color
  std::string activeColor = nextToken(remaining);
  if (!activeColor.empty())
    currentTurn = (activeColor == "w" || activeColor == "W") ? 'w' : 'b';

  // Castling rights
  std::string castlingStr = nextToken(remaining);
  if (!castlingStr.empty() && state != nullptr)
    state->castlingRights = ChessUtils::castlingRightsFromString(castlingStr);

  // En passant target square
  std::string enPassantSquare = nextToken(remaining);
  if (!enPassantSquare.empty() && enPassantSquare != "-" && enPassantSquare.length() >= 2) {
    char file = enPassantSquare[0];
    char rankCh = enPassantSquare[1];
    if (file >= 'a' && file <= 'h' && rankCh >= '1' && rankCh <= '8') {
      int epCol = ChessUtils::fileIndex(file);
      int epRow = ChessUtils::rankIndex(rankCh);
      if (state != nullptr) {
        state->epRow = epRow;
        state->epCol = epCol;
      }
    }
  }

  // Halfmove clock
  std::string halfmoveStr = nextToken(remaining);
  if (!halfmoveStr.empty() && state != nullptr)
    state->halfmoveClock = std::stoi(halfmoveStr);

  // Fullmove number
  std::string fullmoveStr = nextToken(remaining);
  if (!fullmoveStr.empty() && state != nullptr) {
    int fullmove = std::stoi(fullmoveStr);
    state->fullmoveClock = fullmove > 0 ? fullmove : 1;
  }
}

bool validateFEN(const std::string& fen) {
  if (fen.empty()) return false;

  // Extract board part (before first space)
  size_t spacePos = fen.find(' ');
  std::string boardPart = (spacePos != std::string::npos) ? fen.substr(0, spacePos) : fen;

  // Must have exactly 7 '/' separators (8 ranks)
  int slashCount = 0;
  for (char c : boardPart) {
    if (c == '/') slashCount++;
  }
  if (slashCount != 7) return false;

  // Validate each rank sums to 8 and contains only valid characters
  int col = 0;
  for (char c : boardPart) {
    if (c == '/') {
      if (col != 8) return false;
      col = 0;
    } else if (c >= '1' && c <= '8') {
      col += c - '0';
    } else if (std::strchr("rnbqkpRNBQKP", c)) {
      col++;
    } else {
      return false;
    }
  }
  if (col != 8) return false;

  // Remaining fields are optional — if only the board part is given, accept it
  if (spacePos == std::string::npos || spacePos + 1 >= fen.size()) return true;

  std::string remaining = fen.substr(spacePos + 1);

  // Turn field
  std::string turnField = nextToken(remaining);
  if (turnField != "w" && turnField != "b") return false;

  // Castling field (optional from here)
  if (remaining.empty()) return true;
  std::string castlingField = nextToken(remaining);
  if (castlingField != "-") {
    for (char c : castlingField) {
      if (c != 'K' && c != 'Q' && c != 'k' && c != 'q') return false;
    }
  }

  // En passant field
  if (remaining.empty()) return true;
  std::string epField = nextToken(remaining);
  if (epField != "-") {
    if (epField.length() != 2) return false;
    if (epField[0] < 'a' || epField[0] > 'h') return false;
    if (epField[1] != '3' && epField[1] != '6') return false;
  }

  // Halfmove clock
  if (remaining.empty()) return true;
  std::string halfmoveField = nextToken(remaining);
  for (char c : halfmoveField) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }

  // Fullmove number
  if (remaining.empty()) return true;
  std::string fullmoveField = nextToken(remaining);
  if (fullmoveField.empty()) return false;
  for (char c : fullmoveField) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  if (std::stoi(fullmoveField) < 1) return false;

  return true;
}

}  // namespace ChessFEN
