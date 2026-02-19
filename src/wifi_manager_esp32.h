#ifndef WIFI_MANAGER_ESP32_H
#define WIFI_MANAGER_ESP32_H

#include "board_driver.h"
#include "stockfish_settings.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

// Forward declarations
struct LichessConfig;
class MoveHistory;

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChess"
#define AP_PASSWORD "chess123"
#define AP_PORT 80
// Your WiFi Network Credentials (can also be set via web interface)
#define SECRET_SSID "YOUR_SSID"
#define SECRET_PASS "YOUR_PASSWORD"

// ---------------------------
// WiFi Manager Class for ESP32
// ---------------------------
class WiFiManagerESP32 {
 private:
  AsyncWebServer server;

  Preferences prefs;
  String wifiSSID;
  String wifiPassword;
  String gameMode;
  String lichessToken;

  BotConfig botConfig = {StockfishSettings::medium(), true};

  MoveHistory* moveHistory;
  BoardDriver* boardDriver;
  String currentFen;
  float boardEvaluation;

  // Board edit storage (pending edits from web interface)
  String pendingFenEdit;
  bool hasPendingEdit;

  // Resign flag (set from web interface)
  bool hasPendingResign = false;

  // tracks errors across multi-file OTA uploads
  bool otaHasError = false;

  // Web interface methods
  String getWiFiInfoJSON();
  String getBoardUpdateJSON();
  String getLichessInfoJSON();
  String getBoardSettingsJSON();
  void handleBoardEditSuccess(AsyncWebServerRequest* request);
  void handleConnectWiFi(AsyncWebServerRequest* request);
  void handleGameSelection(AsyncWebServerRequest* request);
  void handleSaveLichessToken(AsyncWebServerRequest* request);
  void handleBoardSettings(AsyncWebServerRequest* request);
  void handleBoardCalibration(AsyncWebServerRequest* request);
  void handleOtaResult(AsyncWebServerRequest* request);
  void handleOtaUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
  void handleGamesRequest(AsyncWebServerRequest* request);
  void handleDeleteGame(AsyncWebServerRequest* request);

 public:
  WiFiManagerESP32(BoardDriver* boardDriver, MoveHistory* moveHistory);
  void begin();

  // Configuration getters
  String getWiFiSSID() { return wifiSSID; }
  String getWiFiPassword() { return wifiPassword; }
  // Game selection via web
  int getSelectedGameMode() const { return gameMode.toInt(); }
  void resetGameSelection() { gameMode = "0"; };
  // Bot configuration
  BotConfig getBotConfig() { return botConfig; }
  // Lichess configuration
  LichessConfig getLichessConfig();
  String getLichessToken() { return lichessToken; }
  // Board state management (FEN-based)
  void updateBoardState(const String& fen, float evaluation = 0.0f);
  String getCurrentFen() const { return currentFen; }
  float getEvaluation() const { return boardEvaluation; }
  // Board edit management (FEN-based)
  bool getPendingBoardEdit(String& fenOut);
  void clearPendingEdit();
  // Web resign
  bool getPendingResign() const { return hasPendingResign; }
  void clearPendingResign() { hasPendingResign = false; }
  // WiFi connection management
  bool connectToWiFi(const String& ssid, const String& password, bool fromWeb = false);
};

#endif // WIFI_MANAGER_ESP32_H
