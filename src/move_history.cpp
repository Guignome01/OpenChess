#include "move_history.h"
#include "chess_game.h"
#include "chess_utils.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <sys/stat.h>
#include <time.h>

MoveHistory::MoveHistory() : recording(false) {
  memset(&header, 0, sizeof(header));
}

void MoveHistory::begin() {
  if (!quietExists(GAMES_DIR))
    LittleFS.mkdir(GAMES_DIR);
}

uint32_t MoveHistory::getTimestamp() {
  time_t now = time(nullptr);
  // time() returns small values before NTP sync
  return (now > 1771008768) ? (uint32_t)now : 0;
}

uint8_t MoveHistory::promoCharToCode(char p) {
  switch (tolower(p)) {
    case 'q':
      return 1;
    case 'r':
      return 2;
    case 'b':
      return 3;
    case 'n':
      return 4;
    default:
      return 0;
  }
}

char MoveHistory::promoCodeToChar(uint8_t code) {
  switch (code) {
    case 1:
      return 'q';
    case 2:
      return 'r';
    case 3:
      return 'b';
    case 4:
      return 'n';
    default:
      return ' ';
  }
}

// -------------------------------------------------------
// 2-byte UCI move encoding
//   bits 15..10 = from square (row*8+col, 0-63)
//   bits  9.. 4 = to   square (row*8+col, 0-63)
//   bits  3.. 0 = promotion code (0=none)
// -------------------------------------------------------
uint16_t MoveHistory::encodeMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  uint8_t from = (uint8_t)(fromRow * 8 + fromCol);
  uint8_t to = (uint8_t)(toRow * 8 + toCol);
  uint8_t promo = promoCharToCode(promotion);
  return (uint16_t)((from << 10) | (to << 4) | promo);
}

void MoveHistory::decodeMove(uint16_t encoded, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotion) {
  uint8_t from = (encoded >> 10) & 0x3F;
  uint8_t to = (encoded >> 4) & 0x3F;
  uint8_t promo = encoded & 0x0F;
  fromRow = from / 8;
  fromCol = from % 8;
  toRow = to / 8;
  toCol = to % 8;
  promotion = promoCodeToChar(promo);
}

String MoveHistory::gamePath(int id) {
  char buf[24];
  snprintf(buf, sizeof(buf), "/games/game_%02d.bin", id);
  return String(buf);
}

