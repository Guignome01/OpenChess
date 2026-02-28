#include "utils.h"

#include "codec.h"

#include <cctype>
#include <string>

namespace ChessUtils {

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
    fen += " " + ChessCodec::castlingRightsToString(state->castlingRights);
  else
    fen += " KQkq";

  // En passant target square
  if (state != nullptr && state->epRow >= 0 && state->epCol >= 0) {
    char file = 'a' + state->epCol;
    int rank = 8 - state->epRow;
    fen += ' ';
    fen += file;
    fen += std::to_string(rank);
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

  // Split FEN into parts
  size_t firstSpace = fen.find(' ');
  std::string boardPart = (firstSpace != std::string::npos) ? fen.substr(0, firstSpace) : fen;
  std::string remainingParts = (firstSpace != std::string::npos) ? fen.substr(firstSpace + 1) : "";

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
  if (!remainingParts.empty()) {
    size_t secondSpace = remainingParts.find(' ');
    std::string activeColor = (secondSpace != std::string::npos) ? remainingParts.substr(0, secondSpace) : remainingParts;
    currentTurn = (activeColor == "w" || activeColor == "W") ? 'w' : 'b';
    remainingParts = (secondSpace != std::string::npos) ? remainingParts.substr(secondSpace + 1) : "";
  }

  // Parse castling rights
  if (!remainingParts.empty()) {
    size_t thirdSpace = remainingParts.find(' ');
    if (state != nullptr)
      state->castlingRights = ChessCodec::castlingRightsFromString((thirdSpace != std::string::npos) ? remainingParts.substr(0, thirdSpace) : remainingParts);
    remainingParts = (thirdSpace != std::string::npos) ? remainingParts.substr(thirdSpace + 1) : "";
  }

  // Parse en passant target square
  if (!remainingParts.empty()) {
    size_t fourthSpace = remainingParts.find(' ');
    std::string enPassantSquare = (fourthSpace != std::string::npos) ? remainingParts.substr(0, fourthSpace) : remainingParts;

    if (enPassantSquare != "-" && enPassantSquare.length() >= 2) {
      char file = enPassantSquare[0];
      char rankChar = enPassantSquare[1];

      if (file >= 'a' && file <= 'h' && rankChar >= '1' && rankChar <= '8') {
        int epCol = file - 'a';
        int rank = rankChar - '0';
        int epRow = 8 - rank;
        if (state != nullptr) {
          state->epRow = epRow;
          state->epCol = epCol;
        }
      }
    }
    remainingParts = (fourthSpace != std::string::npos) ? remainingParts.substr(fourthSpace + 1) : "";
  }

  // Parse halfmove clock
  if (!remainingParts.empty()) {
    size_t fifthSpace = remainingParts.find(' ');
    std::string halfmoveStr = (fifthSpace != std::string::npos) ? remainingParts.substr(0, fifthSpace) : remainingParts;
    if (state != nullptr)
      state->halfmoveClock = std::stoi(halfmoveStr);
    remainingParts = (fifthSpace != std::string::npos) ? remainingParts.substr(fifthSpace + 1) : "";
  }

  // Parse fullmove number
  if (!remainingParts.empty()) {
    int fullmove = std::stoi(remainingParts);
    if (state != nullptr)
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
