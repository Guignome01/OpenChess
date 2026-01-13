#include "stockfish_api.h"

bool StockfishAPI::parseResponse(const String& jsonString, StockfishResponse& response) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.f_str());
    response.success = false;
    response.errorMessage = "JSON parsing failed";
    return false;
  }

  // Check if the request was successful
  if (!doc.containsKey("success")) {
    response.success = false;
    response.errorMessage = "Missing 'success' field";
    return false;
  }

  response.success = doc["success"].as<bool>();

  if (!response.success) {
    // If not successful, try to get error message
    if (doc.containsKey("data")) {
      response.errorMessage = doc["data"].as<String>();
    } else {
      response.errorMessage = "Unknown error from API";
    }
    return false;
  }

  // Parse evaluation (can be null)
  if (doc.containsKey("evaluation")) {
    if (doc["evaluation"].isNull()) {
      response.evaluation = 0.0f;
    } else {
      response.evaluation = doc["evaluation"].as<float>();
    }
  } else {
    response.evaluation = 0.0f;
  }

  // Parse mate (can be null)
  response.hasMate = false;
  response.mateInMoves = 0;
  if (doc.containsKey("mate")) {
    if (!doc["mate"].isNull()) {
      response.mateInMoves = doc["mate"].as<int>();
      response.hasMate = true;
    }
  }

  // Parse bestmove (format: "bestmove <move> ponder <move>")
  response.bestMove = "";
  response.ponderMove = "";
  if (doc.containsKey("bestmove")) {
    String bestmoveStr = doc["bestmove"].as<String>();

    // Parse the bestmove string
    // Format: "bestmove b1c3 ponder h7h6"
    int moveStart = bestmoveStr.indexOf("bestmove ") + 9;
    int moveEnd = bestmoveStr.indexOf(" ", moveStart);

    if (moveEnd != -1) {
      response.bestMove = bestmoveStr.substring(moveStart, moveEnd);

      // Extract ponder move if it exists
      int ponderStart = bestmoveStr.indexOf("ponder ");
      if (ponderStart != -1) {
        ponderStart += 7; // length of "ponder "
        int ponderEnd = bestmoveStr.indexOf(" ", ponderStart);

        if (ponderEnd != -1) {
          response.ponderMove = bestmoveStr.substring(ponderStart, ponderEnd);
        } else {
          response.ponderMove = bestmoveStr.substring(ponderStart);
        }
      }
    } else {
      // No space found, use entire string after "bestmove "
      response.bestMove = bestmoveStr.substring(moveStart);
    }
  }

  // Parse continuation (top engine line)
  response.continuation = "";
  if (doc.containsKey("continuation")) {
    response.continuation = doc["continuation"].as<String>();
  }

  return true;
}

String StockfishAPI::buildRequestURL(const String& fen, int depth) {
  // Validate depth (min 5 max 16)
  int validDepth = depth > 16 ? 16 : (depth < 5 ? 5 : depth);

  // Build the URL with URL encoding
  String url = "https://stockfish.online/api/s/v2.php?fen=";

  // URL encode the FEN string (space becomes %20, etc)
  for (int i = 0; i < fen.length(); i++) {
    char c = fen[i];
    if (c == ' ') {
      url += "%20";
    } else if (c == '/' || isalnum(c) || c == '-') {
      url += c;
    } else {
      // For other special characters, use percent encoding
      url += '%';
      url += String(c >> 4, HEX);
      url += String(c & 0xF, HEX);
    }
  }

  // Add depth parameter
  url += "&depth=";
  url += validDepth;

  return url;
}