std::vector<int> MoveHistory::listGameIds() {
  std::vector<int> ids;
  File dir = LittleFS.open(GAMES_DIR);
  if (!dir || !dir.isDirectory())
    return ids;

  File f = dir.openNextFile();
  while (f) {
    String name = f.name();
    // Match "game_NN.bin"
    if (name.startsWith("game_") && name.endsWith(".bin")) {
      int id = name.substring(5, name.length() - 4).toInt();
      if (id > 0)
        ids.push_back(id);
    }
    f = dir.openNextFile();
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

int MoveHistory::nextGameId() {
  auto ids = listGameIds();
  if (ids.empty())
    return 1;
  return ids.back() + 1;
}

void MoveHistory::enforceStorageLimits() {
  auto ids = listGameIds();

  // 1. Enforce MAX_GAMES
  while ((int)ids.size() > MAX_GAMES) {
    LittleFS.remove(gamePath(ids.front()));
    ids.erase(ids.begin());
    Serial.println("MoveHistory: deleted oldest game (max game limit)");
  }

  // 2. Enforce MAX_USAGE_PERCENT
  while (!ids.empty()) {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    if (total == 0 || (float)used / (float)total <= MAX_USAGE_PERCENT)
      break;
    LittleFS.remove(gamePath(ids.front()));
    ids.erase(ids.begin());
    Serial.println("MoveHistory: deleted oldest game (storage limit)");
  }
}

void MoveHistory::startGame(uint8_t mode, uint8_t playerColor, uint8_t botDepth) {
  discardLiveGame();

  memset(&header, 0, sizeof(header));
  header.version = FORMAT_VERSION;
  header.mode = mode;
  header.result = RESULT_IN_PROGRESS;
  header.winnerColor = '?';
  header.playerColor = playerColor;
  header.botDepth = botDepth;
  header.timestamp = getTimestamp();

  // Write initial header to live.bin
  File f = LittleFS.open(LIVE_MOVES_PATH, "w");
  if (f) {
    f.write((const uint8_t*)&header, sizeof(header));
    f.close();
  }

  // Create empty FEN table file
  File ft = LittleFS.open(LIVE_FEN_PATH, "w");
  if (ft) ft.close();

  recording = true;
  Serial.println("MoveHistory: new live game started");
}

void MoveHistory::addMove(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  if (!recording) return;

  uint16_t encoded = encodeMove(fromRow, fromCol, toRow, toCol, promotion);
  File f = LittleFS.open(LIVE_MOVES_PATH, "a");
  if (f) {
    f.write((const uint8_t*)&encoded, 2);
    f.close();
    header.moveCount++;
    updateLiveHeader();
  }
}

void MoveHistory::addFen(const String& fen) {
  if (!recording) return;

  // Write FEN_MARKER to moves file
  uint16_t marker = FEN_MARKER;
  File fm = LittleFS.open(LIVE_MOVES_PATH, "a");
  if (fm) {
    fm.write((const uint8_t*)&marker, 2);
    fm.close();
    header.moveCount++;
  }

  // Write FEN entry to table file: 1-byte length + FEN string
  File ft = LittleFS.open(LIVE_FEN_PATH, "a");
  if (ft) {
    uint8_t len = (uint8_t)min((int)fen.length(), 255);
    header.lastFenOffset = (uint16_t)ft.size(); // Offset of this entry = current file size
    ft.write(len);
    ft.write((const uint8_t*)fen.c_str(), len);
    ft.close();
    header.fenEntryCnt++;
  }

  updateLiveHeader();
}

void MoveHistory::updateLiveHeader() {
  File f = LittleFS.open(LIVE_MOVES_PATH, "r+");
  if (f) {
    f.seek(0);
    f.write((const uint8_t*)&header, sizeof(header));
    f.close();
  }
}

void MoveHistory::finishGame(uint8_t result, char winnerColor) {
  recording = false;

  // Update header fields
  header.result = result;
  header.winnerColor = winnerColor;
  // Refresh timestamp to game-end time if NTP available
  uint32_t ts = getTimestamp();
  if (ts > 0) header.timestamp = ts;

  updateLiveHeader();

  // Read FEN table
  std::vector<uint8_t> fenData;
  if (quietExists(LIVE_FEN_PATH)) {
    File ft = LittleFS.open(LIVE_FEN_PATH, "r");
    if (ft) {
      fenData.resize(ft.size());
      ft.read(fenData.data(), fenData.size());
      ft.close();
    }
  }

  // Append FEN table to live.bin
  if (!fenData.empty()) {
    File fm = LittleFS.open(LIVE_MOVES_PATH, "a");
    if (fm) {
      fm.write(fenData.data(), fenData.size());
      fm.close();
    }
  }

  // Enforce limits before writing new file
  enforceStorageLimits();

  // Rename to completed game file
  int id = nextGameId();
  String dest = gamePath(id);
  LittleFS.rename(LIVE_MOVES_PATH, dest.c_str());
  discardLiveGame();

  Serial.printf("MoveHistory: game saved as %s (%d moves) (%d FEN entries)\n", dest.c_str(), header.moveCount, header.fenEntryCnt);
}

bool MoveHistory::quietExists(const char* path) {
  // Use POSIX stat() to avoid VFS open() error logs from LittleFS.exists()
  struct stat st;
  String fullPath = "/littlefs" + String(path);
  return (stat(fullPath.c_str(), &st) == 0);
}

void MoveHistory::discardLiveGame() {
  recording = false;
  if (quietExists(LIVE_MOVES_PATH)) LittleFS.remove(LIVE_MOVES_PATH);
  if (quietExists(LIVE_FEN_PATH)) LittleFS.remove(LIVE_FEN_PATH);
}

bool MoveHistory::hasLiveGame() {
  return quietExists(LIVE_MOVES_PATH);
}

bool MoveHistory::getLiveGameInfo(uint8_t& mode, uint8_t& playerColor, uint8_t& botDepth) {
  File f = LittleFS.open(LIVE_MOVES_PATH, "r");
  if (!f || f.size() < sizeof(GameHeader))
    return false;

  GameHeader hdr;
  f.read((uint8_t*)&hdr, sizeof(hdr));
  f.close();

  if (hdr.version != FORMAT_VERSION) return false;

  mode = hdr.mode;
  playerColor = hdr.playerColor;
  botDepth = hdr.botDepth;
  return true;
}

bool MoveHistory::replayIntoGame(ChessGame* game) {
  if (!game) return false;

  // Read live header and moves
  File fm = LittleFS.open(LIVE_MOVES_PATH, "r");
  if (!fm || fm.size() < sizeof(GameHeader)) return false;

  GameHeader hdr;
  fm.read((uint8_t*)&hdr, sizeof(hdr));
  if (hdr.version != FORMAT_VERSION) {
    fm.close();
    return false;
  }
  if (hdr.fenEntryCnt == 0) {
    Serial.println("MoveHistory: no FEN in live game, cannot resume");
    fm.close();
    return false;
  }

  // Read all 2-byte move entries
  std::vector<uint16_t> moves(hdr.moveCount);
  for (uint16_t i = 0; i < hdr.moveCount; i++) {
    uint16_t val;
    if (fm.read((uint8_t*)&val, 2) != 2) {
      fm.close();
      return false;
    }
    moves[i] = val;
  }
  fm.close();

  // Read last FEN directly via lastFenOffset
  String lastFen;
  {
    File ft = LittleFS.open(LIVE_FEN_PATH, "r");
    if (!ft) return false;
    ft.seek(hdr.lastFenOffset);
    uint8_t len = ft.read();
    if (len > 0) {
      char* buf = new char[len + 1];
      ft.read((uint8_t*)buf, len);
      buf[len] = '\0';
      lastFen = String(buf);
      delete[] buf;
    }
    ft.close();
  }

  if (lastFen.isEmpty()) {
    Serial.println("MoveHistory: failed to read last FEN");
    return false;
  }

  // Find last FEN marker in moves (scan backwards)
  int lastFenIdx = -1;
  for (int i = (int)moves.size() - 1; i >= 0; i--) {
    if (moves[i] == FEN_MARKER) {
      lastFenIdx = i;
      break;
    }
  }

  if (lastFenIdx < 0) {
    Serial.println("MoveHistory: FEN marker not found in moves");
    return false;
  }

  Serial.println("MoveHistory: resuming from FEN: " + lastFen);

  // Set board state from last FEN
  recording = false;
  game->setBoardStateFromFEN(lastFen);

  // Replay UCI moves after the last FEN marker
  for (int i = lastFenIdx + 1; i < (int)moves.size(); i++) {
    if (moves[i] == FEN_MARKER) continue;
    int fromRow, fromCol, toRow, toCol;
    char promotion;
    decodeMove(moves[i], fromRow, fromCol, toRow, toCol, promotion);
    game->applyMove(fromRow, fromCol, toRow, toCol, promotion);
    game->advanceTurn();
  }

  // Restore header for continued recording
  header = hdr;
  recording = true;

  Serial.printf("MoveHistory: replayed %d moves from last FEN marker, game resumed\n", (moves.size() - 1) - lastFenIdx);
  return true;
}

String MoveHistory::getGameListJSON() {
  auto ids = listGameIds();
  JsonDocument doc;
  JsonArray arr = doc["games"].to<JsonArray>();

  for (int id : ids) {
    File f = LittleFS.open(gamePath(id), "r");
    if (!f || f.size() < sizeof(GameHeader)) continue;

    GameHeader hdr;
    f.read((uint8_t*)&hdr, sizeof(hdr));
    f.close();

    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = id;
    obj["mode"] = hdr.mode;
    obj["result"] = hdr.result;
    obj["winner"] = String((char)hdr.winnerColor);
    obj["playerColor"] = hdr.playerColor ? String((char)hdr.playerColor) : String("?");
    obj["botDepth"] = hdr.botDepth;
    obj["moveCount"] = hdr.moveCount;
    obj["timestamp"] = hdr.timestamp;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

bool MoveHistory::deleteGame(int id) {
  String path = gamePath(id);
  if (!quietExists(path.c_str())) return false;
  return LittleFS.remove(path);
}
