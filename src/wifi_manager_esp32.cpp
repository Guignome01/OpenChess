#include <Arduino.h>
#include <Preferences.h>
#include "wifi_manager_esp32.h"
#include "arduino_secrets.h"

extern "C"
{
#include "nvs_flash.h"
}

WiFiManagerESP32::WiFiManagerESP32(BoardDriver *boardDriver) : server(AP_PORT)
{
    _boardDriver = boardDriver;
    apMode = true;
    clientConnected = false;
    gameMode = "None";
    boardStateValid = false;
    hasPendingEdit = false;
    boardEvaluation = 0.0;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase(); // erase and retry
        nvs_flash_init();
    }
    prefs.begin("wifiCreds", true);
    wifiSSID = prefs.getString("ssid", SECRET_SSID);
    wifiPassword = prefs.getString("pass", SECRET_PASS);
    prefs.end();

    // Initialize board state to empty
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            boardState[row][col] = ' ';
            pendingBoardEdit[row][col] = ' ';
        }
    }
}

void WiFiManagerESP32::begin()
{
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

    if (!apStarted)
    {
        Serial.println("ERROR: Failed to create Access Point!");
        return;
    }

    Serial.println("Debug: Access Point created successfully");

    // Try to connect to existing WiFi
    bool connected = connectToWiFi(wifiSSID, wifiPassword) ? Serial.println("Successfully connected to WiFi network!") : Serial.println("Failed to connect to WiFi. Access Point mode still available.");

    // Wait a moment for everything to stabilize
    delay(100);

    // Print connection information
    Serial.println("=== WiFi Connection Information ===");
    Serial.print("Access Point SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
    if (connected)
    {
        Serial.print("Connected to WiFi: ");
        Serial.println(WiFi.SSID());
        Serial.print("Station IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Access board via: http://");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.print("Access board via: http://");
        Serial.println(WiFi.softAPIP());
    }
    Serial.print("MAC Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("=====================================");

    // Set up web server routes with async handlers
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "text/html", this->indexHTML()); });

    server.on("/styles.css", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "text/css", this->stylesCSS()); });

    server.on("/game", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "text/html", this->gameModeSelectHTML()); });

    server.on("/board", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "application/json", this->boardUpdateJSON()); });

    server.on("/board-view", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "text/html", this->boardViewHTML()); });

    server.on("/board-edit", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "text/html", this->boardEditHTML()); });

    server.on("/board-edit", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleBoardEditSuccess(request); });

    server.on("/connect-wifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleConnectWiFi(request); });

    server.on("/submit", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleConfigSubmit(request); });

    server.on("/gameselect", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleGameSelection(request); });

    server.onNotFound([this](AsyncWebServerRequest *request)
                      {
        String resp = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        resp += "<h2>404 - Page Not Found</h2>";
        resp += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
        resp += "</body></html>";
        request->send(404, "text/html", resp); });

    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

