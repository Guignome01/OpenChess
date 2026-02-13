#include "wifi_manager_esp32.h"
#include "chess_lichess.h"
#include "chess_utils.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>

static const char* INITIAL_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

WiFiManagerESP32::WiFiManagerESP32(BoardDriver* bd) : boardDriver(bd), server(AP_PORT), wifiSSID(SECRET_SSID), wifiPassword(SECRET_PASS), gameMode("0"), lichessToken(""), botConfig(), currentFen(INITIAL_FEN), hasPendingEdit(false), boardEvaluation(0.0f) {}

void WiFiManagerESP32::begin() {
  Serial.println("=== Starting OpenChess WiFi Manager (ESP32) ===");

  // Mount LittleFS filesystem for serving web pages
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: LittleFS mount failed!");
  } else {
    Serial.println("LittleFS mounted successfully");
  }

  if (!ChessUtils::ensureNvsInitialized()) {
    Serial.println("NVS init failed - WiFi credentials not loaded");
  } else {
    prefs.begin("wifiCreds", false);
    if (prefs.isKey("ssid")) {
      wifiSSID = prefs.getString("ssid", SECRET_SSID);
      wifiPassword = prefs.getString("pass", SECRET_PASS);
    }
    prefs.end();

    // Load Lichess token
    prefs.begin("lichess", false);
    if (prefs.isKey("token")) {
      lichessToken = prefs.getString("token", "");
    }
    prefs.end();
    if (lichessToken.length() > 0) {
      Serial.println("Lichess API token loaded from NVS");
    }
  }
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("ERROR: Failed to create Access Point!");
    return;
  }
  bool connected = connectToWiFi(wifiSSID, wifiPassword);
  Serial.println("==== WiFi Connection Information ====");
  Serial.println("A WiFi Access Point was created:");
  Serial.println("- SSID: " AP_SSID);
  Serial.println("- Password: " AP_PASSWORD);
  Serial.println("- Website: http://" + WiFi.softAPIP().toString());
  Serial.println("- MAC Address: " + WiFi.softAPmacAddress());
  if (connected) {
    Serial.println("Connected to WiFi network: ");
    Serial.println("- SSID: " + wifiSSID);
    Serial.println("- Password: " + wifiPassword);
    Serial.println("- Website: http://" + WiFi.localIP().toString());
  } else {
    Serial.println("Configure WiFi credentials from the web interface to join your WiFi network (Stockfish needs internet)");
  }
  Serial.println("=====================================");

  // Set up web server routes with async handlers
  server.on("/board-update", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getBoardUpdateJSON()); });
  server.on("/board-update", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleBoardEditSuccess(request); });
  server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getWiFiInfoJSON()); });
  server.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleConnectWiFi(request); });
  server.on("/gameselect", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleGameSelection(request); });
  server.on("/lichess", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getLichessInfoJSON()); });
  server.on("/lichess", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleSaveLichessToken(request); });
  server.on("/board-settings", HTTP_GET, [this](AsyncWebServerRequest* request) { request->send(200, "application/json", this->getBoardSettingsJSON()); });
  server.on("/board-settings", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleBoardSettings(request); });
  server.on("/board-calibrate", HTTP_POST, [this](AsyncWebServerRequest* request) { this->handleBoardCalibration(request); });
  server.on("/ota", HTTP_POST,
    [this](AsyncWebServerRequest* request) { this->handleOtaResult(request); },
    [this](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      this->handleOtaUpload(request, filename, index, data, len, final);
    });

  // Serve sound files directly (no gzip variant exists, avoids .gz probe errors)
  server.serveStatic("/sounds/", LittleFS, "/sounds/")
      .setTryGzipFirst(false)
      .setCacheControl("max-age=86400");

  // Serve all other static files from LittleFS (gzip handled automatically)
  // no-cache: browser revalidates every request — ensures OTA-updated files are served immediately
  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl("no-cache");

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
  });
  server.begin();
  Serial.println("Web server started on port 80");
}

String WiFiManagerESP32::getBoardUpdateJSON() {
  JsonDocument doc;
  doc["fen"] = currentFen;
  doc["evaluation"] = serialized(String(boardEvaluation, 2));
  String output;
  serializeJson(doc, output);
  return output;
}

