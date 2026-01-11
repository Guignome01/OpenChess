#include "wifi_manager.h"

// Only compile this file for WiFiNINA boards
#ifdef WIFI_MANAGER_WIFININA_ENABLED

#include <Arduino.h>
#include "arduino_secrets.h"

WiFiManager::WiFiManager() : server(AP_PORT) {
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

void WiFiManager::begin() {
    Serial.println("!!! WIFI MANAGER BEGIN FUNCTION CALLED !!!");
    Serial.println("=== Starting OpenChess WiFi Manager ===");
    Serial.println("Debug: WiFi Manager begin() called");
    
    // Check if WiFi is available
    Serial.println("Debug: Checking WiFi module...");
    
    // Try to get WiFi status - this often fails on incompatible boards
    Serial.println("Debug: Attempting to get WiFi status...");
    int initialStatus = WiFi.status();
    Serial.print("Debug: Initial WiFi status: ");
    Serial.println(initialStatus);
    
    // Initialize WiFi module
    Serial.println("Debug: Checking for WiFi module presence...");
    if (initialStatus == WL_NO_MODULE) {
        Serial.println("ERROR: WiFi module not detected!");
        Serial.println("Board type: Arduino Nano RP2040 - WiFi not supported with WiFiNINA");
        Serial.println("This is expected behavior for RP2040 boards.");
        Serial.println("Use physical board selectors for game mode selection.");
        return;
    }
    
    Serial.println("Debug: WiFi module appears to be present");
    
    Serial.println("Debug: WiFi module detected");
    
    // Check firmware version
    String fv = WiFi.firmwareVersion();
    Serial.print("Debug: WiFi firmware version: ");
    Serial.println(fv);
    
    // Try to connect to existing WiFi first (if credentials are available)
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
            apMode = false;
        } else {
            Serial.println("Failed to connect to WiFi. Starting Access Point mode...");
        }
    }
    
    // If not connected, start Access Point
    if (!connected) {
        startAccessPoint();
    }
    
    // Print connection information
    IPAddress ip = getIPAddress();
    Serial.println("=== WiFi Connection Information ===");
    if (apMode) {
        Serial.print("Mode: Access Point");
        Serial.print("SSID: ");
        Serial.println(AP_SSID);
        Serial.print("Password: ");
        Serial.println(AP_PASSWORD);
    } else {
        Serial.println("Mode: Connected to WiFi Network");
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
    }
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.print("Web Interface: http://");
    Serial.println(ip);
    Serial.println("=====================================");
    
    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

void WiFiManager::handleClient() {
    WiFiClient client = server.available();
    
    if (client) {
        clientConnected = true;
        Serial.println("New client connected");
        
        String request = "";
        bool currentLineIsBlank = true;
        bool readingBody = false;
        String body = "";
        
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                
                if (!readingBody) {
                    request += c;
                    
                    if (c == '\n' && currentLineIsBlank) {
                        // Headers ended, now reading body if POST
                        if (request.indexOf("POST") >= 0) {
                            readingBody = true;
                        } else {
                            break; // GET request, no body
                        }
                    }
                    
                    if (c == '\n') {
                        currentLineIsBlank = true;
                    } else if (c != '\r') {
                        currentLineIsBlank = false;
                    }
                } else {
                    // Reading POST body
                    body += c;
                    if (body.length() > 1000) break; // Prevent overflow
                }
            }
        }
        
        // Handle the request
        if (request.indexOf("GET / ") >= 0) {
            // Main configuration page
            String webpage = generateWebPage();
            sendResponse(client, webpage);
        }
        else if (request.indexOf("GET /game") >= 0) {
            // Game selection page
            String gameSelectionPage = generateGameSelectionPage();
            sendResponse(client, gameSelectionPage);
        }
        else if (request.indexOf("GET /board") >= 0) {
            // Board state JSON API
            String boardJSON = generateBoardJSON();
            sendResponse(client, boardJSON, "application/json");
        }
        else if (request.indexOf("GET /board-view") >= 0) {
            // Board visual display page
            String boardViewPage = generateBoardViewPage();
            sendResponse(client, boardViewPage);
        }
        else if (request.indexOf("GET /board-edit") >= 0) {
            // Board edit page
            String boardEditPage = generateBoardEditPage();
            sendResponse(client, boardEditPage);
        }
        else if (request.indexOf("POST /board-edit") >= 0) {
            // Board edit submission
            handleBoardEdit(client, request, body);
        }
        else if (request.indexOf("POST /connect-wifi") >= 0) {
            // WiFi connection request
            handleConnectWiFi(client, request, body);
        }
        else if (request.indexOf("POST /submit") >= 0) {
            // Configuration form submission
            parseFormData(body);
            String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
            response += "<h2>Configuration Saved!</h2>";
            response += "<p>WiFi SSID: " + wifiSSID + "</p>";
            response += "<p>Game Mode: " + gameMode + "</p>";
            response += "<p>Startup Type: " + startupType + "</p>";
            response += "<p><a href='/game' style='color:#ec8703;'>Go to Game Selection</a></p>";
            response += "</body></html>";
            sendResponse(client, response);
        }
        else if (request.indexOf("POST /gameselect") >= 0) {
            // Game selection submission
            handleGameSelection(client, body);
        }
        else {
            // 404 Not Found
            String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
            response += "<h2>404 - Page Not Found</h2>";
            response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
            response += "</body></html>";
            sendResponse(client, response, "text/html");
        }
        
        delay(10);
        client.stop();
        Serial.println("Client disconnected");
        clientConnected = false;
    }
}