String WiFiManagerESP32::stylesCSS()
{
    String resp = R"rawliteral(
    body{font-family:Arial,sans-serif;background-color:#5c5d5e;margin:0;padding:0;display:flex;justify-content:center;align-items:center;min-height:100vh}.container,.board-container{background-color:#353434;border-radius:8px;box-shadow:0 4px 8px rgb(0 0 0 / .1);padding:30px;width:100%;max-width:600px}h2{text-align:center;color:#ec8703;font-size:30px;margin-bottom:30px}h3{text-align:center;color:#ec8703;font-size:20px;margin-bottom:20px}label{font-weight:700;font-size:20px;color:#ec8703;margin-bottom:8px;display:block}input[type="text"],input[type="password"],select{width:100%;padding:11px;margin:10px 0;border:2px solid #777;border-radius:20px;box-sizing:border-box;font-size:18px}input[type="submit"],.button{background-color:#ec8703;color:#fff;border:none;padding:15px;font-size:18px;width:100%;border-radius:20px;cursor:pointer;transition:all 0.3s ease;text-decoration:none;display:block;text-align:center;margin:10px auto;box-sizing:border-box}input[type="submit"]:hover,.button:hover{filter:brightness(1.15);transform:translateY(-2px);box-shadow:0 4px 12px rgb(0 0 0 / .3)}.button.secondary{background-color:#666}.button.secondary:hover{filter:brightness(1.15);transform:translateY(-2px);box-shadow:0 4px 12px rgb(0 0 0 / .3)}.back-button{background-color:#666;color:#fff;border:none;padding:15px;font-size:18px;width:100%;border-radius:20px;cursor:pointer;text-decoration:none;display:block;text-align:center;margin:10px auto;box-sizing:border-box;transition:all 0.3s ease}.back-button:hover{filter:brightness(1.15);transform:translateY(-2px);box-shadow:0 4px 12px rgb(0 0 0 / .3)}.form-group{margin-bottom:15px}.game-grid{display:grid;grid-template-columns:1fr 1fr;gap:20px;margin-bottom:30px}.game-mode{background-color:#444;border:2px solid #ec8703;border-radius:8px;padding:20px;text-align:center;cursor:pointer;transition:all 0.3s ease;color:#fff}.game-mode:hover{background-color:#ec8703;transform:translateY(-2px)}.game-mode.available{border-color:#4CAF50}.game-mode.coming-soon{border-color:#888;opacity:.6}.game-mode.mode-1{border-color:#FF9800;background:linear-gradient(135deg,#444 0%,#FF9800 100%)}.game-mode.mode-2{border-color:#FFF;background:linear-gradient(135deg,#444 0%,#FFFFFF 100%)}.game-mode.mode-3{border-color:#2196F3;background:linear-gradient(135deg,#444 0%,#2196F3 100%)}.game-mode.mode-4{border-color:#F44336;background:linear-gradient(135deg,#444 0%,#F44336 100%)}.game-mode h3{margin:0 0 10px 0;font-size:18px}.game-mode p{margin:0;font-size:14px;opacity:.8}.board{display:grid;grid-template-columns:repeat(8,1fr);gap:0;border:3px solid #ec8703;width:480px;height:480px;margin:0 auto}.square{position:relative;width:60px;height:60px;display:flex;align-items:center;justify-content:center;font-size:40px;font-weight:700}.square.light{background-color:#f0d9b5}.square.dark{background-color:#b58863}.square .piece{text-shadow:2px 2px 4px rgb(0 0 0 / .5)}.square .piece.white{color:#fff}.square .piece.black{color:#000}.square:hover{background-color:#ec8703!important;opacity:.8}.square select{width:100%;height:100%;border:none;background:#fff0;font-size:40px;text-align:center;cursor:pointer;appearance:none;-webkit-appearance:none;-moz-appearance:none;padding:0;margin:0;line-height:60px;vertical-align:middle;color:#fff0;-webkit-text-fill-color:#fff0}.square .select-symbol{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:40px;pointer-events:none;text-shadow:2px 2px 4px rgb(0 0 0 / .5)}.square select optgroup{background:#777;color:#ec8703;font-size:18px;font-weight:700}.square select option{background:#888;color:#e0e0e0;padding:8px;font-size:18px;text-align:center}.square select:focus{outline:2px solid #ec8703}.controls{display:flex;gap:10px;margin-top:20px;margin-bottom:30px}.controls .button{flex:1;margin:0;transition:all 0.3s ease}.controls .button:hover{filter:brightness(1.15);transform:translateY(-2px);box-shadow:0 4px 12px rgb(0 0 0 / .3)}.info{text-align:center;color:#ec8703;margin:10px auto;font-size:14px}.status{text-align:center;color:#ec8703;margin:20px auto;padding:10px;background-color:#444;border-radius:20px}
    )rawliteral";
    return resp;
}

String WiFiManagerESP32::indexHTML()
{
    String resp = R"rawliteral(
    <html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>OpenChess Home</title><link rel="stylesheet" href="styles.css"></head><body><div class="container"><h2>OpenChess Home</h2><div class="status">%STATUS%</div><form action="/connect-wifi" method="POST"><div class="form-group"><label for="ssid">WiFi SSID:</label><input type="text" name="ssid" id="ssid" value="%SSID%" placeholder="Enter Your WiFi SSID"></div><div class="form-group"><label for="password">WiFi Password:</label><input type="text" name="password" id="password" value="%PASSWORD%" placeholder="Enter Your WiFi Password"></div><input type="submit" style="background-color: #4CAF50;" value="Connect to WiFi"></form><a href="/game" class="button">GameMode Selection</a><a href="/board-view" class="button">View Board</a></div></body></html>
    )rawliteral";
    resp.replace("%SSID%", wifiSSID);
    resp.replace("%PASSWORD%", wifiPassword);
    resp.replace("%STATUS%", WiFi.status() == WL_CONNECTED ? "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")" + " | AP also available at: " + WiFi.softAPIP().toString() : "Access Point Mode - SSID: " + String(AP_SSID) + " (IP: " + WiFi.softAPIP().toString() + ")");
    return resp;
}

