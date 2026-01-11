#include "wifi_manager_esp32.h"
#include <Arduino.h>
#include "arduino_secrets.h"

WiFiManagerESP32::WiFiManagerESP32() : server(AP_PORT) {
    apMode = true;
    clientConnected = false;
    wifiSSID = "";
    wifiPassword = "";
    lichessToken = "";
    gameMode = "None";
    startupType = "WiFi";
    boardStateValid = false;
    hasPendingEdit = false;
    boardEvaluation = 0.0;
    
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
    Serial.println("Debug: WiFi Manager begin() called");
    
    // ESP32 can run both AP and Station modes simultaneously
    // Start Access Point first (always available)
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
    
    // Try to connect to existing WiFi (if credentials available)
    bool connected = false;
    
    if (wifiSSID.length() > 0 || strlen(SECRET_SSID) > 0) {
        String ssidToUse = wifiSSID.length() > 0 ? wifiSSID : String(SECRET_SSID);
        String passToUse = wifiPassword.length() > 0 ? wifiPassword : String(SECRET_PASS);
        
        Serial.println("=== Attempting to connect to WiFi network ===");
        Serial.print("SSID: ");
        Serial.println(ssidToUse);
        
        connected = connectToWiFi(ssidToUse, passToUse);
        
        if (connected) {
            Serial.println("Successfully connected to WiFi network!");
        } else {
            Serial.println("Failed to connect to WiFi. Access Point mode still available.");
        }
    }
    
    // Wait a moment for everything to stabilize
    delay(100);
    
    // Print connection information
    Serial.println("=== WiFi Connection Information ===");
    Serial.print("Access Point SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
    if (connected) {
        Serial.print("Connected to WiFi: ");
        Serial.println(WiFi.SSID());
        Serial.print("Station IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Access board via: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("Access board via: http://");
        Serial.println(WiFi.softAPIP());
    }
    Serial.print("MAC Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("=====================================");
    
    // Set up web server routes
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/game", HTTP_GET, [this]() { 
        String gameSelectionPage = this->generateGameSelectionPage();
        this->server.send(200, "text/html", gameSelectionPage);
    });
    server.on("/board", HTTP_GET, [this]() { this->handleBoard(); });
    server.on("/board-view", HTTP_GET, [this]() { this->handleBoardView(); });
    server.on("/board-edit", HTTP_GET, [this]() { 
        String boardEditPage = this->generateBoardEditPage();
        this->server.send(200, "text/html", boardEditPage);
    });
    server.on("/board-edit", HTTP_POST, [this]() { this->handleBoardEdit(); });
    server.on("/connect-wifi", HTTP_POST, [this]() { this->handleConnectWiFi(); });
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
    
    // WiFi Connection Status
    html += "<div class=\"form-group\" style=\"margin-top: 30px; padding: 15px; background-color: #444; border-radius: 5px;\">";
    html += "<h3 style=\"color: #ec8703; margin-top: 0;\">WiFi Connection</h3>";
    html += "<p style=\"color: #ec8703;\">Status: " + getConnectionStatus() + "</p>";
    if (!isConnectedToWiFi() || wifiSSID.length() > 0) {
        html += "<form action=\"/connect-wifi\" method=\"POST\" style=\"margin-top: 15px;\">";
        html += "<input type=\"hidden\" name=\"ssid\" value=\"" + wifiSSID + "\">";
        html += "<input type=\"hidden\" name=\"password\" value=\"" + wifiPassword + "\">";
        html += "<button type=\"submit\" class=\"button\" style=\"background-color: #4CAF50;\">Connect to WiFi</button>";
        html += "</form>";
        html += "<p style=\"font-size: 12px; color: #ec8703; margin-top: 10px;\">Enter WiFi credentials above and click 'Connect to WiFi' to join your network.</p>";
    }
    html += "</div>";
    
    html += "<a href=\"/game\" class=\"button\">Game Selection Interface</a>";
    html += "<a href=\"/board-view\" class=\"button\">View Chess Board</a>";
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
    html += ".game-mode.mode-1 { border-color: #FF9800; background: linear-gradient(135deg, #444 0%, #FF9800 100%); }";
    html += ".game-mode.mode-2 { border-color: #FFFFFF; background: linear-gradient(135deg, #444 0%, #FFFFFF 100%); }";
    html += ".game-mode.mode-3 { border-color: #2196F3; background: linear-gradient(135deg, #444 0%, #2196F3 100%); }";
    html += ".game-mode.mode-4 { border-color: #F44336; background: linear-gradient(135deg, #444 0%, #F44336 100%); }";
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
    
    html += "<div class=\"game-mode available mode-1\" onclick=\"selectGame(1)\">";
    html += "<h3>Chess Moves</h3>";
    html += "<p>Full chess game with move validation and animations</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode available mode-2\" onclick=\"selectGame(2)\">";
    html += "<h3>Chess Bot</h3>";
    html += "<p>Player White vs AI Black (Medium)</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode available mode-3\" onclick=\"selectGame(3)\">";
    html += "<h3>Black AI Stockfish</h3>";
    html += "<p>Player Black vs AI White</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "<div class=\"game-mode available mode-4\" onclick=\"selectGame(4)\">";
    html += "<h3>Sensor Test</h3>";
    html += "<p>Test and calibrate board sensors</p>";
    html += "<span class=\"status\">Available</span>";
    html += "</div>";
    
    html += "</div>";
    html += "<a href=\"/board-view\" class=\"button\">View Chess Board</a>";
    html += "<a href=\"/\" class=\"back-button\">Back to Configuration</a>";
    html += "</div>";
    
    html += "<script>";
    html += "function selectGame(mode) {";
    html += "if (mode === 1 || mode === 2 || mode === 3 || mode === 4) {";
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

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8]) {
    updateBoardState(newBoardState, 0.0);
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

String WiFiManagerESP32::generateBoardJSON() {
    String json = "{";
    json += "\"board\":[";
    
    for (int row = 0; row < 8; row++) {
        json += "[";
        for (int col = 0; col < 8; col++) {
            char piece = boardState[row][col];
            if (piece == ' ') {
                json += "\"\"";
            } else {
                json += "\"";
                json += String(piece);
                json += "\"";
            }
            if (col < 7) json += ",";
        }
        json += "]";
        if (row < 7) json += ",";
    }
    
    json += "],";
    json += "\"valid\":" + String(boardStateValid ? "true" : "false");
    json += ",\"evaluation\":" + String(boardEvaluation, 2);
    json += "}";
    
    return json;
}

void WiFiManagerESP32::handleBoard() {
    String boardJSON = generateBoardJSON();
    sendResponse(boardJSON, "application/json");
}

void WiFiManagerESP32::handleBoardView() {
    String boardViewPage = generateBoardViewPage();
    sendResponse(boardViewPage, "text/html");
}

String WiFiManagerESP32::generateBoardViewPage() {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<meta http-equiv=\"refresh\" content=\"2\">"; // Auto-refresh every 2 seconds
    html += "<title>OpenChess Board View</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #5c5d5e; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
    html += ".container { background-color: #353434; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); padding: 30px; }";
    html += "h2 { text-align: center; color: #ec8703; font-size: 24px; margin-bottom: 20px; }";
    html += ".board-container { display: inline-block; }";
    html += ".board { display: grid; grid-template-columns: repeat(8, 1fr); gap: 0; border: 3px solid #ec8703; width: 480px; height: 480px; }";
    html += ".square { width: 60px; height: 60px; display: flex; align-items: center; justify-content: center; font-size: 40px; font-weight: bold; }";
    html += ".square.light { background-color: #f0d9b5; }";
    html += ".square.dark { background-color: #b58863; }";
    html += ".square .piece { text-shadow: 2px 2px 4px rgba(0,0,0,0.5); }";
    html += ".square .piece.white { color: #ffffff; }";
    html += ".square .piece.black { color: #000000; }";
    html += ".info { text-align: center; color: #ec8703; margin-top: 20px; font-size: 14px; }";
    html += ".back-button { background-color: #666; color: white; border: none; padding: 15px; font-size: 16px; width: 100%; border-radius: 5px; cursor: pointer; text-decoration: none; display: block; text-align: center; margin-top: 20px; }";
    html += ".back-button:hover { background-color: #777; }";
    html += ".status { text-align: center; color: #ec8703; margin-bottom: 20px; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class=\"container\">";
    html += "<h2>CHESS BOARD</h2>";
    
    if (boardStateValid) {
        html += "<div class=\"status\">Board state: Active</div>";
        // Show evaluation if available (for Chess Bot mode)
        if (boardEvaluation != 0.0) {
            float evalInPawns = boardEvaluation / 100.0;
            String evalColor = "#ec8703";
            String evalText = "";
            if (boardEvaluation > 0) {
                evalText = "+" + String(evalInPawns, 2) + " (White advantage)";
                evalColor = "#4CAF50";
            } else {
                evalText = String(evalInPawns, 2) + " (Black advantage)";
                evalColor = "#F44336";
            }
            html += "<div class=\"status\" style=\"color: " + evalColor + ";\">";
            html += "Stockfish Evaluation: " + evalText;
            html += "</div>";
        }
        html += "<div class=\"board-container\">";
        html += "<div class=\"board\">";
        
        // Generate board squares
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                bool isLight = (row + col) % 2 == 0;
                char piece = boardState[row][col];
                
                html += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";
                
                if (piece != ' ') {
                    bool isWhite = (piece >= 'A' && piece <= 'Z');
                    String pieceSymbol = getPieceSymbol(piece);
                    html += "<span class=\"piece " + String(isWhite ? "white" : "black") + "\">" + pieceSymbol + "</span>";
                }
                
                html += "</div>";
            }
        }
        
        html += "</div>";
        html += "</div>";
    } else {
        html += "<div class=\"status\">Board state: Not available</div>";
        html += "<p style=\"text-align: center; color: #ec8703;\">No active game detected. Start a game to view the board.</p>";
    }
    
    html += "<div class=\"info\">";
    html += "<p>Auto-refreshing every 2 seconds</p>";
    html += "<div id=\"evaluation\" style=\"margin-top: 15px; padding: 15px; background-color: #444; border-radius: 5px;\">";
    html += "<div style=\"text-align: center; margin-bottom: 10px; color: #ec8703; font-weight: bold;\">Stockfish Evaluation</div>";
    html += "<div style=\"position: relative; width: 100%; height: 40px; background-color: #333; border: 2px solid #555; border-radius: 5px; overflow: hidden;\">";
    html += "<div id=\"eval-bar\" style=\"position: absolute; top: 0; left: 50%; width: 0%; height: 100%; background: linear-gradient(to right, #F44336 0%, #F44336 50%, #4CAF50 50%, #4CAF50 100%); transition: width 0.3s ease, left 0.3s ease;\"></div>";
    html += "<div style=\"position: absolute; top: 0; left: 50%; width: 2px; height: 100%; background-color: #ec8703; z-index: 2;\"></div>";
    html += "<div id=\"eval-arrow\" style=\"position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 24px; z-index: 3; color: #ec8703; transition: left 0.3s ease;\">⬌</div>";
    html += "</div>";
    html += "<div id=\"eval-text\" style=\"text-align: center; margin-top: 10px; font-size: 14px; color: #ec8703;\">--</div>";
    html += "</div>";
    html += "</div>";
    html += "<a href=\"/board-edit\" class=\"button\">Edit Board</a>";
    html += "<a href=\"/\" class=\"back-button\">Back to Configuration</a>";
    html += "</div>";
    
    html += "<script>";
    html += "// Fetch board state via AJAX for smoother updates";
    html += "function updateBoard() {";
    html += "fetch('/board')";
    html += ".then(response => response.json())";
    html += ".then(data => {";
    html += "if (data.valid) {";
    html += "// Update board display";
    html += "const squares = document.querySelectorAll('.square');";
    html += "let index = 0;";
    html += "for (let row = 0; row < 8; row++) {";
    html += "for (let col = 0; col < 8; col++) {";
    html += "const piece = data.board[row][col];";
    html += "const square = squares[index];";
    html += "if (piece && piece !== '') {";
    html += "const isWhite = piece === piece.toUpperCase();";
    html += "square.innerHTML = '<span class=\"piece ' + (isWhite ? 'white' : 'black') + '\">' + getPieceSymbol(piece) + '</span>';";
    html += "} else {";
    html += "square.innerHTML = '';";
    html += "}";
    html += "index++;";
    html += "}";
    html += "}";
    html += "// Update evaluation bar";
    html += "if (data.evaluation !== undefined) {";
    html += "const evalValue = data.evaluation;";
    html += "const evalInPawns = (evalValue / 100).toFixed(2);";
    html += "const maxEval = 1000; // Maximum evaluation to display (10 pawns)";
    html += "const clampedEval = Math.max(-maxEval, Math.min(maxEval, evalValue));";
    html += "const percentage = Math.abs(clampedEval) / maxEval * 50; // Max 50% on each side";
    html += "const bar = document.getElementById('eval-bar');";
    html += "const arrow = document.getElementById('eval-arrow');";
    html += "const text = document.getElementById('eval-text');";
    html += "let evalText = '';";
    html += "let arrowSymbol = '⬌';";
    html += "if (evalValue > 0) {";
    html += "bar.style.left = '50%';";
    html += "bar.style.width = percentage + '%';";
    html += "bar.style.background = 'linear-gradient(to right, #ec8703 0%, #4CAF50 100%)';";
    html += "arrow.style.left = (50 + percentage) + '%';";
    html += "arrowSymbol = '→';";
    html += "arrow.style.color = '#4CAF50';";
    html += "evalText = '+' + evalInPawns + ' (White advantage)';";
    html += "} else if (evalValue < 0) {";
    html += "bar.style.left = (50 - percentage) + '%';";
    html += "bar.style.width = percentage + '%';";
    html += "bar.style.background = 'linear-gradient(to right, #F44336 0%, #ec8703 100%)';";
    html += "arrow.style.left = (50 - percentage) + '%';";
    html += "arrowSymbol = '←';";
    html += "arrow.style.color = '#F44336';";
    html += "evalText = evalInPawns + ' (Black advantage)';";
    html += "} else {";
    html += "bar.style.left = '50%';";
    html += "bar.style.width = '0%';";
    html += "bar.style.background = '#ec8703';";
    html += "arrow.style.left = '50%';";
    html += "arrowSymbol = '⬌';";
    html += "arrow.style.color = '#ec8703';";
    html += "evalText = '0.00 (Equal)';";
    html += "}";
    html += "arrow.textContent = arrowSymbol;";
    html += "text.textContent = evalText;";
    html += "text.style.color = arrow.style.color;";
    html += "}";
    html += "}";
    html += "});";
    html += "}";
    html += "function getPieceSymbol(piece) {";
    html += "if (!piece) return '';";
    html += "const symbols = {";
    html += "'R': '♖', 'N': '♘', 'B': '♗', 'Q': '♕', 'K': '♔', 'P': '♙',";
    html += "'r': '♜', 'n': '♞', 'b': '♝', 'q': '♛', 'k': '♚', 'p': '♟'";
    html += "};";
    html += "return symbols[piece] || piece;";
    html += "}";
    html += "setInterval(updateBoard, 2000);";
    html += "</script>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