String WiFiManager::generateWebPage() {
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
    if (apMode) {
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

String WiFiManager::generateGameSelectionPage() {
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

void WiFiManager::handleGameSelection(WiFiClient& client, String body) {
    // Parse game mode selection
    int modeStart = body.indexOf("gamemode=");
    if (modeStart >= 0) {
        int modeEnd = body.indexOf("&", modeStart);
        if (modeEnd < 0) modeEnd = body.length();
        
        String selectedMode = body.substring(modeStart + 9, modeEnd);
        int mode = selectedMode.toInt();
        
        Serial.print("Game mode selected via web: ");
        Serial.println(mode);
        
        // Store the selected game mode (you'll access this from main code)
        gameMode = String(mode);
        
        String response = R"({"status":"success","message":"Game mode selected","mode":)" + String(mode) + "}";
        sendResponse(client, response, "application/json");
    }
}

void WiFiManager::sendResponse(WiFiClient& client, String content, String contentType) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + contentType);
    client.println("Connection: close");
    client.println();
    client.println(content);
}

void WiFiManager::parseFormData(String data) {
    // Parse URL-encoded form data
    int ssidStart = data.indexOf("ssid=");
    if (ssidStart >= 0) {
        int ssidEnd = data.indexOf("&", ssidStart);
        if (ssidEnd < 0) ssidEnd = data.length();
        wifiSSID = data.substring(ssidStart + 5, ssidEnd);
        wifiSSID.replace("+", " ");
    }
    
    int passStart = data.indexOf("password=");
    if (passStart >= 0) {
        int passEnd = data.indexOf("&", passStart);
        if (passEnd < 0) passEnd = data.length();
        wifiPassword = data.substring(passStart + 9, passEnd);
    }
    
    int tokenStart = data.indexOf("token=");
    if (tokenStart >= 0) {
        int tokenEnd = data.indexOf("&", tokenStart);
        if (tokenEnd < 0) tokenEnd = data.length();
        lichessToken = data.substring(tokenStart + 6, tokenEnd);
    }
    
    int gameModeStart = data.indexOf("gameMode=");
    if (gameModeStart >= 0) {
        int gameModeEnd = data.indexOf("&", gameModeStart);
        if (gameModeEnd < 0) gameModeEnd = data.length();
        gameMode = data.substring(gameModeStart + 9, gameModeEnd);
        gameMode.replace("+", " ");
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

bool WiFiManager::isClientConnected() {
    return clientConnected;
}

int WiFiManager::getSelectedGameMode() {
    return gameMode.toInt();
}

void WiFiManager::resetGameSelection() {
    gameMode = "0";
}

void WiFiManager::updateBoardState(char newBoardState[8][8]) {
    updateBoardState(newBoardState, 0.0);
}

void WiFiManager::updateBoardState(char newBoardState[8][8], float evaluation) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
}

String WiFiManager::generateBoardJSON() {
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

String WiFiManager::generateBoardViewPage() {
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
    html += "<a href=\"/game\" class=\"back-button\">Game Selection</a>";
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

String WiFiManager::getPieceSymbol(char piece) {
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

String WiFiManager::generateBoardEditPage() {
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
    
    html += "<a href=\"/board-edit\" class=\"button\">Edit Board</a>";
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

void WiFiManager::handleBoardEdit(WiFiClient& client, String request, String body) {
    parseBoardEditData(body);
    
    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(client, response);
}

void WiFiManager::parseBoardEditData(String data) {
    // Parse the form data which contains r0c0, r0c1, etc.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            String paramName = "r" + String(row) + "c" + String(col) + "=";
            int paramStart = data.indexOf(paramName);
            
            if (paramStart >= 0) {
                int valueStart = paramStart + paramName.length();
                int valueEnd = data.indexOf("&", valueStart);
                if (valueEnd < 0) valueEnd = data.length();
                
                String value = data.substring(valueStart, valueEnd);
                value.replace("+", " ");
                value.replace("%20", " ");
                
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

bool WiFiManager::getPendingBoardEdit(char editBoard[8][8]) {
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

void WiFiManager::clearPendingEdit() {
    hasPendingEdit = false;
}

bool WiFiManager::connectToWiFi(String ssid, String password) {
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    int attempts = 0;
    WiFi.begin(ssid.c_str(), password.c_str());
    
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
        apMode = false;
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
}

bool WiFiManager::startAccessPoint() {
    Serial.println("=== Starting Access Point ===");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    
    int status = WiFi.beginAP(AP_SSID, AP_PASSWORD);
    
    if (status != WL_AP_LISTENING) {
        Serial.println("First attempt failed, trying with channel 6...");
        status = WiFi.beginAP(AP_SSID, AP_PASSWORD, 6);
    }
    
    if (status != WL_AP_LISTENING) {
        Serial.println("ERROR: Failed to create Access Point!");
        return false;
    }
    
    // Wait for AP to start
    for (int i = 0; i < 10; i++) {
        delay(1000);
        if (WiFi.status() == WL_AP_LISTENING) {
            Serial.println("AP is now listening!");
            break;
        }
    }
    
    apMode = true;
    return true;
}

IPAddress WiFiManager::getIPAddress() {
    if (apMode) {
        return WiFi.localIP(); // In AP mode, localIP() returns AP IP
    } else {
        return WiFi.localIP(); // In station mode, localIP() returns assigned IP
    }
}

bool WiFiManager::isConnectedToWiFi() {
    return !apMode && WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getConnectionStatus() {
    String status = "";
    if (apMode) {
        status = "Access Point Mode - SSID: " + String(AP_SSID);
    } else if (WiFi.status() == WL_CONNECTED) {
        status = "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
    } else {
        status = "Not connected";
    }
    return status;
}

void WiFiManager::handleConnectWiFi(WiFiClient& client, String request, String body) {
    // Parse WiFi credentials from POST body
    parseFormData(body);
    
    if (wifiSSID.length() > 0) {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);
        
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected) {
            response += "<h2>WiFi Connected!</h2>";
            response += "<p>Successfully connected to: " + wifiSSID + "</p>";
            response += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
            response += "<p>You can now access the board at: http://" + WiFi.localIP().toString() + "</p>";
        } else {
            response += "<h2>WiFi Connection Failed</h2>";
            response += "<p>Could not connect to: " + wifiSSID + "</p>";
            response += "<p>Please check your credentials and try again.</p>";
            response += "<p>Access Point mode will remain active.</p>";
        }
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(client, response);
    } else {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>Error</h2>";
        response += "<p>No WiFi SSID provided.</p>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(client, response);
    }
}

#endif // WIFI_MANAGER_WIFININA_ENABLED