String WiFiManagerESP32::gameModeSelectHTML()
{
    String resp = R"rawliteral(
    <meta charset=UTF-8><meta content="width=device-width,initial-scale=1"name=viewport><title>GameMode Selection</title><link href=styles.css rel=stylesheet><div class=container><h2>GameMode Selection</h2><div class=game-grid><div class="available game-mode mode-1"onclick=selectGame(1)><h3>Chess Moves</h3><p>Human vs Human<p>Visualize available moves</div><div class="available game-mode mode-2"onclick=selectGame(2)><h3>Black Bot</h3><p>Human vs Black Bot<p>(Stockfish Medium)</div><div class="available game-mode mode-3"onclick=selectGame(3)><h3>White Bot</h3><p>Human vs White Bot<p>(Stockfish Medium)</div><div class="available game-mode mode-4"onclick=selectGame(4)><h3>Sensor Test</h3><p>Test board sensors</div></div><a class=button href=/board-view>View Board</a> <a class=back-button href=/ >OpenChess Home</a></div><script>function selectGame(e){1===e||2===e||3===e||4===e?fetch("/gameselect",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"gamemode="+e}).then((e=>e.text())).then((e=>{window.location.href="/board-view"})).catch((e=>{console.error("Error:",e)})):alert("This game mode doesnt exist!")}</script>
    )rawliteral";

    return resp;
}

