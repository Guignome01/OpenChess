#include "wifi_manager_esp32.h"
#include "arduino_secrets.h"
#include "page_router.h"
#include <Arduino.h>
#include <Preferences.h>

extern "C" {
#include "nvs_flash.h"
}

WiFiManagerESP32::WiFiManagerESP32(BoardDriver* boardDriver) : server(AP_PORT) {
  _boardDriver = boardDriver;
  apMode = true;
  clientConnected = false;
  gameMode = "None";
  botConfig.stockfishSettings = StockfishSettings::medium(); // Default to medium
  botConfig.playerIsWhite = true;                            // Default to player as white
  boardStateValid = false;
  hasPendingEdit = false;
  boardEvaluation = 0.0;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase(); // erase and retry
    nvs_flash_init();
  }
  prefs.begin("wifiCreds", true);
  wifiSSID = prefs.getString("ssid", SECRET_SSID);
  wifiPassword = prefs.getString("pass", SECRET_PASS);
  prefs.end();
  // Initialize board state to empty
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      boardState[row][col] = ' ';
      pendingBoardEdit[row][col] = ' ';
    }
  }
}

void WiFiManagerESP32::begin() {
  Serial.println("=== Starting OpenChess WiFi Manager (ESP32) ===");
  // ESP32 can run both AP and Station modes simultaneously. Start Access Point first (always available)
  Serial.printf("Creating Access Point with SSID: %s\nUsing password: %s\n", AP_SSID, AP_PASSWORD);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("ERROR: Failed to create Access Point!");
    return;
  }
  Serial.println("Debug: Access Point created successfully");
  // Try to connect to existing WiFi
  bool connected = connectToWiFi(wifiSSID, wifiPassword) ? Serial.println("Successfully connected to WiFi network!") : Serial.println("Failed to connect to WiFi. Access Point mode still available.");
  // Print connection information
  Serial.println("=== WiFi Connection Information ===");
  if (connected) {
    Serial.printf("Connected to WiFi: %s\nAccess board via: http://%s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  Serial.printf("Access board via: http://%s\nMAC Address: %s\n", WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str());
  Serial.println("=====================================");

  // Set up web server routes with async handlers
  server.on("/board", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getBoardUpdateJSON()); });
  server.on("/wifi-info", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getWiFiInfoJSON()); });
  server.on("/board-edit", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleBoardEditSuccess(request); });
  server.on("/connect-wifi", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleConnectWiFi(request); });
  server.on("/gameselect", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleGameSelection(request); });
  server.on("/botconfig", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleBotConfiguration(request); });
  server.onNotFound([](AsyncWebServerRequest* request) {
    const Page* page = findPage(request->url().c_str());
    if (!page) {
      request->send(404, "text/plain", "Not Found");
      return;
    }
    AsyncWebServerResponse* res =
      request->beginResponse_P(
        200,
        page->mime,
        page->data,
        page->length
      );
    if (page->gzip)
      res->addHeader("Content-Encoding", "gzip");
    res->addHeader("Cache-Control", "max-age=86400");
    request->send(res); });
  server.begin();
  Serial.println("Web server started on port 80");
}

String WiFiManagerESP32::getBoardUpdateJSON() {
  String resp = "{\"board\":[";
  for (int row = 0; row < 8; row++) {
    resp += "[";
    for (int col = 0; col < 8; col++) {
      char piece = boardState[row][col];
      if (piece == ' ') {
        resp += "\"\"";
      } else {
        resp += "\"" + String(piece) + "\"";
      }
      if (col < 7)
        resp += ",";
    }
    resp += "]";
    if (row < 7)
      resp += ",";
  }
  resp += "],\"valid\":" + String(boardStateValid ? "true" : "false");
  resp += ",\"evaluation\":" + String(boardEvaluation, 2) + "}";
  return resp;
}

String WiFiManagerESP32::getWiFiInfoJSON() {
  return "{\"ssid\":\"" + wifiSSID + "\",\"password\":\"" + wifiPassword + "\",\"connected\":\"" + (WiFi.status() == WL_CONNECTED ? "true" : "false") + "\",\"ap_ssid\":\"" AP_SSID "\",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",\"local_ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0") + "\"}";
}

