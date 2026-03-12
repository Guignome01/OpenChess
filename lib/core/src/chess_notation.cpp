#include "chess_notation.h"

#include <cctype>
#include <cstring>

#include "chess_history.h"  // MoveEntry
#include "chess_iterator.h"
#include "chess_rules.h"
#include "chess_utils.h"

namespace ChessNotation {

// Strip trailing '+' and '#' check/checkmate suffixes in place.
static void stripCheckSuffix(std::string& s) {
  while (!s.empty() && (s.back() == '+' || s.back() == '#'))
    s.pop_back();
}

// Castling notation: kingside "O-O", queenside "O-O-O".
static std::string castlingNotation(const MoveEntry& move) {
  return (move.toCol > move.fromCol) ? "O-O" : "O-O-O";
}

// ---------------------------------------------------------------------------
// Output — Coordinate notation
// ---------------------------------------------------------------------------

std::string toCoordinate(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  std::string move = ChessUtils::squareName(fromRow, fromCol);
  move += ChessUtils::squareName(toRow, toCol);

  if (promotion != ' ' && promotion != '\0') {
    move += static_cast<char>(tolower(promotion));
  }

  return move;
}

// ---------------------------------------------------------------------------
// Output — Long Algebraic Notation
// ---------------------------------------------------------------------------

std::string toLAN(const MoveEntry& move) {
  // Castling
  if (move.isCastling) {
    return castlingNotation(move);
  }

  std::string result;
  char piece = toupper(move.piece);

  // Piece prefix (omit for pawns)
  if (piece != 'P') {
    result += piece;
  }

  // Origin square
  result += ChessUtils::squareName(move.fromRow, move.fromCol);

  // Separator: 'x' for captures, '-' otherwise
  result += move.isCapture ? 'x' : '-';

  // Destination square
  result += ChessUtils::squareName(move.toRow, move.toCol);

  // Promotion suffix
  if (move.isPromotion && move.promotion != ' ') {
    result += '=';
    result += static_cast<char>(toupper(move.promotion));
  }

  return result;
}

// ---------------------------------------------------------------------------
// Output — Standard Algebraic Notation
// ---------------------------------------------------------------------------

// Piece letter for SAN (uppercase, omit for pawns).
static char sanPieceLetter(char piece) {
  char upper = toupper(piece);
  return (upper == 'P') ? '\0' : upper;
}

std::string toSAN(const char board[8][8], const PositionState& state,
                  const MoveEntry& move) {
  // Castling
  if (move.isCastling) {
    return castlingNotation(move);
  }

  std::string result;
  char piece = toupper(move.piece);
  char color = ChessUtils::getPieceColor(move.piece);

  if (piece == 'P') {
    // Pawn moves
    if (move.isCapture) {
      // Capture: file of origin + 'x' + destination
      result += ChessUtils::fileChar(move.fromCol);
      result += 'x';
    }
    result += ChessUtils::squareName(move.toRow, move.toCol);

    if (move.isPromotion && move.promotion != ' ') {
      result += '=';
      result += static_cast<char>(toupper(move.promotion));
    }
  } else {
    // Piece moves — may require disambiguation
    result += piece;

    // Find all same-type pieces of the same color that can also move to the target
    bool needFile = false;
    bool needRank = false;

    ChessIterator::forEachPiece(board, [&](int r, int c, char other) {
      if (r == move.fromRow && c == move.fromCol) return;
      if (toupper(other) != piece) return;
      if (ChessUtils::getPieceColor(other) != color) return;

      if (!ChessRules::isValidMove(board, r, c, move.toRow, move.toCol, state))
        return;

      if (c != move.fromCol)
        needFile = true;
      else
        needRank = true;
    });

    if (needFile) result += ChessUtils::fileChar(move.fromCol);
    if (needRank) result += ChessUtils::rankChar(move.fromRow);

    if (move.isCapture) result += 'x';

    result += ChessUtils::squareName(move.toRow, move.toCol);
  }

  return result;
}

// ---------------------------------------------------------------------------
// Input — Coordinate notation
// ---------------------------------------------------------------------------

bool parseCoordinate(const std::string& move,
                     int& fromRow, int& fromCol,
                     int& toRow, int& toCol,
                     char& promotion) {
  size_t len = move.length();
  if (len < 4 || len > 5) return false;

  char fromFile = move[0];
  char fromRank = move[1];
  char toFile = move[2];
  char toRank = move[3];

  fromCol = ChessUtils::fileIndex(fromFile);
  fromRow = ChessUtils::rankIndex(fromRank);
  toCol = ChessUtils::fileIndex(toFile);
  toRow = ChessUtils::rankIndex(toRank);

  if (!ChessUtils::isValidSquare(fromRow, fromCol) || !ChessUtils::isValidSquare(toRow, toCol))
    return false;

  // From and to squares must differ
  if (fromRow == toRow && fromCol == toCol) return false;

  // Promotion (optional 5th char) must be q, r, b, or n
  promotion = ' ';
  if (len == 5) {
    char promo = tolower(move[4]);
    if (!ChessUtils::isValidPromotionChar(promo)) return false;
    promotion = move[4];
  }

  return true;
}

// ---------------------------------------------------------------------------
// Input — Long Algebraic Notation
// ---------------------------------------------------------------------------

bool parseLAN(const std::string& move,
              int& fromRow, int& fromCol,
              int& toRow, int& toCol,
              char& promotion) {
  if (move.empty()) return false;

  // Handle castling: LAN lacks board context needed to resolve the king's
  // position, so we return false here.  The auto-detect entry point
  // parseMove() tries LAN first; when it returns false for castling
  // notation, it falls through to parseSAN which has board context.
  // Coordinate-style castling (e.g. e1g1) is handled by parseCoordinate.
  std::string upper;
  for (char c : move) upper += static_cast<char>(toupper(c));
  if (upper == "O-O" || upper == "0-0" ||
      upper == "O-O-O" || upper == "0-0-0") {
    return false;
  }

  // Strip check/checkmate suffixes
  std::string cleaned = move;
  stripCheckSuffix(cleaned);

  // Strip promotion marker (=Q, =q, etc.)
  promotion = ' ';
  if (cleaned.size() >= 2 && cleaned[cleaned.size() - 2] == '=') {
    char promo = tolower(cleaned.back());
    if (ChessUtils::isValidPromotionChar(promo)) {
      promotion = cleaned.back();
      cleaned.erase(cleaned.size() - 2, 2);
    }
  }

  // Strip piece prefix (uppercase letters A-Z at start that aren't file letters
  // in context — the first char is a piece if followed by a file letter)
  if (!cleaned.empty() && isupper(cleaned[0])) {
    char next = cleaned.size() > 1 ? cleaned[1] : 0;
    // Piece prefix if next char is a file letter (a-h)
    if (next >= 'a' && next <= 'h') {
      cleaned.erase(0, 1);
    }
  }

  // Remove separator characters ('-' and 'x')
  std::string coords;
  for (char c : cleaned) {
    if (c != '-' && c != 'x') {
      coords += c;
    }
  }

  // Now we should have pure coordinate notation (e.g., "e2e4")
  // Re-attach promotion if we stripped it
  if (promotion != ' ') {
    coords += promotion;
  }

  return parseCoordinate(coords, fromRow, fromCol, toRow, toCol, promotion);
}

// ---------------------------------------------------------------------------
// Input — Standard Algebraic Notation
// ---------------------------------------------------------------------------

// Check if a character is a piece letter (NBRQK — not P, pawns are implicit)
static bool isPieceLetter(char c) {
  return c == 'N' || c == 'B' || c == 'R' || c == 'Q' || c == 'K';
}

// Find and validate a castling move for the given side.
static bool findCastlingMove(const char board[8][8], const PositionState& state,
                             char currentTurn, bool kingSide,
                             int& fromRow, int& fromCol, int& toRow, int& toCol,
                             char& promotion) {
  int row = ChessUtils::homeRow(currentTurn);
  char king = board[row][4];
  if (toupper(king) != 'K' || ChessUtils::getPieceColor(king) != currentTurn)
    return false;
  if (!ChessUtils::hasCastlingRight(state.castlingRights, currentTurn, kingSide))
    return false;
  fromRow = row; fromCol = 4;
  toRow = row;   toCol = kingSide ? 6 : 2;
  promotion = ' ';
  return ChessRules::isValidMove(board, fromRow, fromCol, toRow, toCol, state);
}

bool parseSAN(const char board[8][8], const PositionState& state,
              char currentTurn, const std::string& san,
              int& fromRow, int& fromCol,
              int& toRow, int& toCol,
              char& promotion) {
  if (san.empty()) return false;

  // Handle castling
  std::string upper;
  for (char c : san) upper += static_cast<char>(toupper(c));
  // Strip check suffixes for castling comparison
  std::string castleStr = upper;
  stripCheckSuffix(castleStr);

  if (castleStr == "O-O" || castleStr == "0-0")
    return findCastlingMove(board, state, currentTurn, true, fromRow, fromCol, toRow, toCol, promotion);

  if (castleStr == "O-O-O" || castleStr == "0-0-0")
    return findCastlingMove(board, state, currentTurn, false, fromRow, fromCol, toRow, toCol, promotion);

  // Strip check/checkmate suffixes
  std::string s = san;
  stripCheckSuffix(s);
  if (s.empty()) return false;

  // Extract promotion (=Q or just Q at end for pawns reaching last rank)
  promotion = ' ';
  if (s.size() >= 2 && s[s.size() - 2] == '=') {
    char promo = s.back();
    if (ChessUtils::isValidPromotionChar(promo)) {
      promotion = tolower(promo);
      s.erase(s.size() - 2, 2);
    }
  }

  // Determine piece type
  char pieceType;  // uppercase piece letter
  if (isPieceLetter(s[0])) {
    pieceType = s[0];
    s.erase(0, 1);
  } else {
    pieceType = 'P';  // pawn
  }

  // Remove capture marker
  // After removing piece letter, remaining can be like: "xf3", "ef3", "e4", "ae4", "1e4", "a1e4"
  std::string stripped;
  for (char c : s) {
    if (c != 'x') stripped += c;
  }
  s = stripped;

  if (s.size() < 2) return false;

  // Last two chars are always the destination square
  char destFile = s[s.size() - 2];
  char destRank = s[s.size() - 1];
  if (destFile < 'a' || destFile > 'h' || destRank < '1' || destRank > '8')
    return false;

  toCol = ChessUtils::fileIndex(destFile);
  toRow = ChessUtils::rankIndex(destRank);

  // Disambiguation hints (characters before the destination)
  int hintFile = -1;  // 0-7 if file hint given
  int hintRank = -1;  // 0-7 if rank hint given
  std::string hints = s.substr(0, s.size() - 2);
  for (char c : hints) {
    if (c >= 'a' && c <= 'h') {
      hintFile = ChessUtils::fileIndex(c);
    } else if (c >= '1' && c <= '8') {
      hintRank = ChessUtils::rankIndex(c);
    }
  }

  // Search the board for a matching piece
  int matchRow = -1, matchCol = -1;
  int matchCount = 0;

  ChessIterator::forEachPiece(board, [&](int r, int c, char p) {
    if (toupper(p) != pieceType) return;
    if (ChessUtils::getPieceColor(p) != currentTurn) return;

    if (hintFile >= 0 && c != hintFile) return;
    if (hintRank >= 0 && r != hintRank) return;

    if (!ChessRules::isValidMove(board, r, c, toRow, toCol, state))
      return;

    matchRow = r;
    matchCol = c;
    ++matchCount;
  });

  if (matchCount != 1) return false;  // ambiguous or no match

  fromRow = matchRow;
  fromCol = matchCol;
  return true;
}

// ---------------------------------------------------------------------------
// Input — Auto-detect format
// ---------------------------------------------------------------------------

// Quick check: is this string coordinate notation? (4-5 chars: [a-h][1-8][a-h][1-8][qrbn]?)
static bool looksLikeCoordinate(const std::string& move) {
  size_t len = move.length();
  if (len < 4 || len > 5) return false;
  if (move[0] < 'a' || move[0] > 'h') return false;
  if (move[1] < '1' || move[1] > '8') return false;
  if (move[2] < 'a' || move[2] > 'h') return false;
  if (move[3] < '1' || move[3] > '8') return false;
  if (len == 5) {
    if (!ChessUtils::isValidPromotionChar(move[4])) return false;
  }
  return true;
}

// Quick check: does the string contain LAN-specific separators?
static bool looksLikeLAN(const std::string& move) {
  // LAN has '-' or 'x' between squares, often with a piece prefix
  // Must have at least 5 chars (e2-e4) and contain '-' or 'x' with
  // file-rank patterns on both sides
  for (size_t i = 0; i < move.size(); ++i) {
    if (move[i] == '-' && i > 0) return true;
  }
  // Check for piece prefix + squares (e.g., "Ng1xf3")
  if (move.size() >= 5 && isPieceLetter(move[0])) {
    // Check if char at position 3 or 4 is 'x'
    for (size_t i = 2; i < move.size() && i < 5; ++i) {
      if (move[i] == 'x') return true;
    }
  }
  return false;
}

bool parseMove(const char board[8][8], const PositionState& state,
               char currentTurn, const std::string& move,
               int& fromRow, int& fromCol,
               int& toRow, int& toCol,
               char& promotion) {
  if (move.empty()) return false;

  // 1. Try coordinate notation first (fastest, most common in UCI protocol)
  if (looksLikeCoordinate(move)) {
    if (parseCoordinate(move, fromRow, fromCol, toRow, toCol, promotion))
      return true;
  }

  // 2. Try LAN (has separators like '-' or piece prefix with 'x')
  if (looksLikeLAN(move)) {
    if (parseLAN(move, fromRow, fromCol, toRow, toCol, promotion))
      return true;
  }

  // 3. Try SAN (requires board context)
  if (parseSAN(board, state, currentTurn, move, fromRow, fromCol, toRow, toCol, promotion))
    return true;

  return false;
}

}  // namespace ChessNotation