String WiFiManagerESP32::boardUpdateJSON()
{
    String resp = "{\"board\":[";
    for (int row = 0; row < 8; row++)
    {
        resp += "[";
        for (int col = 0; col < 8; col++)
        {
            char piece = boardState[row][col];
            if (piece == ' ')
            {
                resp += "\"\"";
            }
            else
            {
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

String WiFiManagerESP32::boardViewHTML()
{
    String resp = R"rawliteral(
    <html lang><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Chess Board</title><link rel="stylesheet" href="styles.css"></head><body><div class="container"><h2>Chess Board</h2><div class="status">Board state: <span id="state-display" style="color: #F44336;">✗ Invalid</span></div><div class="board"> %BOARD_HTML% </div><div class="info"><div id="evaluation" style="margin-top: 15px; padding: 15px; background-color: #444; border-radius: 5px;"><div style="position: relative; width: 100%; height: 40px; background-color: #333; border: 2px solid #555; border-radius: 5px; overflow: hidden;"><div id="eval-bar" style="position: absolute; top: 0; left: 50%; width: 0%; height: 100%; background: linear-gradient(to right, #F44336 0%, #F44336 50%, #4CAF50 50%, #4CAF50 100%); transition: width 0.3s ease, left 0.3s ease;"></div><div style="position: absolute; top: 0; left: 50%; width: 2px; height: 100%; background-color: #ec8703; z-index: 2;"></div><div id="eval-arrow" style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 24px; z-index: 3; color: #ec8703; transition: left 0.3s ease;"> ⬌</div></div><div id="eval-text" style="text-align: center; margin-top: 10px; font-size: 14px; color: #ec8703;">-- </div></div></div><a href="/board-edit" class="button">Edit Board</a><a href="/" class="back-button">OpenChess Home</a></div><script>function updateBoard(){fetch("/board").then((e=>e.json())).then((e=>{const t=document.getElementById("state-display");if(e.valid?(t.style.color="#4CAF50",t.textContent="✓ Valid"):(t.style.color="#F44336",t.textContent="✗ Invalid"),e.valid){const t=document.querySelectorAll(".square");let l=0;for(let n=0;n<8;n++)for(let o=0;o<8;o++){const a=e.board[n][o],r=t[l];if(a&&""!==a){const e=a===a.toUpperCase();r.innerHTML='<span class="piece '+(e?"white":"black")+'">'+getPieceSymbol(a)+"</span>"}else r.innerHTML="";l++}if(void 0!==e.evaluation){const t=e.evaluation,l=(t/100).toFixed(2),n=1e3,o=Math.max(-n,Math.min(n,t)),a=Math.abs(o)/n*50,r=document.getElementById("eval-bar"),s=document.getElementById("eval-arrow"),c=document.getElementById("eval-text");let d="",i="⬌";t>0?(r.style.left="50%",r.style.width=a+"%",r.style.background="linear-gradient(to right, #ec8703 0%, #4CAF50 100%)",s.style.left=50+a+"%",i="→",s.style.color="#4CAF50",d="+"+l+" (White advantage)"):t<0?(r.style.left=50-a+"%",r.style.width=a+"%",r.style.background="linear-gradient(to right, #F44336 0%, #ec8703 100%)",s.style.left=50-a+"%",i="←",s.style.color="#F44336",d=l+" (Black advantage)"):(r.style.left="50%",r.style.width="0%",r.style.background="#ec8703",s.style.left="50%",i="⬌",s.style.color="#ec8703",d="0.00 (Equal)"),s.textContent=i,c.textContent=d,c.style.color=s.style.color}}}))}function getPieceSymbol(e){if(!e)return"";return{R:"♖",N:"♘",B:"♗",Q:"♕",K:"♔",P:"♙",r:"♜",n:"♞",b:"♝",q:"♛",k:"♚",p:"♟"}[e.toUpperCase()]||e}setInterval(updateBoard,500);</script></body></html>
    )rawliteral";

    String boardHTML = "";
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            bool isLight = (row + col) % 2 == 0;
            char piece = boardState[row][col];

            boardHTML += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";

            if (piece != ' ')
            {
                bool isWhite = (piece >= 'A' && piece <= 'Z');
                String pieceSymbol = getPieceSymbol(piece);
                boardHTML += "<span class=\"piece " + String(isWhite ? "white" : "black") + "\">" + pieceSymbol + "</span>";
            }

            boardHTML += "</div>";
        }
    }
    resp.replace("%BOARD_HTML%", boardHTML);

    return resp;
}

String WiFiManagerESP32::boardEditHTML()
{
    String resp = R"rawliteral(
    <html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Edit Board</title><link rel="stylesheet" href="styles.css"></head><body><div class="container"><h2>Edit Board</h2><div class="status">Click on any square to change the piece</div><form id="boardForm" method="POST" action="/board-edit"><div class="board"> %BOARD_HTML% </div><div class="controls"><button type="submit" class="button" style="background-color: #28a745;">✓ Apply</button><button type="button" class="button" style="background-color: #17a2b8;" onclick="loadCurrentBoard()">↻ Reload</button><button type="button" class="button" style="background-color: #dc3545;" onclick="clearBoard()">✕ Clear</button></div></form><a href="/board-view" class="button">View Board</a><a href="/" class="back-button">OpenChess Home</a></div><script>const PIECE_SYMBOL={r:"♖",n:"♘",b:"♗",q:"♕",k:"♔",p:"♙"};function loadCurrentBoard(){fetch("/board").then((e=>e.json())).then((e=>{if(e.valid)for(let t=0;t<8;t++)for(let o=0;o<8;o++){const n=e.board[t][o]||"",r=document.getElementById("r"+t+"c"+o);r.value=n,r.dispatchEvent(new Event("change"))}}))}function clearBoard(){if(confirm("Clear all pieces from the board?"))for(let e=0;e<8;e++)for(let t=0;t<8;t++){const o=document.getElementById("r"+e+"c"+t);o.value="",o.dispatchEvent(new Event("change"))}}document.addEventListener("DOMContentLoaded",(()=>{document.querySelectorAll(".square").forEach((e=>{const t=e.querySelector("select");if(!t)return;let o=e.querySelector(".select-symbol");o||(o=document.createElement("span"),o.className="select-symbol",e.appendChild(o)),t.querySelectorAll("optgroup").forEach(((e,t)=>{const o=0===t?"#ffffff":"#222222";e.querySelectorAll("option").forEach((e=>e.style.color=o))}));const n=()=>{const e=t.value;if(!e)return void(o.textContent="");const n=PIECE_SYMBOL[e.toLowerCase()]||"";o.textContent=n,o.style.color=e===e.toUpperCase()?"#ffffff":"#111111"};t.addEventListener("change",n),n()}))}));</script></body></html>
    )rawliteral";

    // Generate editable board squares
    String boardHTML = "";
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            bool isLight = (row + col) % 2 == 0;
            char piece = boardState[row][col];
            boardHTML += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";
            boardHTML += "<select name=\"r" + String(row) + "c" + String(col) + "\" id=\"r" + String(row) + "c" + String(col) + "\">";
            boardHTML += "<optgroup label=\"White Pieces\">";
            boardHTML += "<option value=\"R\"" + String(piece == 'R' ? " selected" : "") + ">♖ Rook</option>";
            boardHTML += "<option value=\"N\"" + String(piece == 'N' ? " selected" : "") + ">♘ Knight</option>";
            boardHTML += "<option value=\"B\"" + String(piece == 'B' ? " selected" : "") + ">♗ Bishop</option>";
            boardHTML += "<option value=\"Q\"" + String(piece == 'Q' ? " selected" : "") + ">♕ Queen</option>";
            boardHTML += "<option value=\"K\"" + String(piece == 'K' ? " selected" : "") + ">♔ King</option>";
            boardHTML += "<option value=\"P\"" + String(piece == 'P' ? " selected" : "") + ">♙ Pawn</option>";
            boardHTML += "</optgroup><optgroup label=\"Black Pieces\">";
            boardHTML += "<option value=\"r\"" + String(piece == 'r' ? " selected" : "") + ">♖ Rook</option>";
            boardHTML += "<option value=\"n\"" + String(piece == 'n' ? " selected" : "") + ">♘ Knight</option>";
            boardHTML += "<option value=\"b\"" + String(piece == 'b' ? " selected" : "") + ">♗ Bishop</option>";
            boardHTML += "<option value=\"q\"" + String(piece == 'q' ? " selected" : "") + ">♕ Queen</option>";
            boardHTML += "<option value=\"k\"" + String(piece == 'k' ? " selected" : "") + ">♔ King</option>";
            boardHTML += "<option value=\"p\"" + String(piece == 'p' ? " selected" : "") + ">♙ Pawn</option>";
            boardHTML += "<option value=\"\"" + String(piece == ' ' ? " selected" : "") + ">-</option>";
            boardHTML += "</optgroup></select></div>";
        }
    }
    resp.replace("%BOARD_HTML%", boardHTML);

    return resp;
}

