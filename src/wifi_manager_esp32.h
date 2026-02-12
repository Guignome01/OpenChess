#ifndef WIFI_MANAGER_ESP32_H
#define WIFI_MANAGER_ESP32_H

// Include Arduino.h first to set up ESP32 environment
#include <Arduino.h>
#include <Preferences.h>

// ESP32 uses built-in WiFi library from the core
// Note: If you get WiFiNINA errors, ensure:
// 1. You're compiling for ESP32 board (Tools -> Board -> ESP32)
// 2. WiFiNINA library is not interfering (you may need to temporarily remove it)
#include "board_driver.h"
#include "stockfish_settings.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// Forward declaration for Lichess config
struct LichessConfig;

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

  // Configuration variables
  Preferences prefs;
  String wifiSSID;
  String wifiPassword;
  String gameMode;
  String lichessToken;

  // Bot configuration
  BotConfig botConfig = {StockfishSettings::medium(), true};

  // Board state storage
  BoardDriver* boardDriver;
  String currentFen; // Current board state in FEN notation
  float boardEvaluation;

  // Board edit storage (pending edits from web interface)
  String pendingFenEdit;
  bool hasPendingEdit;

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

 public:
  WiFiManagerESP32(BoardDriver* boardDriver);
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
  // WiFi connection management
  bool connectToWiFi(const String& ssid, const String& password, bool fromWeb = false);
};

#endif // WIFI_MANAGER_ESP32_H
