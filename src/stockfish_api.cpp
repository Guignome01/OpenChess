#include "stockfish_api.h"
#include "arduino_secrets.h"

bool StockfishAPI::parseResponse(const String& response, StockfishResponse& stockfishResp) {
  if (response.length() == 0) {
    stockfishResp.success = false;
    stockfishResp.errorMessage = "JSON parsing failed: Empty response";
    return false;
  }

  // Separate HTTP headers from body using the blank line (CRLF CRLF)
  int headerEndPos = response.indexOf("\r\n\r\n");
  String jsonOnly;

  if (headerEndPos != -1) {
    // Headers found, extract content after the blank line
    jsonOnly = response.substring(headerEndPos + 4);
  } else {
    // Try Unix line endings (LF LF)
    headerEndPos = response.indexOf("\n\n");
    if (headerEndPos != -1) {
      jsonOnly = response.substring(headerEndPos + 2);
    } else {
      // No header separator found, assume entire response is JSON
      jsonOnly = response;
    }
  }

  // Trim whitespace
  jsonOnly.trim();

  if (jsonOnly.length() == 0) {
    stockfishResp.success = false;
    stockfishResp.errorMessage = "JSON parsing failed: No content after headers";
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonOnly);

  if (error) {
    stockfishResp.success = false;
    stockfishResp.errorMessage = "JSON parsing failed: " + String(error.f_str());
    return false;
  }

  // Check if the request was successful
  if (!doc.containsKey("success")) {
    stockfishResp.success = false;
    stockfishResp.errorMessage = "Missing 'success' field";
    return false;
  }

  stockfishResp.success = doc["success"].as<bool>();

  if (!stockfishResp.success) {
    // If not successful, try to get error message
    if (doc.containsKey("error")) {
      stockfishResp.errorMessage = doc["error"].as<String>();
    } else {
      stockfishResp.errorMessage = "Unknown error from API";
    }
    return false;
  }

  // Parse evaluation (can be null)
  if (doc.containsKey("evaluation")) {
    if (doc["evaluation"].isNull()) {
      stockfishResp.evaluation = 0.0f;
    } else {
      stockfishResp.evaluation = doc["evaluation"].as<float>();
    }
  } else {
    stockfishResp.evaluation = 0.0f;
  }

  // Parse mate (can be null)
  stockfishResp.mateInMoves = 0;
  stockfishResp.hasMate = false;
  if (doc.containsKey("mate")) {
    if (!doc["mate"].isNull()) {
      stockfishResp.mateInMoves = doc["mate"].as<int>();
      stockfishResp.hasMate = true;
    }
  }

  // Parse bestmove (format: "bestmove <move> ponder <move>")
  stockfishResp.bestMove = "";
  stockfishResp.ponderMove = "";
  if (doc.containsKey("bestmove")) {
    String bestmoveStr = doc["bestmove"].as<String>();

    // Parse the bestmove string
    // Format: "bestmove b1c3 ponder h7h6"
    int moveStart = bestmoveStr.indexOf("bestmove ") + 9;
    int moveEnd = bestmoveStr.indexOf(" ", moveStart);

    if (moveEnd != -1) {
      stockfishResp.bestMove = bestmoveStr.substring(moveStart, moveEnd);

      // Extract ponder move if it exists
      int ponderStart = bestmoveStr.indexOf("ponder ");
      if (ponderStart != -1) {
        ponderStart += 7; // length of "ponder "
        int ponderEnd = bestmoveStr.indexOf(" ", ponderStart);

        if (ponderEnd != -1) {
          stockfishResp.ponderMove = bestmoveStr.substring(ponderStart, ponderEnd);
        } else {
          stockfishResp.ponderMove = bestmoveStr.substring(ponderStart);
        }
      }
    } else {
      // No space found, use entire string after "bestmove " (normal if this move is a checkmate)
      stockfishResp.bestMove = bestmoveStr.substring(moveStart);
    }
  }

  // Parse continuation (top engine line)
  stockfishResp.continuation = "";
  if (doc.containsKey("continuation")) {
    stockfishResp.continuation = doc["continuation"].as<String>();
  }

  return true;
}

bool StockfishAPI::validateUCIMove(const String& move, String& errorMessage, int& fromRow, int& fromCol, int& toRow, int& toCol) {
  errorMessage = "";
  fromRow = fromCol = toRow = toCol = -1;

  // Length check (4 or 5 characters)
  size_t len = move.length();
  if (len < 4 || len > 5) {
    errorMessage = "Invalid move length";
    return false;
  }

  // Extract coordinates
  char fromFile = move[0];
  char fromRank = move[1];
  char toFile = move[2];
  char toRank = move[3];

  // File validation [a-h]
  if (fromFile < 'a' || fromFile > 'h') {
    errorMessage = "Invalid from-file (must be a-h)";
    return false;
  }
  if (toFile < 'a' || toFile > 'h') {
    errorMessage = "Invalid to-file (must be a-h)";
    return false;
  }

  // Rank validation [1-8]
  if (fromRank < '1' || fromRank > '8') {
    errorMessage = "Invalid from-rank (must be 1-8)";
    return false;
  }
  if (toRank < '1' || toRank > '8') {
    errorMessage = "Invalid to-rank (must be 1-8)";
    return false;
  }

  // Convert to array coordinates (rank 1 = row 7, rank 8 = row 0)
  fromCol = fromFile - 'a';
  fromRow = 7 - ((fromRank - '0') - 1);
  toCol = toFile - 'a';
  toRow = 7 - ((toRank - '0') - 1);

  // Validate coordinate ranges
  if (fromRow < 0 || fromRow >= 8 || fromCol < 0 || fromCol >= 8 ||
      toRow < 0 || toRow >= 8 || toCol < 0 || toCol >= 8) {
    errorMessage = "Invalid coordinates after parsing";
    return false;
  }

  // From and to squares must differ
  if (fromFile == toFile && fromRank == toRank) {
    errorMessage = "From and to squares are identical";
    return false;
  }

  // Promotion (optional 5th char) must be q, r, b, or n (case-insensitive)
  if (len == 5) {
    char promo = tolower(move[4]);
    if (promo != 'q' && promo != 'r' && promo != 'b' && promo != 'n') {
      errorMessage = "Invalid promotion piece";
      return false;
    }
  }

  return true;
}

String StockfishAPI::buildRequestURL(const String& fen, int depth) {
  // Validate depth (min 5 max 15)
  int validDepth = depth > 15 ? 15 : (depth < 5 ? 5 : depth);

  // Build just the path + query (no scheme/host) so callers can reuse host/port constants
  String path = String(STOCKFISH_API_PATH) + "?fen=";

  // URL encode the FEN string (space becomes %20, etc)
  for (int i = 0; i < fen.length(); i++) {
    char c = fen[i];
    if (c == ' ') {
      path += "%20";
    } else if (c == '/' || isalnum(c) || c == '-') {
      path += c;
    } else {
      // For other special characters, use percent encoding
      path += '%';
      path += String(c >> 4, HEX);
      path += String(c & 0xF, HEX);
    }
  }

  // Add depth parameter
  path += "&depth=";
  path += validDepth;

  return path;
}