void WiFiManagerESP32::handleBoardEditSuccess(AsyncWebServerRequest *request)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            String paramName = "r" + String(row) + "c" + String(col);
            if (request->hasArg(paramName.c_str()))
            {
                String value = request->arg(paramName.c_str());
                if (value.length() > 0)
                    pendingBoardEdit[row][col] = value.charAt(0);
                else
                    pendingBoardEdit[row][col] = ' ';
            }
            else
            {
                pendingBoardEdit[row][col] = ' ';
            }
        }
    }
    hasPendingEdit = true;
    Serial.println("Board edit received and stored");

    String resp = R"rawliteral(
<html>
<body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>
    <h2>Board Updated!</h2>
    <p>Your board changes have been applied.</p>
    <p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>
    <p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>
    <p><a href='/' style='color:#ec8703;'>Back to Home</a></p>
</body>
</html>
    )rawliteral";
    request->send(200, "text/html", resp);
}

void WiFiManagerESP32::handleConnectWiFi(AsyncWebServerRequest *request)
{
    if (request->hasArg("ssid"))
        wifiSSID = request->arg("ssid");
    if (request->hasArg("password"))
        wifiPassword = request->arg("password");

    if (wifiSSID.length() > 0 && wifiPassword.length() > 0)
    {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);

        String resp = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected)
        {
            resp += "<h2>WiFi Connected!</h2>";
            resp += "<p>Successfully connected to: " + wifiSSID + "</p>";
            resp += "<p>Station IP Address: " + WiFi.localIP().toString() + "</p>";
            resp += "<p>Access Point still available at: " + WiFi.softAPIP().toString() + "</p>";
            resp += "<p>You can access the board at either IP address.</p>";
            prefs.begin("wifiCreds", false);
            prefs.putString("ssid", wifiSSID);
            prefs.putString("pass", wifiPassword);
            prefs.end();
        }
        else
        {
            resp += "<h2>WiFi Connection Failed</h2>";
            resp += "<p>Could not connect to: " + wifiSSID + "</p>";
            resp += "<p>Please check your credentials and try again.</p>";
            resp += "<p>Access Point mode is still available at: " + WiFi.softAPIP().toString() + "</p>";
        }
        resp += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        resp += "</body></html>";
        request->send(200, "text/html", resp);
    }
    else
    {
        String resp = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        resp += "<h2>Error</h2>";
        resp += "<p>No WiFi SSID or Password provided.</p>";
        resp += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        resp += "</body></html>";
        request->send(200, "text/html", resp);
    }
}