String WiFiManagerESP32::getPieceSymbol(char piece) {
    switch(piece) {
        case 'R': return "♖";  // White Rook
        case 'N': return "♘";  // White Knight
        case 'B': return "♗";  // White Bishop
        case 'Q': return "♕";  // White Queen
        case 'K': return "♔";  // White King
        case 'P': return "♙";  // White Pawn
        case 'r': return "♜";  // Black Rook
        case 'n': return "♞";  // Black Knight
        case 'b': return "♝";  // Black Bishop
        case 'q': return "♛";  // Black Queen
        case 'k': return "♚";  // Black King
        case 'p': return "♟";  // Black Pawn
        default: return String(piece);
    }
}

String WiFiManagerESP32::generateBoardEditPage() {
    String html = "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>Edit Chess Board</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #5c5d5e; margin: 0; padding: 20px; }";
    html += ".container { background-color: #353434; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); padding: 30px; max-width: 800px; margin: 0 auto; }";
    html += "h2 { text-align: center; color: #ec8703; font-size: 24px; margin-bottom: 20px; }";
    html += ".board-container { display: inline-block; margin: 20px auto; }";
    html += ".board { display: grid; grid-template-columns: repeat(8, 1fr); gap: 0; border: 3px solid #ec8703; width: 480px; height: 480px; }";
    html += ".square { width: 60px; height: 60px; display: flex; align-items: center; justify-content: center; position: relative; }";
    html += ".square.light { background-color: #f0d9b5; }";
    html += ".square.dark { background-color: #b58863; }";
    html += ".square:hover { background-color: #ec8703 !important; opacity: 0.8; }";
    html += ".square select { width: 100%; height: 100%; border: none; background: transparent; font-size: 32px; text-align: center; cursor: pointer; appearance: none; -webkit-appearance: none; -moz-appearance: none; }";
    html += ".square select:focus { outline: 2px solid #ec8703; }";
    html += ".controls { text-align: center; margin-top: 20px; }";
    html += ".button { background-color: #ec8703; color: white; border: none; padding: 15px 30px; font-size: 16px; border-radius: 5px; cursor: pointer; margin: 10px; }";
    html += ".button:hover { background-color: #ebca13; }";
    html += ".button.secondary { background-color: #666; }";
    html += ".button.secondary:hover { background-color: #777; }";
    html += ".info { text-align: center; color: #ec8703; margin-top: 20px; font-size: 14px; }";
    html += ".back-button { background-color: #666; color: white; border: none; padding: 15px; font-size: 16px; width: 100%; border-radius: 5px; cursor: pointer; text-decoration: none; display: block; text-align: center; margin-top: 20px; }";
    html += ".back-button:hover { background-color: #777; }";
    html += ".status { text-align: center; color: #ec8703; margin-bottom: 20px; padding: 10px; background-color: #444; border-radius: 5px; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class=\"container\">";
    html += "<h2>EDIT CHESS BOARD</h2>";
    html += "<div class=\"status\">Click on any square to change the piece. Empty = no piece.</div>";
    
    html += "<form id=\"boardForm\" method=\"POST\" action=\"/board-edit\">";
    html += "<div class=\"board-container\">";
    html += "<div class=\"board\">";
    
    // Generate editable board squares
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            bool isLight = (row + col) % 2 == 0;
            char piece = boardState[row][col];
            
            html += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";
            html += "<select name=\"r" + String(row) + "c" + String(col) + "\" id=\"r" + String(row) + "c" + String(col) + "\">";
            html += "<option value=\"\"" + String(piece == ' ' ? " selected" : "") + "></option>";
            html += "<option value=\"R\"" + String(piece == 'R' ? " selected" : "") + ">♖ R</option>";
            html += "<option value=\"N\"" + String(piece == 'N' ? " selected" : "") + ">♘ N</option>";
            html += "<option value=\"B\"" + String(piece == 'B' ? " selected" : "") + ">♗ B</option>";
            html += "<option value=\"Q\"" + String(piece == 'Q' ? " selected" : "") + ">♕ Q</option>";
            html += "<option value=\"K\"" + String(piece == 'K' ? " selected" : "") + ">♔ K</option>";
            html += "<option value=\"P\"" + String(piece == 'P' ? " selected" : "") + ">♙ P</option>";
            html += "<option value=\"r\"" + String(piece == 'r' ? " selected" : "") + ">♜ r</option>";
            html += "<option value=\"n\"" + String(piece == 'n' ? " selected" : "") + ">♞ n</option>";
            html += "<option value=\"b\"" + String(piece == 'b' ? " selected" : "") + ">♝ b</option>";
            html += "<option value=\"q\"" + String(piece == 'q' ? " selected" : "") + ">♛ q</option>";
            html += "<option value=\"k\"" + String(piece == 'k' ? " selected" : "") + ">♚ k</option>";
            html += "<option value=\"p\"" + String(piece == 'p' ? " selected" : "") + ">♟ p</option>";
            html += "</select>";
            html += "</div>";
        }
    }
    
    html += "</div>";
    html += "</div>";
    
    html += "<div class=\"controls\">";
    html += "<button type=\"submit\" class=\"button\">Apply Changes</button>";
    html += "<button type=\"button\" class=\"button secondary\" onclick=\"loadCurrentBoard()\">Reload Current Board</button>";
    html += "<button type=\"button\" class=\"button secondary\" onclick=\"clearBoard()\">Clear All</button>";
    html += "</div>";
    html += "</form>";
    
    html += "<div class=\"info\">";
    html += "<p><strong>Instructions:</strong></p>";
    html += "<p>• Uppercase letters (R,N,B,Q,K,P) = White pieces</p>";
    html += "<p>• Lowercase letters (r,n,b,q,k,p) = Black pieces</p>";
    html += "<p>• Empty = No piece on that square</p>";
    html += "<p>• Click 'Apply Changes' to update the physical board</p>";
    html += "</div>";
    
    html += "<a href=\"/board-view\" class=\"back-button\">View Board</a>";
    html += "<a href=\"/\" class=\"back-button\">Back to Configuration</a>";
    html += "</div>";
    
    html += "<script>";
    html += "function loadCurrentBoard() {";
    html += "fetch('/board')";
    html += ".then(response => response.json())";
    html += ".then(data => {";
    html += "if (data.valid) {";
    html += "for (let row = 0; row < 8; row++) {";
    html += "for (let col = 0; col < 8; col++) {";
    html += "const piece = data.board[row][col] || '';";
    html += "const select = document.getElementById('r' + row + 'c' + col);";
    html += "select.value = piece;";
    html += "}";
    html += "}";
    html += "}";
    html += "});";
    html += "}";
    html += "function clearBoard() {";
    html += "if (confirm('Clear all pieces from the board?')) {";
    html += "for (let row = 0; row < 8; row++) {";
    html += "for (let col = 0; col < 8; col++) {";
    html += "document.getElementById('r' + row + 'c' + col).value = '';";
    html += "}";
    html += "}";
    html += "}";
    html += "}";
    html += "</script>";
    html += "</body>";
    html += "</html>";
    
    return html;
}

