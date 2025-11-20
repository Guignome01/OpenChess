#ifndef WIFI_MANAGER_ESP32_H
#define WIFI_MANAGER_ESP32_H

#if !defined(ESP32) && !defined(ESP8266)
  #error "wifi_manager_esp32.h is only for ESP32/ESP8266 boards"
#endif

// Include Arduino.h first to set up ESP32 environment
#include <Arduino.h>

// ESP32 uses built-in WiFi library from the core
// Note: If you get WiFiNINA errors, ensure:
// 1. You're compiling for ESP32 board (Tools -> Board -> ESP32)
// 2. WiFiNINA library is not interfering (you may need to temporarily remove it)
#include <WiFi.h>
#include <WebServer.h>

// ---------------------------
// WiFi Configuration
// ---------------------------
#define AP_SSID "OpenChessBoard"
#define AP_PASSWORD "chess123"
#define AP_PORT 80

// ---------------------------
// WiFi Manager Class for ESP32
// ---------------------------
class WiFiManagerESP32 {
private:
    WebServer server;
    bool apMode;
    bool clientConnected;
    
    // Configuration variables
    String wifiSSID;
    String wifiPassword;
    String lichessToken;
    String gameMode;
    String startupType;
    
    // Web interface methods
    String generateWebPage();
    String generateGameSelectionPage();
    void handleRoot();
    void handleGameSelection();
    void handleConfigSubmit();
    void sendResponse(String content, String contentType = "text/html");
    void parseFormData(String data);
    
public:
    WiFiManagerESP32();
    void begin();
    void handleClient();
    bool isClientConnected();
    
    // Configuration getters
    String getWiFiSSID() { return wifiSSID; }
    String getWiFiPassword() { return wifiPassword; }
    String getLichessToken() { return lichessToken; }
    String getGameMode() { return gameMode; }
    String getStartupType() { return startupType; }
    
    // Game selection via web
    int getSelectedGameMode();
    void resetGameSelection();
};

#endif // WIFI_MANAGER_ESP32_H