void WiFiManagerESP32::handleConfigSubmit(AsyncWebServerRequest *request)
{
    // Parse form data from async request
    if (request->hasArg("ssid"))
        wifiSSID = request->arg("ssid");
    if (request->hasArg("password"))
        wifiPassword = request->arg("password");
    Serial.println("Configuration updated:");
    Serial.println("SSID: " + wifiSSID);
    Serial.println("Password: " + wifiPassword);
    prefs.begin("wifiCreds", false);
    prefs.putString("ssid", wifiSSID);
    prefs.putString("pass", wifiPassword);
    prefs.end();

    String resp = R"rawliteral(
    <meta charset=UTF-8><meta content="width=device-width,initial-scale=1"name=viewport><title>OpenChess</title><link href=styles.css rel=stylesheet><div class=container><h2>Configuration Saved!</h2><h3><span>WiFi SSID:</span> <span style=color:#fff>%SSID%</span></h3><h3><span>WiFi Password:</span> <span style=color:#fff>%PASSWORD%</span></h3><a class=button href=/game>GameMode Selection</a></div>
    )rawliteral";
    resp.replace("%SSID%", wifiSSID);
    resp.replace("%PASSWORD%", wifiPassword);
    request->send(200, "text/html", resp);
}

void WiFiManagerESP32::handleGameSelection(AsyncWebServerRequest *request)
{
    int mode = 0;
    if (request->hasArg("gamemode"))
        mode = request->arg("gamemode").toInt();
    gameMode = String(mode);
    Serial.println("Game mode selected via web: " + gameMode);
    String resp = "{\"status\":\"success\",\"message\":\"Game mode selected\",\"mode\":" + gameMode + "}";
    request->send(200, "application/json", resp);
}

String WiFiManagerESP32::getPieceSymbol(char piece)
{
    switch (piece)
    {
    case 'R':
        return "♖"; // White Rook
    case 'N':
        return "♘"; // White Knight
    case 'B':
        return "♗"; // White Bishop
    case 'Q':
        return "♕"; // White Queen
    case 'K':
        return "♔"; // White King
    case 'P':
        return "♙"; // White Pawn
    case 'r':
        return "♖"; // Black Rook
    case 'n':
        return "♘"; // Black Knight
    case 'b':
        return "♗"; // Black Bishop
    case 'q':
        return "♕"; // Black Queen
    case 'k':
        return "♔"; // Black King
    case 'p':
        return "♙"; // Black Pawn
    default:
        return " "; // Empty square
    }
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
}

bool WiFiManagerESP32::getPendingBoardEdit(char editBoard[8][8])
{
    if (hasPendingEdit)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                editBoard[row][col] = pendingBoardEdit[row][col];
            }
        }
        return true;
    }
    return false;
}

void WiFiManagerESP32::clearPendingEdit()
{
    hasPendingEdit = false;
}

bool WiFiManagerESP32::connectToWiFi(String ssid, String password)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Already connected to WiFi");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    }
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);

    // ESP32 can run both AP and Station modes simultaneously
    WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes

    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        _boardDriver->showConnectingAnimation();
        attempts++;
        Serial.print("Connection attempt ");
        Serial.print(attempts);
        Serial.print("/10 - Status: ");
        Serial.println(WiFi.status());
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    }
    else
    {
        Serial.println("Failed to connect to WiFi");
        // AP mode is still available
        return false;
    }
}

bool WiFiManagerESP32::isClientConnected()
{
    return WiFi.softAPgetStationNum() > 0;
}