void WiFiManagerESP32::handleBoardEdit() {
    parseBoardEditData();
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(response);
}

void WiFiManagerESP32::parseBoardEditData() {
    // Parse the form data which contains r0c0, r0c1, etc.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            String paramName = "r" + String(row) + "c" + String(col);
            
            if (server.hasArg(paramName)) {
                String value = server.arg(paramName);
                if (value.length() > 0) {
                    pendingBoardEdit[row][col] = value.charAt(0);
                } else {
                    pendingBoardEdit[row][col] = ' ';
                }
            } else {
                pendingBoardEdit[row][col] = ' ';
            }
        }
    }
    
    hasPendingEdit = true;
    Serial.println("Board edit received and stored");
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

bool WiFiManagerESP32::connectToWiFi(String ssid, String password) {
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    // ESP32 can run both AP and Station modes simultaneously
    WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        Serial.print("Connection attempt ");
        Serial.print(attempts);
        Serial.print("/20 - Status: ");
        Serial.println(WiFi.status());
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        // AP mode is still available
        return false;
    }
}

bool WiFiManagerESP32::startAccessPoint() {
    // AP is already started in begin(), this is just for compatibility
    return WiFi.softAP(AP_SSID, AP_PASSWORD);
}

IPAddress WiFiManagerESP32::getIPAddress() {
    // Return station IP if connected, otherwise AP IP
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP();
    } else {
        return WiFi.softAPIP();
    }
}

