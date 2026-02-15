#include "chess_utils.h"
#include "chess_engine.h"

extern "C" {
#include "nvs_flash.h"
}

String ChessUtils::castlingRightsToString(uint8_t rights) {
  String s = "";
  if (rights & 0x01) s += "K";
  if (rights & 0x02) s += "Q";
  if (rights & 0x04) s += "k";
  if (rights & 0x08) s += "q";
  if (s.length() == 0) s = "-";
  return s;
}

uint8_t ChessUtils::castlingRightsFromString(const String& rightsStr) {
  uint8_t rights = 0;
  for (size_t i = 0; i < rightsStr.length(); i++) {
    char c = rightsStr.charAt(i);
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

String ChessUtils::boardToFEN(const char board[8][8], char currentTurn, ChessEngine* chessEngine) {
  String fen = "";

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
          fen += String(emptyCount);
          emptyCount = 0;
        }
        fen += board[row][col];
      }
    if (emptyCount > 0)
      fen += String(emptyCount);
    if (row < 7)
      fen += "/";
  }

  // Active color
  fen += " " + String(currentTurn);

  // Castling availability
  if (chessEngine != nullptr)
    fen += " " + ChessUtils::castlingRightsToString(chessEngine->getCastlingRights());
  else
    fen += " KQkq";

  // En passant target square
  if (chessEngine != nullptr && chessEngine->hasEnPassantTarget()) {
    int epRow, epCol;
    chessEngine->getEnPassantTarget(epRow, epCol);
    // Convert row/col to algebraic notation (e.g., e3, e6)
    char file = 'a' + epCol;
    int rank = 8 - epRow;
    fen += " " + String(file) + String(rank);
  } else {
    fen += " -";
  }

  // Halfmove clock
  if (chessEngine != nullptr)
    fen += " " + String(chessEngine->getHalfmoveClock());
  else
    fen += " 0";

  // Fullmove number
  if (chessEngine != nullptr)
    fen += " " + String(chessEngine->getFullmoveClock());
  else
    fen += " 1";

  return fen;
}

void ChessUtils::fenToBoard(const String& fen, char board[8][8], char& currentTurn, ChessEngine* chessEngine) {
  // Parse FEN string and update board state
  // FEN format: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

  // Split FEN into parts
  int firstSpace = fen.indexOf(' ');
  String boardPart = (firstSpace > 0) ? fen.substring(0, firstSpace) : fen;
  String remainingParts = (firstSpace > 0) ? fen.substring(firstSpace + 1) : "";

  // Clear board
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      board[row][col] = ' ';

  // Parse FEN ranks (rank 8 first, rank 1 last)
  // FEN: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
  // Our array: row 0 = rank 8 (Black), row 7 = rank 1 (White)
  int row = 0; // Start with rank 8 (row 0 in our array)
  int col = 0;

  for (size_t i = 0; i < boardPart.length() && row < 8; i++) {
    char c = boardPart.charAt(i);

    if (c == '/') {
      // Next rank
      row++;
      col = 0;
    } else if (c >= '1' && c <= '8') {
      // Empty squares
      int emptyCount = c - '0';
      col += emptyCount;
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      // Piece
      if (row >= 0 && row < 8 && col >= 0 && col < 8) {
        board[row][col] = c;
        col++;
      }
    }
  }
  if (chessEngine != nullptr)
    chessEngine->reset();

  // Parse active color
  if (remainingParts.length() > 0) {
    int secondSpace = remainingParts.indexOf(' ');
    String activeColor = (secondSpace > 0) ? remainingParts.substring(0, secondSpace) : remainingParts;
    currentTurn = (activeColor == "w" || activeColor == "W") ? 'w' : 'b';
    remainingParts = (secondSpace > 0) ? remainingParts.substring(secondSpace + 1) : "";
  }

  // Parse castling rights
  if (remainingParts.length() > 0) {
    int thirdSpace = remainingParts.indexOf(' ');
    if (chessEngine != nullptr)
      chessEngine->setCastlingRights(castlingRightsFromString((thirdSpace > 0) ? remainingParts.substring(0, thirdSpace) : remainingParts));
    remainingParts = (thirdSpace > 0) ? remainingParts.substring(thirdSpace + 1) : "";
  }

  // Parse en passant target square
  if (remainingParts.length() > 0) {
    int fourthSpace = remainingParts.indexOf(' ');
    String enPassantSquare = (fourthSpace > 0) ? remainingParts.substring(0, fourthSpace) : remainingParts;

    if (enPassantSquare != "-" && enPassantSquare.length() >= 2) {
      // Parse algebraic notation (e.g., "e3", "e6")
      char file = enPassantSquare.charAt(0);
      char rankChar = enPassantSquare.charAt(1);

      if (file >= 'a' && file <= 'h' && rankChar >= '1' && rankChar <= '8') {
        int epCol = file - 'a';
        int rank = rankChar - '0';
        int epRow = 8 - rank;
        if (chessEngine != nullptr)
          chessEngine->setEnPassantTarget(epRow, epCol);
      }
    }
    remainingParts = (fourthSpace > 0) ? remainingParts.substring(fourthSpace + 1) : "";
  }

  // Parse halfmove clock
  if (remainingParts.length() > 0) {
    int fifthSpace = remainingParts.indexOf(' ');
    String halfmoveStr = (fifthSpace > 0) ? remainingParts.substring(0, fifthSpace) : remainingParts;
    if (chessEngine != nullptr)
      chessEngine->setHalfmoveClock(halfmoveStr.toInt());
    remainingParts = (fifthSpace > 0) ? remainingParts.substring(fifthSpace + 1) : "";
  }

  // Parse fullmove number
  if (remainingParts.length() > 0) {
    int fullmove = remainingParts.toInt();
    if (chessEngine != nullptr)
      chessEngine->setFullmoveClock(fullmove > 0 ? fullmove : 1);
  }
}

void ChessUtils::printBoard(const char board[8][8]) {
  Serial.println("====== BOARD ======");
  for (int row = 0; row < 8; row++) {
    String rowStr = String(8 - row) + " ";
    for (int col = 0; col < 8; col++) {
      char piece = board[row][col];
      rowStr += (piece == ' ') ? String(". ") : String(piece) + " ";
    }
    rowStr += " " + String(8 - row);
    Serial.println(rowStr);
  }
  Serial.println("  a b c d e f g h");
  Serial.println("===================");
}

float ChessUtils::evaluatePosition(const char board[8][8]) {
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

String ChessUtils::toUCIMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  String move = "";
  move += (char)('a' + fromCol);
  move += (char)('0' + (8 - fromRow));
  move += (char)('a' + toCol);
  move += (char)('0' + (8 - toRow));

  if (promotion != ' ' && promotion != '\0') {
    move += (char)tolower(promotion);
  }

  return move;
}

bool ChessUtils::parseUCIMove(const String& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
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

  // Validate that from and to squares are on the board (this is always false if the above checks passed, but we check again, just in case, to make it clear)
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

bool ChessUtils::ensureNvsInitialized() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    Serial.printf("[NVS] Init failed with error 0x%x, erasing and retrying...\n", err);
    nvs_flash_erase();
    err = nvs_flash_init();
    if (err == ESP_OK) {
      Serial.println("[NVS] Successfully initialized after erase");
    } else {
      Serial.printf("[NVS] Still failed after erase: 0x%x\n", err);
    }
  }
  return err == ESP_OK;
}
