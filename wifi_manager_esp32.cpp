#include "wifi_manager_esp32.h"
#include <Arduino.h>

WiFiManagerESP32::WiFiManagerESP32() : server(AP_PORT) {
    apMode = true;
    clientConnected = false;
    wifiSSID = "";
    wifiPassword = "";
    lichessToken = "";
    gameMode = "None";
    startupType = "WiFi";
}

void WiFiManagerESP32::begin() {
    Serial.println("=== Starting OpenChess WiFi Manager (ESP32) ===");
    Serial.println("Debug: WiFi Manager begin() called");
    
    // Start Access Point
    Serial.print("Debug: Creating Access Point with SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Debug: Using password: ");
    Serial.println(AP_PASSWORD);
    
    Serial.println("Debug: Calling WiFi.softAP()...");
    
    // Create Access Point
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    if (!apStarted) {
        Serial.println("ERROR: Failed to create Access Point!");
        return;
    }
    
    Serial.println("Debug: Access Point created successfully");
    
    // Wait a moment for AP to stabilize
    delay(100);
    
    // Print AP information
    IPAddress ip = WiFi.softAPIP();
    Serial.println("=== WiFi Access Point Information ===");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.print("Web Interface: http://");
    Serial.println(ip);
    Serial.print("MAC Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("=====================================");
    
    // Set up web server routes
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/game", HTTP_GET, [this]() { 
        String gameSelectionPage = this->generateGameSelectionPage();
        this->server.send(200, "text/html", gameSelectionPage);
    });
    server.on("/submit", HTTP_POST, [this]() { this->handleConfigSubmit(); });
    server.on("/gameselect", HTTP_POST, [this]() { this->handleGameSelection(); });
    server.onNotFound([this]() {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>404 - Page Not Found</h2>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
        response += "</body></html>";
        this->sendResponse(response, "text/html");
    });
    
    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

void WiFiManagerESP32::handleClient() {
    server.handleClient();
}

void WiFiManagerESP32::handleRoot() {
    String webpage = generateWebPage();
    sendResponse(webpage);
}

void WiFiManagerESP32::handleConfigSubmit() {
    if (server.hasArg("plain")) {
        parseFormData(server.arg("plain"));
    } else {
        // Try to get form data from POST body
        String body = "";
        while (server.hasArg("ssid") || server.hasArg("password") || server.hasArg("token") || 
               server.hasArg("gameMode") || server.hasArg("startupType")) {
            if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
            if (server.hasArg("password")) wifiPassword = server.arg("password");
            if (server.hasArg("token")) lichessToken = server.arg("token");
            if (server.hasArg("gameMode")) gameMode = server.arg("gameMode");
            if (server.hasArg("startupType")) startupType = server.arg("startupType");
            break;
        }
    }
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Configuration Saved!</h2>";
    response += "<p>WiFi SSID: " + wifiSSID + "</p>";
    response += "<p>Game Mode: " + gameMode + "</p>";
    response += "<p>Startup Type: " + startupType + "</p>";
    response += "<p><a href='/game' style='color:#ec8703;'>Go to Game Selection</a></p>";
    response += "</body></html>";
    sendResponse(response);
}

void WiFiManagerESP32::handleGameSelection() {
    // Parse game mode selection
    int mode = 0;
    
    if (server.hasArg("gamemode")) {
        mode = server.arg("gamemode").toInt();
    } else if (server.hasArg("plain")) {
        // Try to parse from plain body
        String body = server.arg("plain");
        int modeStart = body.indexOf("gamemode=");
        if (modeStart >= 0) {
            int modeEnd = body.indexOf("&", modeStart);
            if (modeEnd < 0) modeEnd = body.length();
            String selectedMode = body.substring(modeStart + 9, modeEnd);
            mode = selectedMode.toInt();
        }
    }
    
    Serial.print("Game mode selected via web: ");
    Serial.println(mode);
    
    // Store the selected game mode
    gameMode = String(mode);
    
    String response = "{\"status\":\"success\",\"message\":\"Game mode selected\",\"mode\":" + String(mode) + "}";
    sendResponse(response, "application/json");
}

void WiFiManagerESP32::sendResponse(String content, String contentType) {
    server.send(200, contentType, content);
}

