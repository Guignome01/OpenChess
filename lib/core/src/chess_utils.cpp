#include "chess_utils.h"

#include <cctype>
#include <string>

namespace ChessUtils {

// Extract the next space-delimited token from `remaining`, advance `remaining` past it.
static std::string nextToken(std::string& remaining) {
  if (remaining.empty()) return "";
  size_t sp = remaining.find(' ');
  std::string token = (sp != std::string::npos) ? remaining.substr(0, sp) : remaining;
  remaining = (sp != std::string::npos) ? remaining.substr(sp + 1) : "";
  return token;
}

std::string boardToFEN(const char board[8][8], char currentTurn, const PositionState* state) {
  std::string fen;

  // Board position - FEN expects rank 8 (Black pieces) first, rank 1 (White pieces) last
  // Our array: row 0 = rank 8 (Black), row 7 = rank 1 (White)
  // boardToFEN loops from row 0 to row 7, so rank 8 is output first (correct for FEN)
  for (int row = 0; row < 8; row++) {
    int emptyCount = 0;
    for (int col = 0; col < 8; col++)
      if (board[row][col] == ' ') {
        emptyCount++;
      } else {
        if (emptyCount > 0) {
          fen += std::to_string(emptyCount);
          emptyCount = 0;
        }
        fen += board[row][col];
      }
    if (emptyCount > 0)
      fen += std::to_string(emptyCount);
    if (row < 7)
      fen += '/';
  }

  // Active color
  fen += ' ';
  fen += currentTurn;

  // Castling availability
  if (state != nullptr)
    fen += " " + castlingRightsToString(state->castlingRights);
  else
    fen += " KQkq";

  // En passant target square
  if (state != nullptr && state->epRow >= 0 && state->epCol >= 0) {
    fen += ' ';
    fen += fileChar(state->epCol);
    fen += rankChar(state->epRow);
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
  // Parse FEN string and update board state
  // FEN format: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

  std::string remaining = fen;
  std::string boardPart = nextToken(remaining);

  // Clear board
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      board[row][col] = ' ';

  // Parse FEN ranks (rank 8 first, rank 1 last)
  int row = 0;
  int col = 0;

  for (size_t i = 0; i < boardPart.length() && row < 8; i++) {
    char c = boardPart[i];

    if (c == '/') {
      row++;
      col = 0;
    } else if (c >= '1' && c <= '8') {
      int emptyCount = c - '0';
      col += emptyCount;
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      if (row >= 0 && row < 8 && col >= 0 && col < 8) {
        board[row][col] = c;
        col++;
      }
    }
  }
  if (state != nullptr)
    *state = PositionState{};

  // Parse active color
  std::string activeColor = nextToken(remaining);
  if (!activeColor.empty())
    currentTurn = (activeColor == "w" || activeColor == "W") ? 'w' : 'b';

  // Parse castling rights
  std::string castlingStr = nextToken(remaining);
  if (!castlingStr.empty() && state != nullptr)
    state->castlingRights = castlingRightsFromString(castlingStr);

  // Parse en passant target square
  std::string enPassantSquare = nextToken(remaining);
  if (!enPassantSquare.empty() && enPassantSquare != "-" && enPassantSquare.length() >= 2) {
    char file = enPassantSquare[0];
    char rankChar = enPassantSquare[1];

    if (file >= 'a' && file <= 'h' && rankChar >= '1' && rankChar <= '8') {
      int epCol = fileIndex(file);
      int epRow = rankIndex(rankChar);
      if (state != nullptr) {
        state->epRow = epRow;
        state->epCol = epCol;
      }
    }
  }

  // Parse halfmove clock
  std::string halfmoveStr = nextToken(remaining);
  if (!halfmoveStr.empty() && state != nullptr)
    state->halfmoveClock = std::stoi(halfmoveStr);

  // Parse fullmove number
  std::string fullmoveStr = nextToken(remaining);
  if (!fullmoveStr.empty() && state != nullptr) {
    int fullmove = std::stoi(fullmoveStr);
    state->fullmoveClock = fullmove > 0 ? fullmove : 1;
  }
}

float evaluatePosition(const char board[8][8]) {
  // Simple material evaluation
  // Positive = White advantage, negative = Black advantage
  float evaluation = 0.0f;

  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      char piece = board[row][col];
      float value = 0.0f;

      // Get piece value (uppercase = White, lowercase = Black)
      switch (tolower(piece)) {
        case 'p':
          value = 1.0f;
          break; // Pawn
        case 'n':
          value = 3.0f;
          break; // Knight
        case 'b':
          value = 3.0f;
          break; // Bishop
        case 'r':
          value = 5.0f;
          break; // Rook
        case 'q':
          value = 9.0f;
          break; // Queen
        case 'k':
          value = 0.0f;
          break; // King (not counted)
        default:
          continue; // Empty square
      }

      // Add to evaluation (positive for White, negative for Black)
      if (piece >= 'A' && piece <= 'Z')
        evaluation += value; // White piece
      else if (piece >= 'a' && piece <= 'z')
        evaluation -= value; // Black piece
    }

  return evaluation;
}

}  // namespace ChessUtils