bool WiFiManagerESP32::isConnectedToWiFi() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManagerESP32::getConnectionStatus() {
    String status = "";
    if (WiFi.status() == WL_CONNECTED) {
        status = "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
        status += " | AP also available at: " + WiFi.softAPIP().toString();
    } else {
        status = "Access Point Mode - SSID: " + String(AP_SSID) + " (IP: " + WiFi.softAPIP().toString() + ")";
    }
    return status;
}

void WiFiManagerESP32::handleConnectWiFi() {
    // Parse WiFi credentials from POST
    if (server.hasArg("ssid")) {
        wifiSSID = server.arg("ssid");
    }
    if (server.hasArg("password")) {
        wifiPassword = server.arg("password");
    }
    
    if (wifiSSID.length() > 0) {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);
        
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected) {
            response += "<h2>WiFi Connected!</h2>";
            response += "<p>Successfully connected to: " + wifiSSID + "</p>";
            response += "<p>Station IP Address: " + WiFi.localIP().toString() + "</p>";
            response += "<p>Access Point still available at: " + WiFi.softAPIP().toString() + "</p>";
            response += "<p>You can access the board at either IP address.</p>";
        } else {
            response += "<h2>WiFi Connection Failed</h2>";
            response += "<p>Could not connect to: " + wifiSSID + "</p>";
            response += "<p>Please check your credentials and try again.</p>";
            response += "<p>Access Point mode is still available at: " + WiFi.softAPIP().toString() + "</p>";
        }
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(response);
    } else {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>Error</h2>";
        response += "<p>No WiFi SSID provided.</p>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(response);
    }
}

