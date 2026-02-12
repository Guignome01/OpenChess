#include "lichess_api.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Static member initialization
String LichessAPI::apiToken = "";

void LichessAPI::setToken(const String& token) {
  apiToken = token;
}

String LichessAPI::getToken() {
  return apiToken;
}

bool LichessAPI::hasToken() {
  return apiToken.length() > 0;
}

String LichessAPI::makeHttpRequest(const String& method, const String& path, const String& body) {
  WiFiClientSecure client;
  client.setInsecure(); // For simplicity; in production, use proper cert validation

  if (!client.connect(LICHESS_API_HOST, LICHESS_API_PORT)) {
    Serial.println("Lichess API: Connection failed");
    return "";
  }

  // Build HTTP request
  String request = method + " " + path + " HTTP/1.1\r\n";
  request += "Host: " LICHESS_API_HOST "\r\n";
  request += "Authorization: Bearer " + apiToken + "\r\n";
  request += "Accept: application/json\r\n";

  if (body.length() > 0) {
    request += "Content-Type: application/x-www-form-urlencoded\r\n";
    request += "Content-Length: " + String(body.length()) + "\r\n";
  }

  request += "Connection: close\r\n\r\n";

  if (body.length() > 0) {
    request += body;
  }

  client.print(request);

  // Wait for response
  unsigned long timeout = millis() + 10000;
  while (client.connected() && !client.available()) {
    if (millis() > timeout) {
      Serial.println("Lichess API: Request timeout");
      client.stop();
      return "";
    }
    delay(10);
  }

  // Read response
  String response = "";
  bool headersDone = false;

  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (!headersDone) {
      if (line == "\r" || line.length() == 0) {
        headersDone = true;
      }
    } else {
      response += line + "\n";
    }
  }

  client.stop();
  return response;
}

bool LichessAPI::verifyToken(String& username) {
  String response = makeHttpRequest("GET", "/api/account");
  if (response.length() == 0) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.println("Lichess API: JSON parse error in verifyToken");
    return false;
  }

  if (doc.containsKey("username")) {
    username = doc["username"].as<String>();
    Serial.println("Lichess API: Verified token for user: " + username);
    return true;
  }

  return false;
}

bool LichessAPI::pollForGameEvent(LichessEvent& event) {
  // Check for currently playing games first
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(LICHESS_API_HOST, LICHESS_API_PORT)) {
    return false;
  }

  // Use the /api/account/playing endpoint to get current games
  String request = "GET /api/account/playing HTTP/1.1\r\n";
  request += "Host: " LICHESS_API_HOST "\r\n";
  request += "Authorization: Bearer " + apiToken + "\r\n";
  request += "Accept: application/json\r\n";
  request += "Connection: close\r\n\r\n";

  client.print(request);

  unsigned long timeout = millis() + 10000;
  while (client.connected() && !client.available()) {
    if (millis() > timeout) {
      client.stop();
      return false;
    }
    delay(10);
  }

  String response = "";
  bool headersDone = false;

  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (!headersDone) {
      if (line == "\r" || line.length() == 0) {
        headersDone = true;
      }
    } else {
      response += line;
    }
  }

  client.stop();

  if (response.length() == 0) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    return false;
  }

  JsonArray games = doc["nowPlaying"].as<JsonArray>();
  if (games.size() > 0) {
    // Get the first active game
    JsonObject game = games[0];
    event.type = LichessEventType::GAME_START;
    event.gameId = game["gameId"].as<String>();
    event.fen = game["fen"].as<String>();

    String color = game["color"].as<String>();
    event.myColor = (color == "white") ? 'w' : 'b';

    Serial.println("Lichess: Found active game: " + event.gameId);
    return true;
  }

  return false;
}

bool LichessAPI::getGameState(const String& gameId, LichessGameState& state) {
  String response = makeHttpRequest("GET", "/api/board/game/stream/" + gameId);
  if (response.length() == 0) {
    return false;
  }

  // The stream returns multiple JSON objects, we need the first "gameFull" event
  int newlinePos = response.indexOf('\n');
  String firstLine = (newlinePos > 0) ? response.substring(0, newlinePos) : response;

  return parseGameFullEvent(firstLine, state);
}

bool LichessAPI::pollGameStream(const String& gameId, LichessGameState& state) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(LICHESS_API_HOST, LICHESS_API_PORT)) {
    return false;
  }

  String request = "GET /api/board/game/stream/" + gameId + " HTTP/1.1\r\n";
  request += "Host: " LICHESS_API_HOST "\r\n";
  request += "Authorization: Bearer " + apiToken + "\r\n";
  request += "Accept: application/x-ndjson\r\n";
  request += "Connection: close\r\n\r\n";

  client.print(request);

  unsigned long timeout = millis() + 15000;
  bool headersDone = false;
  String jsonLine = "";
  bool foundData = false;
  bool isChunked = false;

  while (client.connected() || client.available()) {
    if (millis() > timeout) {
      break;
    }

    if (client.available()) {
      String line = client.readStringUntil('\n');

      // Check for chunked transfer encoding in headers
      if (!headersDone) {
        String lowerLine = line;
        lowerLine.toLowerCase();
        if (lowerLine.indexOf("transfer-encoding: chunked") >= 0) {
          isChunked = true;
        }
      }

      line.trim();

      if (!headersDone) {
        if (line.length() == 0) {
          headersDone = true;
        }
        continue;
      }

      // Skip empty lines (keep-alive)
      if (line.length() == 0) {
        continue;
      }

      // If chunked encoding, skip chunk size lines (hex numbers)
      if (isChunked) {
        // Check if this is a chunk size (hex number)
        bool isChunkSize = true;
        for (size_t i = 0; i < line.length(); i++) {
          char c = line[i];
          if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            isChunkSize = false;
            break;
          }
        }
        if (isChunkSize && line.length() <= 8) {
          continue; // Skip chunk size line
        }
      }

      // Must start with '{' to be valid JSON
      if (line[0] != '{') {
        continue;
      }

      jsonLine = line;
      foundData = true;
      break; // Got one event, process it
    }

    delay(10);
  }

  client.stop();

  if (!foundData || jsonLine.length() == 0) {
    Serial.println("Lichess: No JSON data received from game stream");
    return false;
  }

  Serial.println("Lichess: Game stream JSON: " + jsonLine.substring(0, min((size_t)200, jsonLine.length())));

  // Try to parse as gameFull first, then as gameState
  if (parseGameFullEvent(jsonLine, state)) {
    return true;
  }

  Serial.println("Lichess: parseGameFullEvent failed, trying parseGameStateEvent");
  return parseGameStateEvent(jsonLine, state);
}