String WiFiManagerESP32::generateWebPage() {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>OPENCHESSBOARD CONFIGURATION</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #5c5d5e; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
    html += ".container { background-color: #353434; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); padding: 30px; width: 100%; max-width: 500px; }";
    html += "h2 { text-align: center; color: #ec8703; font-size: 24px; margin-bottom: 20px; }";
    html += "label { font-size: 16px; color: #ec8703; margin-bottom: 8px; display: block; }";
    html += "input[type=\"text\"], input[type=\"password\"], select { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; font-size: 16px; }";
    html += "input[type=\"submit\"], .button { background-color: #ec8703; color: white; border: none; padding: 15px; font-size: 16px; width: 100%; border-radius: 5px; cursor: pointer; transition: background-color 0.3s ease; text-decoration: none; display: block; text-align: center; margin: 10px 0; }";
    html += "input[type=\"submit\"]:hover, .button:hover { background-color: #ebca13; }";
    html += ".form-group { margin-bottom: 15px; }";
    html += ".note { font-size: 14px; color: #ec8703; text-align: center; margin-top: 20px; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class=\"container\">";
    html += "<h2>OPENCHESSBOARD CONFIGURATION</h2>";
    html += "<form action=\"/submit\" method=\"POST\">";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"ssid\">WiFi SSID:</label>";
    html += "<input type=\"text\" name=\"ssid\" id=\"ssid\" value=\"" + wifiSSID + "\" placeholder=\"Enter Your WiFi SSID\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"password\">WiFi Password:</label>";
    html += "<input type=\"password\" name=\"password\" id=\"password\" value=\"\" placeholder=\"Enter Your WiFi Password\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"token\">Lichess Token (Optional):</label>";
    html += "<input type=\"text\" name=\"token\" id=\"token\" value=\"" + lichessToken + "\" placeholder=\"Enter Your Lichess Token (Future Feature)\">";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"gameMode\">Default Game Mode:</label>";
    html += "<select name=\"gameMode\" id=\"gameMode\">";
    html += "<option value=\"None\"";
    if (gameMode == "None") html += " selected";
    html += ">Local Chess Only</option>";
    html += "<option value=\"5+3\"";
    if (gameMode == "5+3") html += " selected";
    html += ">5+3 (Future)</option>";
    html += "<option value=\"10+5\"";
    if (gameMode == "10+5") html += " selected";
    html += ">10+5 (Future)</option>";
    html += "<option value=\"15+10\"";
    if (gameMode == "15+10") html += " selected";
    html += ">15+10 (Future)</option>";
    html += "<option value=\"AI level 1\"";
    if (gameMode == "AI level 1") html += " selected";
    html += ">AI level 1 (Future)</option>";
    html += "<option value=\"AI level 2\"";
    if (gameMode == "AI level 2") html += " selected";
    html += ">AI level 2 (Future)</option>";
    html += "</select>";
    html += "</div>";
    
    html += "<div class=\"form-group\">";
    html += "<label for=\"startupType\">Default Startup Type:</label>";
    html += "<select name=\"startupType\" id=\"startupType\">";
    html += "<option value=\"WiFi\"";
    if (startupType == "WiFi") html += " selected";
    html += ">WiFi Mode</option>";
    html += "<option value=\"Local\"";
    if (startupType == "Local") html += " selected";
    html += ">Local Mode</option>";
    html += "</select>";
    html += "</div>";
    
    html += "<input type=\"submit\" value=\"Save Configuration\">";
    html += "</form>";
    html += "<a href=\"/game\" class=\"button\">Game Selection Interface</a>";
    html += "<div class=\"note\">";
    html += "<p>Configure your OpenChess board settings and WiFi connection.</p>";
    html += "</div>";
    html += "</div>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

String WiFiManagerESP32::generateGameSelectionPage() {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>OPENCHESSBOARD GAME SELECTION</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #5c5d5e; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
    html += ".container { background-color: #353434; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); padding: 30px; width: 100%; max-width: 600px; }";
    html += "h2 { text-align: center; color: #ec8703; font-size: 24px; margin-bottom: 30px; }";
    html += ".game-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 30px; }";
    html += ".game-mode { background-color: #444; border: 2px solid #ec8703; border-radius: 8px; padding: 20px; text-align: center; cursor: pointer; transition: all 0.3s ease; color: #fff; }";
    html += ".game-mode:hover { background-color: #ec8703; transform: translateY(-2px); }";
    html += ".game-mode.available { border-color: #4CAF50; }";
    html += ".game-mode.coming-soon { border-color: #888; opacity: 0.6; }";
    html += ".game-mode h3 { margin: 0 0 10px 0; font-size: 18px; }";
    html += ".game-mode p { margin: 0; font-size: 14px; opacity: 0.8; }";
    html += ".status { font-size: 12px; padding: 5px 10px; border-radius: 15px; margin-top: 10px; display: inline-block; }";
    html += ".available .status { background-color: #4CAF50; color: white; }";
    html += ".coming-soon .status { background-color: #888; color: white; }";
    html += ".back-button { background-color: #666; color: white; border: none; padding: 15px; font-size: 16px; width: 100%; border-radius: 5px; cursor: pointer; text-decoration: none; display: block; text-align: center; margin-top: 20px; }";
    html += ".back-button:hover { background-color: #777; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class=\"container\">";
    html += "<h2>GAME SELECTION</h2>";
    html += "<div class=\"game-grid\">";
    
    html += "<div class=\"game-mode available\" onclick=\"selectGame(1)\">";
    html += "<h3>Chess Moves</h3>";
    html += "<p>Full chess game with move validation and animations</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode coming-soon\">";
    html += "<h3>Game Mode 2</h3>";
    html += "<p>Future game mode placeholder</p>";
    html += "<span class=\"status\">Coming Soon</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode coming-soon\">";
    html += "<h3>Game Mode 3</h3>";
    html += "<p>Future game mode placeholder</p>";
    html += "<span class=\"status\">Coming Soon</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode available\" onclick=\"selectGame(4)\">";
    html += "<h3>Sensor Test</h3>";
    html += "<p>Test and calibrate board sensors</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "</div>";
    html += "<a href=\"/\" class=\"back-button\">Back to Configuration</a>";
    html += "</div>";
    
    html += "<script>";
    html += "function selectGame(mode) {";
    html += "if (mode === 1 || mode === 4) {";
    html += "fetch('/gameselect', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'gamemode=' + mode })";
    html += ".then(response => response.text())";
    html += ".then(data => { alert('Game mode ' + mode + ' selected! Check your chess board.'); })";
    html += ".catch(error => { console.error('Error:', error); });";
    html += "} else { alert('This game mode is coming soon!'); }";
    html += "}";
    html += "</script>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

void WiFiManagerESP32::parseFormData(String data) {
    // Parse URL-encoded form data
    int ssidStart = data.indexOf("ssid=");
    if (ssidStart >= 0) {
        int ssidEnd = data.indexOf("&", ssidStart);
        if (ssidEnd < 0) ssidEnd = data.length();
        wifiSSID = data.substring(ssidStart + 5, ssidEnd);
        wifiSSID.replace("+", " ");
        wifiSSID.replace("%20", " ");
    }
    
    int passStart = data.indexOf("password=");
    if (passStart >= 0) {
        int passEnd = data.indexOf("&", passStart);
        if (passEnd < 0) passEnd = data.length();
        wifiPassword = data.substring(passStart + 9, passEnd);
        wifiPassword.replace("+", " ");
        wifiPassword.replace("%20", " ");
    }
    
    int tokenStart = data.indexOf("token=");
    if (tokenStart >= 0) {
        int tokenEnd = data.indexOf("&", tokenStart);
        if (tokenEnd < 0) tokenEnd = data.length();
        lichessToken = data.substring(tokenStart + 6, tokenEnd);
        lichessToken.replace("+", " ");
        lichessToken.replace("%20", " ");
    }
    
    int gameModeStart = data.indexOf("gameMode=");
    if (gameModeStart >= 0) {
        int gameModeEnd = data.indexOf("&", gameModeStart);
        if (gameModeEnd < 0) gameModeEnd = data.length();
        gameMode = data.substring(gameModeStart + 9, gameModeEnd);
        gameMode.replace("+", " ");
        gameMode.replace("%20", " ");
    }
    
    int startupStart = data.indexOf("startupType=");
    if (startupStart >= 0) {
        int startupEnd = data.indexOf("&", startupStart);
        if (startupEnd < 0) startupEnd = data.length();
        startupType = data.substring(startupStart + 12, startupEnd);
    }
    
    Serial.println("Configuration updated:");
    Serial.println("SSID: " + wifiSSID);
    Serial.println("Game Mode: " + gameMode);
    Serial.println("Startup Type: " + startupType);
}

bool WiFiManagerESP32::isClientConnected() {
    return WiFi.softAPgetStationNum() > 0;
}

int WiFiManagerESP32::getSelectedGameMode() {
    return gameMode.toInt();
}

void WiFiManagerESP32::resetGameSelection() {
    gameMode = "0";
}