String WiFiManagerESP32::getWiFiInfoJSON() {
  JsonDocument doc;
  doc["ssid"] = wifiSSID;
  doc["password"] = wifiPassword;
  doc["connected"] = (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  doc["ap_ssid"] = AP_SSID;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["local_ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "0.0.0.0";
  String output;
  serializeJson(doc, output);
  return output;
}

void WiFiManagerESP32::handleBoardEditSuccess(AsyncWebServerRequest* request) {
  if (request->hasArg("fen")) {
    pendingFenEdit = request->arg("fen");
    hasPendingEdit = true;
    Serial.println("Board edit received (FEN): " + pendingFenEdit);
    request->send(200, "text/plain", "OK");
  } else {
    Serial.println("Board edit failed: no FEN parameter");
    request->send(400, "text/plain", "Missing FEN parameter");
  }
}

void WiFiManagerESP32::handleConnectWiFi(AsyncWebServerRequest* request) {
  String newWifiSSID = "";
  String newWifiPassword = "";
  if (request->hasArg("ssid"))
    newWifiSSID = request->arg("ssid");
  if (request->hasArg("password"))
    newWifiPassword = request->arg("password");

  if (newWifiSSID.length() >= 1 && newWifiPassword.length() >= 5 && (newWifiSSID != wifiSSID || newWifiPassword != wifiPassword)) {
    request->send(200, "text/plain", "OK");
    if (connectToWiFi(newWifiSSID, newWifiPassword, true)) {
      if (!ChessUtils::ensureNvsInitialized())
        Serial.println("NVS init failed - WiFi credentials not saved");
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
  // If bot game mode, also handle bot config
  if (mode == 2) {
    if (request->hasArg("difficulty") && request->hasArg("playerColor")) {
      switch (request->arg("difficulty").toInt()) {
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
      botConfig.playerIsWhite = request->arg("playerColor") == "white";
      Serial.printf("Bot configuration received: Depth=%d, Player is %s\n", botConfig.stockfishSettings.depth, botConfig.playerIsWhite ? "White" : "Black");
    } else {
      request->send(400, "text/plain", "Missing bot parameters");
      return;
    }
  }
  // If Lichess mode, verify token exists
  if (mode == 3) {
    if (lichessToken.length() == 0) {
      request->send(400, "text/plain", "No Lichess API token configured");
      return;
    }
    Serial.println("Lichess mode selected via web");
  }
  Serial.println("Game mode selected via web: " + gameMode);
  request->send(200, "text/plain", "OK");
}

String WiFiManagerESP32::getLichessInfoJSON() {
  // Don't expose the actual token, just whether it exists and a masked version
  String maskedToken = "";
  if (lichessToken.length() > 8) {
    maskedToken = lichessToken.substring(0, 4) + "..." + lichessToken.substring(lichessToken.length() - 4);
  } else if (lichessToken.length() > 0) {
    maskedToken = "****";
  }
  JsonDocument doc;
  doc["hasToken"] = (lichessToken.length() > 0);
  doc["maskedToken"] = maskedToken;
  String output;
  serializeJson(doc, output);
  return output;
}

void WiFiManagerESP32::handleSaveLichessToken(AsyncWebServerRequest* request) {
  if (!request->hasArg("token")) {
    request->send(400, "text/plain", "Missing token parameter");
    return;
  }

  String newToken = request->arg("token");
  newToken.trim();

  if (newToken.length() < 10) {
    request->send(400, "text/plain", "Token too short");
    return;
  }

  // Save to NVS
  if (!ChessUtils::ensureNvsInitialized()) {
    request->send(500, "text/plain", "NVS init failed");
    return;
  }

  prefs.begin("lichess", false);
  prefs.putString("token", newToken);
  prefs.end();

  lichessToken = newToken;
  Serial.println("Lichess API token saved to NVS");

  request->send(200, "text/plain", "OK");
}

String WiFiManagerESP32::getBoardSettingsJSON() {
  JsonDocument doc;
  doc["brightness"] = boardDriver->getBrightness();
  doc["dimMultiplier"] = boardDriver->getDimMultiplier();
  String output;
  serializeJson(doc, output);
  return output;
}

void WiFiManagerESP32::handleBoardSettings(AsyncWebServerRequest* request) {
  bool changed = false;

  if (request->hasArg("brightness")) {
    int brightness = request->arg("brightness").toInt();
    if (brightness >= 0 && brightness <= 255) {
      boardDriver->setBrightness((uint8_t)brightness);
      changed = true;
    }
  }

  if (request->hasArg("dimMultiplier")) {
    int dimMult = request->arg("dimMultiplier").toInt();
    if (dimMult >= 0 && dimMult <= 100) {
      boardDriver->setDimMultiplier((uint8_t)dimMult);
      changed = true;
    }
  }

  if (changed) {
    boardDriver->saveLedSettings();
    Serial.println("Board settings updated via web interface");
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "No valid settings provided");
  }
}

void WiFiManagerESP32::handleBoardCalibration(AsyncWebServerRequest* request) {
  boardDriver->triggerCalibration();
  request->send(200, "text/plain", "Calibration will start on next reboot");
}

void WiFiManagerESP32::handleOtaResult(AsyncWebServerRequest* request) {
  bool success = !otaHasError;
  AsyncWebServerResponse* response = request->beginResponse(success ? 200 : 500, "text/plain", success ? "OK" : "FAIL");
  response->addHeader("Connection", "close");
  request->send(response);
  otaHasError = false; // reset for next OTA attempt
  if (success) {
    Serial.println("OTA update successful, scheduling reboot...");
    xTaskCreate([](void*) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }, "OtaReboot", 2048, nullptr, 1, nullptr);
  }
}

void WiFiManagerESP32::handleOtaUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    // Skip this file entirely if a previous file in the same request failed
    if (otaHasError) {
      Serial.printf("OTA skipping '%s' due to previous error\n", filename.c_str());
      return;
    }
    if (!filename.endsWith(".bin")) {
      Serial.printf("OTA rejected: invalid file type '%s'\n", filename.c_str());
      otaHasError = true;
      return;
    }
    // Auto-detect partition type from filename
    bool isFilesystem = filename.indexOf("littlefs") >= 0 || filename.indexOf("spiffs") >= 0;
    int command = isFilesystem ? U_SPIFFS : U_FLASH;
    Serial.printf("OTA %s update starting: %s\n", isFilesystem ? "filesystem" : "firmware", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
      Update.printError(Serial);
      otaHasError = true;
      return;
    }
  }

  if (otaHasError || Update.hasError())
    return;

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
    Update.abort();
    otaHasError = true;
    return;
  }

  if (final) {
    if (Update.end(true)) {
      Serial.printf("OTA update complete: %u bytes\n", index + len);
    } else {
      Update.printError(Serial);
      otaHasError = true;
    }
  }
}

LichessConfig WiFiManagerESP32::getLichessConfig() {
  LichessConfig config;
  config.apiToken = lichessToken;
  return config;
}

void WiFiManagerESP32::updateBoardState(const String& fen, float evaluation) {
  currentFen = fen;
  boardEvaluation = evaluation;
}

bool WiFiManagerESP32::getPendingBoardEdit(String& fenOut) {
  if (hasPendingEdit) {
    fenOut = pendingFenEdit;
    return true;
  }
  return false;
}

void WiFiManagerESP32::clearPendingEdit() {
  currentFen = pendingFenEdit;
  hasPendingEdit = false;
}

bool WiFiManagerESP32::connectToWiFi(const String& ssid, const String& password, bool fromWeb) {
  if (!fromWeb && WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi");
    return true;
  }
  Serial.println("=== Connecting to WiFi Network" + String(fromWeb ? "(from web)" : "") + " ===");
  Serial.printf("SSID: %s\nPassword: %s\n", ssid.c_str(), password.c_str());

  // ESP32 can run both AP and Station modes simultaneously
  WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    boardDriver->showConnectingAnimation();
    attempts++;
    Serial.printf("Connection attempt %d/10 - Status: %d\n", attempts, WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    // AP mode is still available
    return false;
  }
}