// Parse a space-separated UCI moves string into a move count and the last move.
static void parseMovesList(const String& moves, int& moveCount, String& lastMove) {
  moveCount = 0;
  if (moves.length() > 0) {
    moveCount = 1;
    for (size_t i = 0; i < moves.length(); i++)
      if (moves[i] == ' ') moveCount++;
    int lastSpace = moves.lastIndexOf(' ');
    lastMove = (lastSpace >= 0) ? moves.substring(lastSpace + 1) : moves;
  } else {
    lastMove = "";
  }
}

// Check whether the game has ended and populate state accordingly.
static void checkGameEndStatus(JsonObject obj, LichessGameState& state) {
  String status = obj["status"].as<String>();
  state.status = status;
  if (status == "mate" || status == "resign" || status == "stalemate" || status == "timeout" || status == "draw" || status == "outoftime" || status == "aborted") {
    state.gameEnded = true;
    if (obj.containsKey("winner"))
      state.winner = obj["winner"].as<String>();
  }
}

// ---------------------------------------------------------------

bool LichessAPI::parseGameFullEvent(const String& json, LichessGameState& state) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.println("Lichess: JSON parse error in parseGameFullEvent");
    return false;
  }

  // Check if this is a "gameFull" event
  if (!doc.containsKey("type") || doc["type"].as<String>() != "gameFull") {
    // Try to parse anyway if it has the right structure
    if (!doc.containsKey("id") && !doc.containsKey("state")) {
      return false;
    }
  }

  state.gameId = doc["id"].as<String>();
  state.gameStarted = true;
  state.gameEnded = false;

  // Get player color
  String whiteId = doc["white"]["id"].as<String>();
  String myId = "";

  // We need to know our username to determine color
  // For now, use the "white"/"black" approach from the initial event
  if (doc.containsKey("white") && doc.containsKey("black")) {
    // Check which one has aiLevel (if any) - that's the bot
    if (doc["white"].containsKey("aiLevel")) {
      state.myColor = 'b'; // We're Black, White is AI
    } else if (doc["black"].containsKey("aiLevel")) {
      state.myColor = 'w'; // We're White, Black is AI
    }
  }

  // Get game state
  if (doc.containsKey("state")) {
    JsonObject stateObj = doc["state"];
    String moves = stateObj["moves"].as<String>();

    int moveCount = 0;
    parseMovesList(moves, moveCount, state.lastMove);
    state.isMyTurn = ((moveCount % 2 == 0) && state.myColor == 'w') || ((moveCount % 2 == 1) && state.myColor == 'b');

    // Get FEN if available
    if (stateObj.containsKey("fen")) {
      state.fen = stateObj["fen"].as<String>();
    }

    checkGameEndStatus(stateObj, state);
  }

  // Get initial FEN if provided
  if (doc.containsKey("initialFen") && doc["initialFen"].as<String>() != "startpos") {
    state.fen = doc["initialFen"].as<String>();
  }

  return true;
}

bool LichessAPI::parseGameStateEvent(const String& json, LichessGameState& state) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    return false;
  }

  // Check if this is a "gameState" event
  String type = doc["type"].as<String>();
  if (type != "gameState") {
    // Could be chatLine or other events
    return false;
  }

  String moves = doc["moves"].as<String>();

  int moveCount = 0;
  parseMovesList(moves, moveCount, state.lastMove);
  state.isMyTurn = ((moveCount % 2 == 0) && state.myColor == 'w') || ((moveCount % 2 == 1) && state.myColor == 'b');

  checkGameEndStatus(doc.as<JsonObject>(), state);

  return true;
}

bool LichessAPI::makeMove(const String& gameId, const String& move) {
  String path = "/api/board/game/" + gameId + "/move/" + move;
  String response = makeHttpRequest("POST", path);

  // Check for success
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    // Sometimes the response is just "ok" or empty on success
    return response.indexOf("ok") >= 0 || response.indexOf("true") >= 0;
  }

  if (doc.containsKey("ok") && doc["ok"].as<bool>()) {
    Serial.println("Lichess: Move sent successfully: " + move);
    return true;
  }

  Serial.println("Lichess: Move failed: " + response);
  return false;
}

bool LichessAPI::resignGame(const String& gameId) {
  String path = "/api/board/game/" + gameId + "/resign";
  String response = makeHttpRequest("POST", path);
  return response.indexOf("ok") >= 0 || response.indexOf("true") >= 0;
}
