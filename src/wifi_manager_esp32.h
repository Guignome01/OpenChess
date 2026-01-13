#ifndef WIFI_MANAGER_ESP32_H
#define WIFI_MANAGER_ESP32_H

#if !defined(ESP32) && !defined(ESP8266)
#error "wifi_manager_esp32.h is only for ESP32/ESP8266 boards"
#endif

// Include Arduino.h first to set up ESP32 environment
#include <Arduino.h>
#include <Preferences.h>

// ESP32 uses built-in WiFi library from the core
// Note: If you get WiFiNINA errors, ensure:
// 1. You're compiling for ESP32 board (Tools -> Board -> ESP32)
// 2. WiFiNINA library is not interfering (you may need to temporarily remove it)
#include "board_driver.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChess"
#define AP_PASSWORD "chess123"
#define AP_PORT 80

// ---------------------------
// WiFi Manager Class for ESP32
// ---------------------------
class WiFiManagerESP32 {
 private:
  AsyncWebServer server;
  bool apMode;
  bool clientConnected;

  // Configuration variables
  Preferences prefs;
  String wifiSSID;
  String wifiPassword;
  String gameMode;

  // Board state storage
  BoardDriver* _boardDriver;
  char boardState[8][8];
  bool boardStateValid;
  float boardEvaluation; // Stockfish evaluation (in centipawns)

  // Board edit storage (pending edits from web interface)
  char pendingBoardEdit[8][8];
  bool hasPendingEdit;

  // Web interface methods
  String getWiFiInfoJSON();
  String getBoardUpdateJSON();
  void handleBoardEditSuccess(AsyncWebServerRequest* request);
  void handleConnectWiFi(AsyncWebServerRequest* request);
  void handleGameSelection(AsyncWebServerRequest* request);

 public:
  WiFiManagerESP32(BoardDriver* boardDriver);
  void begin();

  // Configuration getters
  String getWiFiSSID() { return wifiSSID; }
  String getWiFiPassword() { return wifiPassword; }
  // Game selection via web
  int getSelectedGameMode() { return gameMode.toInt(); }
  void resetGameSelection() { gameMode = "0"; };
  // Board state management
  void updateBoardState(char newBoardState[8][8], float evaluation = 0.0f);
  bool hasValidBoardState() { return boardStateValid; }
  float getEvaluation() { return boardEvaluation; }
  // Board edit management
  bool getPendingBoardEdit(char editBoard[8][8]);
  void clearPendingEdit();
  // WiFi connection management
  bool connectToWiFi(String ssid, String password, bool fromWeb = false);
  bool isClientConnected();
};

#endif // WIFI_MANAGER_ESP32_H