void WiFiManagerESP32::handleBoardEditSuccess(AsyncWebServerRequest* request) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      String paramName = "r" + String(row) + "c" + String(col);
      if (request->hasArg(paramName.c_str())) {
        String value = request->arg(paramName.c_str());
        if (value.length() > 0)
          pendingBoardEdit[row][col] = value.charAt(0);
        else
          pendingBoardEdit[row][col] = ' ';
      } else {
        pendingBoardEdit[row][col] = ' ';
      }
    }
  }
  hasPendingEdit = true;
  Serial.println("Board edit received and stored");
  request->send(200, "text/plain", "OK");
}

void WiFiManagerESP32::handleConnectWiFi(AsyncWebServerRequest* request) {
  String newWifiSSID = "";
  String newWifiPassword = "";
  if (request->hasArg("ssid"))
    newWifiSSID = request->arg("ssid");
  if (request->hasArg("password"))
    newWifiPassword = request->arg("password");

  if (newWifiSSID.length() >= 1 && newWifiPassword.length() >= 5 && newWifiSSID != wifiSSID && newWifiPassword != wifiPassword) {
    request->send(200, "text/plain", "OK");
    if (connectToWiFi(newWifiSSID, newWifiPassword, true)) {
      prefs.begin("wifiCreds", false);
      prefs.putString("ssid", newWifiSSID);
      prefs.putString("pass", newWifiPassword);
      prefs.end();
      wifiSSID = newWifiSSID;
      wifiPassword = newWifiPassword;
      Serial.println("WiFi credentials updated and saved to NVS");
    }
    return;
  }
  request->send(400, "text/plain", "ERROR");
}

void WiFiManagerESP32::handleGameSelection(AsyncWebServerRequest* request) {
  int mode = 0;
  if (request->hasArg("gamemode"))
    mode = request->arg("gamemode").toInt();
  gameMode = String(mode);
  Serial.println("Game mode selected via web: " + gameMode);
  request->send(200, "text/plain", "OK");
}

void WiFiManagerESP32::handleBotConfiguration(AsyncWebServerRequest* request) {
  if (request->hasArg("difficulty") && request->hasArg("playerColor")) {
    int difficulty = request->arg("difficulty").toInt();
    switch (difficulty) {
      case 1:
        botConfig.stockfishSettings = StockfishSettings::easy();
        break;
      case 2:
        botConfig.stockfishSettings = StockfishSettings::medium();
        break;
      case 3:
        botConfig.stockfishSettings = StockfishSettings::hard();
        break;
      case 4:
        botConfig.stockfishSettings = StockfishSettings::expert();
        break;
      default:
        botConfig.stockfishSettings = StockfishSettings::medium();
        break;
    }
    String playerColor = request->arg("playerColor");
    botConfig.playerIsWhite = (playerColor == "white");

    Serial.printf("Bot configuration received: Depth=%d, Player is %s\n",
                  botConfig.stockfishSettings.depth, botConfig.playerIsWhite ? "White" : "Black");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Missing parameters");
  }
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      boardState[row][col] = newBoardState[row][col];
    }
  }
  boardStateValid = true;
  boardEvaluation = evaluation;
}

bool WiFiManagerESP32::getPendingBoardEdit(char editBoard[8][8]) {
  if (hasPendingEdit) {
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        editBoard[row][col] = pendingBoardEdit[row][col];
      }
    }
    return true;
  }
  return false;
}

void WiFiManagerESP32::clearPendingEdit() {
  hasPendingEdit = false;
}

bool WiFiManagerESP32::connectToWiFi(String ssid, String password, bool fromWeb) {
  if (!fromWeb && WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    apMode = false; // We're connected, but AP is still running
    return true;
  }
  Serial.println("=== Connecting to WiFi Network" + String(fromWeb ? "(from web)" : "") + " ===");
  Serial.printf("SSID: %s\nPassword: %s\n", ssid.c_str(), password.c_str());

  // ESP32 can run both AP and Station modes simultaneously
  WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    _boardDriver->showConnectingAnimation();
    attempts++;
    Serial.printf("Connection attempt %d/10 - Status: %d\n", attempts, WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    apMode = false; // We're connected, but AP is still running
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    // AP mode is still available
    return false;
  }
}

bool WiFiManagerESP32::isClientConnected() {
  return WiFi.softAPgetStationNum() > 0;
}
