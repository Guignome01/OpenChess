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
#include <array>

// Forward declarations
struct LichessConfig;
class MoveHistory;

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChess"
#define AP_PASSWORD "chess123"
#define AP_PORT 80
#define MDNS_HOSTNAME "openchess"

// ---------------------------
// Security Configuration
// ---------------------------
static constexpr uint8_t MAX_SAVED_NETWORKS = 3;

// WiFi state machine stabilization delay before disabling AP
static constexpr unsigned long AP_STABILIZATION_MS = 10000; // 10 seconds

// Reconnection backoff intervals (ms)
static constexpr unsigned long RECONNECT_INITIAL_MS = 5000;
static constexpr unsigned long RECONNECT_MAX_MS = 60000;

// ---------------------------
// WiFi State Machine
// ---------------------------
enum class WiFiState {
  AP_ONLY,       // No STA connection, AP is active
  CONNECTING,    // Attempting STA connection, AP is active
  CONNECTED,     // STA connected, AP disabled after stabilization
  RECONNECTING   // STA lost, AP re-enabled, attempting reconnection
};

// ---------------------------
// Saved Network Entry
// ---------------------------
struct SavedNetwork {
  String ssid;
  String password;
};

// ---------------------------
// WiFi Manager Class for ESP32
// ---------------------------
class WiFiManagerESP32 {
 private:
  AsyncWebServer server;
  Preferences prefs;
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
  String otaErrorMessage;

  // --- WiFi State Machine ---
  WiFiState wifiState = WiFiState::AP_ONLY;
  TimerHandle_t apStabilizationTimer = nullptr;
  unsigned long reconnectInterval = RECONNECT_INITIAL_MS;
  unsigned long lastReconnectAttempt = 0;
  uint8_t reconnectNetworkIndex = 0;
  bool apActive = true;

  // --- Known-Networks Registry ---
  std::array<SavedNetwork, MAX_SAVED_NETWORKS> savedNetworks;
  uint8_t networkCount = 0;
  int8_t connectedNetworkIndex = -1; // Index of currently connected network, -1 if none

  void loadNetworks();
  void saveNetworks();
  bool connectToNetwork(uint8_t index);

  // --- OTA Password ---
  String otaPasswordHash; // SHA-256 hex, empty = no password set
  String otaPasswordSalt; // 16-byte hex salt

  void loadOtaPassword();
  bool verifyOtaPassword(const String& password) const;
  String hashPassword(const String& password, const String& salt) const;
  String generateRandomHex(size_t bytes) const;

  // --- WiFi Event Handler ---
  static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
  static WiFiManagerESP32* instance; // Singleton for event callback
  void handleWiFiConnected();
  void handleWiFiDisconnected();
  static void apStabilizationCallback(TimerHandle_t timer);
  void disableAP();
  void enableAP();

  // --- Web Interface Methods ---
  String getNetworksJSON();
  String getScanResultsJSON();
  String getBoardUpdateJSON();
  String getLichessInfoJSON();
  String getBoardSettingsJSON();
  void handleBoardEditSuccess(AsyncWebServerRequest* request);
  void handleAddNetwork(AsyncWebServerRequest* request);
  void handleDeleteNetwork(AsyncWebServerRequest* request);
  void handleConnectNetwork(AsyncWebServerRequest* request);
  void handleWiFiScan(AsyncWebServerRequest* request);
  void handleGameSelection(AsyncWebServerRequest* request);
  void handleSaveLichessToken(AsyncWebServerRequest* request);
  void handleBoardSettings(AsyncWebServerRequest* request);
  void handleBoardCalibration(AsyncWebServerRequest* request);
  void handleOtaResult(AsyncWebServerRequest* request);
  void handleOtaUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
  void handleOtaPassword(AsyncWebServerRequest* request);
  void handleGamesRequest(AsyncWebServerRequest* request);
  void handleDeleteGame(AsyncWebServerRequest* request);

 public:
  WiFiManagerESP32(BoardDriver* boardDriver, MoveHistory* moveHistory);
  void begin();
  void update(); // Called from loop() — handles reconnection

  // Configuration getters
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
  // WiFi state
  WiFiState getWiFiState() const { return wifiState; }
  bool isWiFiConnected() const { return wifiState == WiFiState::CONNECTED; }
};

#endif // WIFI_MANAGER_ESP32_H
