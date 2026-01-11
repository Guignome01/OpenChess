#include "chess_bot.h"
#include <Arduino.h>

ChessBot::ChessBot(BoardDriver* boardDriver, ChessEngine* chessEngine, BotDifficulty diff, bool playerWhite) {
    _boardDriver = boardDriver;
    _chessEngine = chessEngine;
    difficulty = diff;
    playerIsWhite = playerWhite;
    
    // Set difficulty settings
    switch(difficulty) {
        case BOT_EASY: settings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: settings = StockfishSettings::medium(); break;
        case BOT_HARD: settings = StockfishSettings::hard(); break;
        case BOT_EXPERT: settings = StockfishSettings::expert(); break;
    }
    
    // Set initial turn: In chess, White always moves first
    // If player is White, player goes first (isWhiteTurn = true)
    // If player is Black, bot (White) goes first (isWhiteTurn = true)
    isWhiteTurn = true;  // White always moves first in chess
    gameStarted = false;
    botThinking = false;
    wifiConnected = false;
    currentEvaluation = 0.0;
}

void ChessBot::begin() {
    Serial.println("=== Starting Chess Bot Mode ===");
    Serial.print("Player plays: ");
    Serial.println(playerIsWhite ? "White" : "Black");
    Serial.print("Bot plays: ");
    Serial.println(playerIsWhite ? "Black" : "White");
    Serial.print("Bot Difficulty: ");
    
    switch(difficulty) {
        case BOT_EASY: Serial.println("Easy (Depth 6)"); break;
        case BOT_MEDIUM: Serial.println("Medium (Depth 10)"); break;
        case BOT_HARD: Serial.println("Hard (Depth 14)"); break;
        case BOT_EXPERT: Serial.println("Expert (Depth 16)"); break;
    }
    
    _boardDriver->clearAllLEDs();
    _boardDriver->showLEDs();
    
    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    showConnectionStatus();
    
    if (connectToWiFi()) {
        Serial.println("WiFi connected! Bot mode ready.");
        wifiConnected = true;
        
        // Show success animation
        for (int i = 0; i < 3; i++) {
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(200);
            
            // Light up entire board green briefly
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    _boardDriver->setSquareLED(row, col, 0, 255, 0); // Green
                }
            }
            _boardDriver->showLEDs();
            delay(200);
        }
        
        initializeBoard();
        waitForBoardSetup();
    } else {
        Serial.println("Failed to connect to WiFi. Bot mode unavailable.");
        wifiConnected = false;
        
        // Show error animation (red flashing)
        for (int i = 0; i < 5; i++) {
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(300);
            
            // Light up entire board red briefly
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    _boardDriver->setSquareLED(row, col, 255, 0, 0); // Red
                }
            }
            _boardDriver->showLEDs();
            delay(300);
        }
        
        _boardDriver->clearAllLEDs();
        _boardDriver->showLEDs();
    }
}

void ChessBot::update() {
    if (!wifiConnected) {
        return; // No WiFi, can't play against bot
    }
    
    if (!gameStarted) {
        return; // Waiting for initial setup
    }
    
    if (botThinking) {
        showBotThinking();
        return;
    }
    
    _boardDriver->readSensors();
    
    // Detect piece movements (player's turn)
    bool isPlayerTurn = (playerIsWhite && isWhiteTurn) || (!playerIsWhite && !isWhiteTurn);
    if (isPlayerTurn) {
        static unsigned long lastTurnDebug = 0;
        if (millis() - lastTurnDebug > 5000) {
            Serial.print("Your turn! Move a ");
            Serial.println(playerIsWhite ? "WHITE piece (uppercase letters)" : "BLACK piece (lowercase letters)");
            lastTurnDebug = millis();
        }
        // Look for piece pickups and placements
        static int selectedRow = -1, selectedCol = -1;
        static bool piecePickedUp = false;
        
        // Check for piece pickup
        if (!piecePickedUp) {
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    if (!_boardDriver->getSensorState(row, col) && _boardDriver->getSensorPrev(row, col)) {
                        // Check what piece was picked up
                        char piece = board[row][col];
                        
                        if (piece != ' ') {
                            // Player should only be able to move their own pieces
                            bool isPlayerPiece = (playerIsWhite && piece >= 'A' && piece <= 'Z') || 
                                                 (!playerIsWhite && piece >= 'a' && piece <= 'z');
                            if (isPlayerPiece) {
                            selectedRow = row;
                            selectedCol = col;
                            piecePickedUp = true;
                            
                            Serial.print("Player picked up ");
                            Serial.print(playerIsWhite ? "WHITE" : "BLACK");
                            Serial.print(" piece '");
                            Serial.print(board[row][col]);
                            Serial.print("' at ");
                            Serial.print((char)('a' + col));
                            Serial.print(8 - row);
                            Serial.print(" (array position ");
                            Serial.print(row);
                            Serial.print(",");
                            Serial.print(col);
                            Serial.println(")");
                            
                            // Show selected square
                            _boardDriver->setSquareLED(row, col, 255, 0, 0); // Red
                            
                            // Show possible moves
                            int moveCount = 0;
                            int moves[27][2];
                            _chessEngine->getPossibleMoves(board, row, col, moveCount, moves);
                            
                            for (int i = 0; i < moveCount; i++) {
                                _boardDriver->setSquareLED(moves[i][0], moves[i][1], 0, 0, 0, 255); // Bright white using W channel
                            }
                            _boardDriver->showLEDs();
                            break;
                            } else {
                                // Player tried to pick up the wrong color piece
                                Serial.print("ERROR: You tried to pick up ");
                                Serial.print((piece >= 'A' && piece <= 'Z') ? "WHITE" : "BLACK");
                                Serial.print(" piece '");
                                Serial.print(piece);
                                Serial.print("' at ");
                                Serial.print((char)('a' + col));
                                Serial.print(8 - row);
                                Serial.print(". You can only move ");
                                Serial.print(playerIsWhite ? "WHITE" : "BLACK");
                                Serial.println(" pieces!");
                                
                                // Flash red to indicate error
                                _boardDriver->blinkSquare(row, col, 3);
                            }
                        }
                    }
                }
                if (piecePickedUp) break;
            }
        }
        
        // Check for piece placement
        if (piecePickedUp) {
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    if (_boardDriver->getSensorState(row, col) && !_boardDriver->getSensorPrev(row, col)) {
                        // Check if piece was returned to its original position
                        if (row == selectedRow && col == selectedCol) {
                            // Piece returned to original position - cancel selection
                            Serial.println("Piece returned to original position. Selection cancelled.");
                            piecePickedUp = false;
                            selectedRow = selectedCol = -1;
                            
                            // Clear all indicators
                            _boardDriver->clearAllLEDs();
                            _boardDriver->showLEDs();
                            break;
                        }
                        
                        // Piece placed somewhere else - validate move
                        int moveCount = 0;
                        int moves[27][2];
                        _chessEngine->getPossibleMoves(board, selectedRow, selectedCol, moveCount, moves);
                        
                        bool validMove = false;
                        for (int i = 0; i < moveCount; i++) {
                            if (moves[i][0] == row && moves[i][1] == col) {
                                validMove = true;
                                break;
                            }
                        }
                        
                        if (validMove) {
                            char piece = board[selectedRow][selectedCol];
                            
                            // Complete LED animations BEFORE API request
                            processPlayerMove(selectedRow, selectedCol, row, col, piece);
                            
                            // Flash confirmation on destination square for player move
                            confirmSquareCompletion(row, col);
                            
                            piecePickedUp = false;
                            selectedRow = selectedCol = -1;
                            
                            // Switch to bot's turn
                            // If player is White, bot is Black (isWhiteTurn = false)
                            // If player is Black, bot is White (isWhiteTurn = true)
                            isWhiteTurn = !playerIsWhite;
                            botThinking = true;
                            
                            Serial.println("Player move completed. Bot thinking...");
                            
                            // Start bot move calculation
                            makeBotMove();
                        } else {
                            Serial.println("Invalid move! Please try again.");
                            _boardDriver->blinkSquare(row, col, 3); // Blink red for invalid move
                            
                            // Restore move indicators - piece is still selected
                            _boardDriver->clearAllLEDs();
                            
                            // Show selected square again
                            _boardDriver->setSquareLED(selectedRow, selectedCol, 255, 0, 0); // Red
                            
                            // Show possible moves again
                            int moveCount = 0;
                            int moves[27][2];
                            _chessEngine->getPossibleMoves(board, selectedRow, selectedCol, moveCount, moves);
                            
                            for (int i = 0; i < moveCount; i++) {
                                _boardDriver->setSquareLED(moves[i][0], moves[i][1], 0, 0, 0, 255); // Bright white using W channel
                            }
                            _boardDriver->showLEDs();
                            
                            Serial.println("Piece is still selected. Place it on a valid move or return it to its original position.");
                        }
                        break;
                    }
                }
            }
        }
    } else {
        // Bot's turn - if player is Black, bot (White) goes first
        if (!botThinking && !playerIsWhite && isWhiteTurn) {
            // Bot plays White and it's White's turn
            botThinking = true;
            makeBotMove();
        } else if (!botThinking && playerIsWhite && !isWhiteTurn) {
            // Bot plays Black and it's Black's turn
            botThinking = true;
            makeBotMove();
        }
    }
    
    _boardDriver->updateSensorPrev();
}

bool ChessBot::connectToWiFi() {
#if defined(ESP32) || defined(ESP8266)
    // ESP32/ESP8266 WiFi connection
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    
    WiFi.mode(WIFI_STA);  // Set WiFi to station mode
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    
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
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
    // WiFiNINA boards
    // Check for WiFi module
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("WiFi module not found!");
        return false;
    }
    
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        WiFi.begin(SECRET_SSID, SECRET_PASS);
        delay(5000);
        attempts++;
        
        Serial.print("Connection attempt ");
        Serial.print(attempts);
        Serial.print("/10 - Status: ");
        Serial.println(WiFi.status());
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
#else
    Serial.println("WiFi not supported on this board");
    return false;
#endif
}

String ChessBot::makeStockfishRequest(String fen) {
    WiFiSSLClient client;
    
#if defined(ESP32) || defined(ESP8266)
    // ESP32/ESP8266: Set insecure mode for SSL (or add proper certificate validation)
    // For production, you should validate certificates properly
    client.setInsecure();
#endif
    
    Serial.println("Making API request to Stockfish...");
    Serial.print("FEN: ");
    Serial.println(fen);
    
    // Retry logic
    for (int attempt = 1; attempt <= settings.maxRetries; attempt++) {
        Serial.print("Attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(settings.maxRetries);
        
        if (client.connect(STOCKFISH_API_URL, STOCKFISH_API_PORT)) {
            // URL encode the FEN string
            String encodedFen = urlEncode(fen);
            
            // Make HTTP GET request
            String url = String(STOCKFISH_API_PATH) + "?fen=" + encodedFen + "&depth=" + String(settings.depth);
            
            Serial.print("Request URL: ");
            Serial.println(url);
            
            client.println("GET " + url + " HTTP/1.1");
            client.println("Host: " + String(STOCKFISH_API_URL));
            client.println("Connection: close");
            client.println();
            
            // Wait for response
            unsigned long startTime = millis();
            String response = "";
            bool gotResponse = false;
            
            while (client.connected() && (millis() - startTime < settings.timeoutMs)) {
                if (client.available()) {
                    response = client.readString();
                    gotResponse = true;
                    break;
                }
                delay(10);
            }
            
            client.stop();
            
            if (gotResponse && response.length() > 0) {
                // Debug: Print raw response (truncated if too long)
                Serial.println("=== RAW API RESPONSE ===");
                if (response.length() > 500) {
                    Serial.println(response.substring(0, 500) + "... (truncated)");
                } else {
                    Serial.println(response);
                }
                Serial.println("=== END RAW RESPONSE ===");
                
                return response;
            } else {
                Serial.println("API request timeout or empty response");
                if (attempt < settings.maxRetries) {
                    Serial.println("Retrying...");
                    delay(1000); // Wait before retry
                }
            }
        } else {
            Serial.println("Failed to connect to Stockfish API");
            if (attempt < settings.maxRetries) {
                Serial.println("Retrying...");
                delay(1000); // Wait before retry
            }
        }
    }
    
    Serial.println("All API request attempts failed");
    return "";
}

bool ChessBot::parseStockfishResponse(String response, String &bestMove, float &evaluation) {
    // Initialize evaluation to 0 (neutral)
    evaluation = 0.0;
    
    // Find JSON content
    int jsonStart = response.indexOf("{");
    if (jsonStart == -1) {
        Serial.println("No JSON found in response");
        Serial.println("Response was: " + response);
        return false;
    }
    
    String json = response.substring(jsonStart);
    Serial.print("Extracted JSON: ");
    Serial.println(json);
    
    // Check if request was successful (some APIs use "success":true, others just return the move)
    bool hasSuccess = json.indexOf("\"success\"") >= 0;
    if (hasSuccess && json.indexOf("\"success\":true") == -1) {
        Serial.println("API request was not successful");
        return false;
    }
    
    // Parse evaluation - try different possible field names
    // Format 1: "evaluation": 0.5 (in pawns)
    // Format 2: "evaluation": 50 (in centipawns)
    // Format 3: "score": 0.5
    // Format 4: "cp": 50 (centipawns)
    int evalStart = json.indexOf("\"evaluation\":");
    if (evalStart == -1) {
        evalStart = json.indexOf("\"score\":");
        if (evalStart == -1) {
            evalStart = json.indexOf("\"cp\":");
            if (evalStart >= 0) {
                evalStart += 5; // Skip "cp":
            }
        } else {
            evalStart += 8; // Skip "score":
        }
    } else {
        evalStart += 14; // Skip "evaluation":
    }
    
    if (evalStart >= 0) {
        // Find the number value
        String evalStr = json.substring(evalStart);
        evalStr.trim();
        // Remove any leading whitespace or quotes
        while (evalStr.length() > 0 && (evalStr[0] == ' ' || evalStr[0] == '"' || evalStr[0] == '\'')) {
            evalStr = evalStr.substring(1);
        }
        // Find the end of the number (comma, }, or whitespace)
        int evalEnd = evalStr.length();
        for (int i = 0; i < evalStr.length(); i++) {
            char c = evalStr[i];
            if (c == ',' || c == '}' || c == ' ' || c == '\n' || c == '\r') {
                evalEnd = i;
                break;
            }
        }
        evalStr = evalStr.substring(0, evalEnd);
        evalStr.trim();
        
        if (evalStr.length() > 0) {
            evaluation = evalStr.toFloat();
            // If evaluation is small (< 10), assume it's in pawns, convert to centipawns
            // If evaluation is large (> 10), assume it's already in centipawns
            if (evaluation > -10 && evaluation < 10) {
                evaluation = evaluation * 100.0; // Convert pawns to centipawns
            }
            Serial.print("Parsed evaluation: ");
            Serial.print(evaluation);
            Serial.println(" centipawns");
        }
    } else {
        Serial.println("No evaluation field found in response");
    }
    
    // Parse bestmove field - try different possible formats
    // Format 1: "bestmove":"e2e4"
    // Format 2: "bestmove":"bestmove e2e4 ponder e7e5"
    // Format 3: {"move":"e2e4"}
    
    int bestmoveStart = json.indexOf("\"bestmove\":\"");
    if (bestmoveStart == -1) {
        // Try alternative format with just "move"
        bestmoveStart = json.indexOf("\"move\":\"");
        if (bestmoveStart == -1) {
            Serial.println("No bestmove or move field found in response");
            return false;
        }
        bestmoveStart += 8; // Skip "move":"
    } else {
        bestmoveStart += 12; // Skip "bestmove":"
    }
    
    int bestmoveEnd = json.indexOf("\"", bestmoveStart);
    if (bestmoveEnd == -1) {
        Serial.println("Invalid bestmove format - no closing quote");
        return false;
    }
    
    String fullMove = json.substring(bestmoveStart, bestmoveEnd);
    Serial.print("Full move string: ");
    Serial.println(fullMove);
    
    // Check if the move string contains "bestmove " prefix (some APIs include it)
    int moveStart = fullMove.indexOf("bestmove ");
    if (moveStart >= 0) {
        moveStart += 9; // Skip "bestmove "
        int moveEnd = fullMove.indexOf(" ", moveStart);
        if (moveEnd == -1) {
            // No ponder part, take rest of string
            bestMove = fullMove.substring(moveStart);
        } else {
            bestMove = fullMove.substring(moveStart, moveEnd);
        }
    } else {
        // Move is directly in the field value
        bestMove = fullMove;
    }
    
    // Clean up the move - remove any whitespace
    bestMove.trim();
    
    Serial.print("Parsed move: ");
    Serial.println(bestMove);
    
    // Validate move format (should be 4-5 characters like "e2e4" or "e7e8q")
    if (bestMove.length() < 4 || bestMove.length() > 5) {
        Serial.print("Invalid move length: ");
        Serial.println(bestMove.length());
        return false;
    }
    
    return true;
}

void ChessBot::makeBotMove() {
    Serial.println("=== BOT MOVE CALCULATION ===");
    Serial.print("Bot is playing as: ");
    Serial.println(isWhiteTurn ? "White" : "Black");
    
    // Show thinking animation
    showBotThinking();
    
    String fen = boardToFEN();
    Serial.print("Sending FEN to Stockfish: ");
    Serial.println(fen);
    
    String response = makeStockfishRequest(fen);
    
    if (response.length() > 0) {
        String bestMove;
        float evaluation = 0.0;
        if (parseStockfishResponse(response, bestMove, evaluation)) {
            // Store and print evaluation
            currentEvaluation = evaluation;
            Serial.print("=== STOCKFISH EVALUATION ===");
            Serial.println();
            if (evaluation > 0) {
                Serial.print("White advantage: +");
                Serial.print(evaluation / 100.0, 2);
                Serial.println(" pawns");
            } else if (evaluation < 0) {
                Serial.print("Black advantage: ");
                Serial.print(evaluation / 100.0, 2);
                Serial.println(" pawns");
            } else {
                Serial.println("Position is equal (0.00 pawns)");
            }
            Serial.print("Evaluation in centipawns: ");
            Serial.println(evaluation);
            Serial.println("============================");
            
            int fromRow, fromCol, toRow, toCol;
            if (parseMove(bestMove, fromRow, fromCol, toRow, toCol)) {
                Serial.print("Bot calculated move: ");
                Serial.println(bestMove);
                
                // Verify the move is from the correct color piece
                // Bot plays White if player is Black, Bot plays Black if player is White
                char piece = board[fromRow][fromCol];
                bool botPlaysWhite = !playerIsWhite;
                bool isBotPiece = (botPlaysWhite && piece >= 'A' && piece <= 'Z') || 
                                  (!botPlaysWhite && piece >= 'a' && piece <= 'z');
                
                if (!isBotPiece) {
                    Serial.print("ERROR: Bot tried to move a ");
                    Serial.print((piece >= 'A' && piece <= 'Z') ? "WHITE" : "BLACK");
                    Serial.print(" piece, but bot plays ");
                    Serial.println(botPlaysWhite ? "WHITE" : "BLACK");
                    Serial.print("Piece at source: ");
                    Serial.println(piece);
                    botThinking = false;
                    return;
                }
                
                if (piece == ' ') {
                    Serial.println("ERROR: Bot tried to move from an empty square!");
                    botThinking = false;
                    return;
                }
                
                executeBotMove(fromRow, fromCol, toRow, toCol);
                
                // Switch back to player's turn
                // If player is White, isWhiteTurn = true; if player is Black, isWhiteTurn = false
                isWhiteTurn = playerIsWhite;
                botThinking = false;
                
                Serial.println("Bot move completed. Your turn!");
            } else {
                Serial.print("Failed to parse bot move: ");
                Serial.println(bestMove);
                botThinking = false;
            }
        } else {
            Serial.println("Failed to parse Stockfish response");
            Serial.print("Response was: ");
            if (response.length() > 200) {
                Serial.println(response.substring(0, 200) + "... (truncated)");
            } else {
                Serial.println(response);
            }
            botThinking = false;
        }
    } else {
        Serial.println("No response from Stockfish API after all retries");
        botThinking = false;
    }
}

String ChessBot::boardToFEN() {
    String fen = "";
    
    // Board position - FEN expects rank 8 (black pieces) first, rank 1 (white pieces) last
    // Our array: row 0 = rank 1 (white pieces at bottom), row 7 = rank 8 (black pieces at top)
    // So we need to reverse the order: start from row 7 and go to row 0
    for (int row = 7; row >= 0; row--) {
        int emptyCount = 0;
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == ' ') {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += String(emptyCount);
                    emptyCount = 0;
                }
                fen += board[row][col];
            }
        }
        if (emptyCount > 0) {
            fen += String(emptyCount);
        }
        if (row > 0) fen += "/";
    }
    
    // Active color - when we call this for bot's move, isWhiteTurn is false (bot is Black)
    // So we correctly indicate it's Black's turn
    fen += isWhiteTurn ? " w" : " b";
    
    // Castling availability (simplified - assume all available initially)
    fen += " KQkq";
    
    // En passant target square (simplified - assume none)
    fen += " -";
    
    // Halfmove clock (simplified)
    fen += " 0";
    
    // Fullmove number (simplified)
    fen += " 1";
    
    Serial.print("Generated FEN: ");
    Serial.println(fen);
    Serial.print("Active color: ");
    Serial.println(isWhiteTurn ? "White" : "Black");
    
    return fen;
}

void ChessBot::fenToBoard(String fen) {
    // Parse FEN string and update board state
    // FEN format: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    // We only parse the board part (first part before space)
    
    int spacePos = fen.indexOf(' ');
    if (spacePos > 0) {
        fen = fen.substring(0, spacePos);
    }
    
    // Clear board
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = ' ';
        }
    }
    
    // Parse FEN ranks (rank 8 first, rank 1 last)
    // FEN: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
    // Our array: row 0 = rank 1, row 7 = rank 8
    int rank = 7; // Start with rank 8 (row 7 in our array)
    int col = 0;
    
    for (int i = 0; i < fen.length() && rank >= 0; i++) {
        char c = fen.charAt(i);
        
        if (c == '/') {
            // Next rank
            rank--;
            col = 0;
        } else if (c >= '1' && c <= '8') {
            // Empty squares
            int emptyCount = c - '0';
            col += emptyCount;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            // Piece
            if (rank >= 0 && rank < 8 && col >= 0 && col < 8) {
                board[rank][col] = c;
                col++;
            }
        }
    }
    
    Serial.println("Board updated from FEN");
    printCurrentBoard();
}

bool ChessBot::parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol) {
    if (move.length() < 4) {
        Serial.print("Move too short: ");
        Serial.println(move);
        return false;
    }
    
    // Parse chess notation (e.g., "e2e4")
    // File (column): a-h -> 0-7
    // Rank (row): 1-8 -> In our array: rank 1 = row 0, rank 8 = row 7
    // So rank 1 -> row 0, rank 2 -> row 1, ..., rank 8 -> row 7
    
    char fromFile = move.charAt(0);
    char fromRank = move.charAt(1);
    char toFile = move.charAt(2);
    char toRank = move.charAt(3);
    
    // Validate file characters
    if (fromFile < 'a' || fromFile > 'h' || toFile < 'a' || toFile > 'h') {
        Serial.println("Invalid file in move");
        return false;
    }
    
    // Validate rank characters
    if (fromRank < '1' || fromRank > '8' || toRank < '1' || toRank > '8') {
        Serial.println("Invalid rank in move");
        return false;
    }
    
    fromCol = fromFile - 'a';
    fromRow = (fromRank - '0') - 1;  // Convert 1-8 to 0-7 (rank 1 = row 0)
    toCol = toFile - 'a';
    toRow = (toRank - '0') - 1;      // Convert 1-8 to 0-7
    
    // Debug coordinate conversion
    Serial.print("Move string: ");
    Serial.println(move);
    Serial.print("Parsed: ");
    Serial.print(fromFile);
    Serial.print(fromRank);
    Serial.print(" -> ");
    Serial.print(toFile);
    Serial.print(toRank);
    Serial.print(" | Array coords: (");
    Serial.print(fromRow);
    Serial.print(",");
    Serial.print(fromCol);
    Serial.print(") to (");
    Serial.print(toRow);
    Serial.print(",");
    Serial.print(toCol);
    Serial.println(")");
    
    // Check for promotion
    if (move.length() >= 5) {
        char promotionPiece = move.charAt(4);
        Serial.print("Promotion to: ");
        Serial.println(promotionPiece);
    }
    
    // Validate coordinates
    bool valid = (fromRow >= 0 && fromRow < 8 && fromCol >= 0 && fromCol < 8 &&
                  toRow >= 0 && toRow < 8 && toCol >= 0 && toCol < 8);
    
    if (!valid) {
        Serial.println("Invalid coordinates after parsing");
    }
    
    return valid;
}

void ChessBot::executeBotMove(int fromRow, int fromCol, int toRow, int toCol) {
    char piece = board[fromRow][fromCol];
    char capturedPiece = board[toRow][toCol];
    
    // Update board state
    board[toRow][toCol] = piece;
    board[fromRow][fromCol] = ' ';
    
    Serial.print("Bot wants to move piece from ");
    Serial.print((char)('a' + fromCol));
    Serial.print(8 - fromRow);
    Serial.print(" to ");
    Serial.print((char)('a' + toCol));
    Serial.println(8 - toRow);
    Serial.println("Please make this move on the physical board...");
    
    // Show the move that needs to be made
    showBotMoveIndicator(fromRow, fromCol, toRow, toCol);
    
    // Wait for user to physically complete the bot's move
    waitForBotMoveCompletion(fromRow, fromCol, toRow, toCol);
    
    if (capturedPiece != ' ') {
        Serial.print("Piece captured: ");
        Serial.println(capturedPiece);
        _boardDriver->captureAnimation();
    }
    
    // Flash confirmation on the destination square
    confirmSquareCompletion(toRow, toCol);
    
    Serial.println("Bot move completed. Your turn!");
}

void ChessBot::showBotThinking() {
    static unsigned long lastUpdate = 0;
    static int thinkingStep = 0;
    
    if (millis() - lastUpdate > 500) {
        // Animated thinking indicator - pulse the corners
        _boardDriver->clearAllLEDs();
        
        uint8_t brightness = (sin(thinkingStep * 0.3) + 1) * 127;
        
        _boardDriver->setSquareLED(0, 0, 0, 0, brightness); // Corner LEDs pulse blue
        _boardDriver->setSquareLED(0, 7, 0, 0, brightness);
        _boardDriver->setSquareLED(7, 0, 0, 0, brightness);
        _boardDriver->setSquareLED(7, 7, 0, 0, brightness);
        
        _boardDriver->showLEDs();
        
        thinkingStep++;
        lastUpdate = millis();
    }
}

void ChessBot::showConnectionStatus() {
    // Show WiFi connection attempt with animated LEDs
    for (int i = 0; i < 8; i++) {
        _boardDriver->setSquareLED(3, i, 0, 0, 255); // Blue row
        _boardDriver->showLEDs();
        delay(200);
    }
}

void ChessBot::initializeBoard() {
    // Copy initial board state
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = INITIAL_BOARD[row][col];
        }
    }
}

void ChessBot::waitForBoardSetup() {
    Serial.println("Please set up the chess board in starting position...");
    
    while (!_boardDriver->checkInitialBoard(INITIAL_BOARD)) {
        _boardDriver->readSensors();
        _boardDriver->updateSetupDisplay(INITIAL_BOARD);
        _boardDriver->showLEDs();
        delay(100);
    }
    
    Serial.println("Board setup complete! Game starting...");
    _boardDriver->fireworkAnimation();
    gameStarted = true;
    
    // Show initial board state
    printCurrentBoard();
}

void ChessBot::processPlayerMove(int fromRow, int fromCol, int toRow, int toCol, char piece) {
    char capturedPiece = board[toRow][toCol];
    
    // Update board state
    board[toRow][toCol] = piece;
    board[fromRow][fromCol] = ' ';
    
    Serial.print("Player moved ");
    Serial.print(piece);
    Serial.print(" from ");
    Serial.print((char)('a' + fromCol));
    Serial.print(8 - fromRow);
    Serial.print(" to ");
    Serial.print((char)('a' + toCol));
    Serial.println(8 - toRow);
    
    if (capturedPiece != ' ') {
        Serial.print("Captured ");
        Serial.println(capturedPiece);
        _boardDriver->captureAnimation();
    }
    
    // Check for pawn promotion
    if (_chessEngine->isPawnPromotion(piece, toRow)) {
        char promotedPiece = _chessEngine->getPromotedPiece(piece);
        board[toRow][toCol] = promotedPiece;
        Serial.print("Pawn promoted to ");
        Serial.println(promotedPiece);
        _boardDriver->promotionAnimation(toCol);
    }
}

String ChessBot::urlEncode(String str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encoded += "%20";
        } else if (c == '/') {
            encoded += "%2F";
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

void ChessBot::showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol) {
    // Clear all LEDs first
    _boardDriver->clearAllLEDs();
    
    // Show source square flashing (where to pick up from)
    // Use white channel (W) for much brighter display
    _boardDriver->setSquareLED(fromRow, fromCol, 0, 0, 0, 255); // Bright white using W channel
    
    // Show destination square solid (where to place)
    _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);     // Bright white using W channel
    
    _boardDriver->showLEDs();
}

void ChessBot::waitForBotMoveCompletion(int fromRow, int fromCol, int toRow, int toCol) {
    bool piecePickedUp = false;
    bool moveCompleted = false;
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    
    Serial.println("Waiting for you to complete the bot's move...");
    
    while (!moveCompleted) {
        _boardDriver->readSensors();
        
        // Blink the source square
        if (millis() - lastBlink > 500) {
            _boardDriver->clearAllLEDs();
            if (blinkState && !piecePickedUp) {
                _boardDriver->setSquareLED(fromRow, fromCol, 255, 255,  255, 255); // Flash source - bright white using W channel
            }
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255);         // Always show destination - bright white using W channel
            _boardDriver->showLEDs();
            
            blinkState = !blinkState;
            lastBlink = millis();
        }
        
        // Check if piece was picked up from source
        if (!piecePickedUp && !_boardDriver->getSensorState(fromRow, fromCol)) {
            piecePickedUp = true;
            Serial.println("Bot piece picked up, now place it on the destination...");
            
            // Stop blinking source, just show destination
            _boardDriver->clearAllLEDs();
            _boardDriver->setSquareLED(toRow, toCol, 0, 0, 0, 255); // Bright white using W channel
            _boardDriver->showLEDs();
        }
        
        // Check if piece was placed on destination
        if (piecePickedUp && _boardDriver->getSensorState(toRow, toCol)) {
            moveCompleted = true;
            Serial.println("Bot move completed on physical board!");
        }
        
        delay(50);
        _boardDriver->updateSensorPrev();
    }
}

void ChessBot::confirmMoveCompletion() {
    // This will be called with specific square coordinates when we need them
    confirmSquareCompletion(-1, -1); // Default - no specific square
}

void ChessBot::confirmSquareCompletion(int row, int col) {
    if (row >= 0 && col >= 0) {
        // Flash specific square twice
        for (int flash = 0; flash < 2; flash++) {
            _boardDriver->setSquareLED(row, col, 0, 255, 0); // Green flash
            _boardDriver->showLEDs();
            delay(150);
            
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(150);
        }
    } else {
        // Flash entire board (fallback for when we don't have specific coords)
        for (int flash = 0; flash < 2; flash++) {
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    _boardDriver->setSquareLED(r, c, 0, 255, 0); // Green flash
                }
            }
            _boardDriver->showLEDs();
            delay(150);
            
            _boardDriver->clearAllLEDs();
            _boardDriver->showLEDs();
            delay(150);
        }
    }
}

void ChessBot::printCurrentBoard() {
    Serial.println("=== CURRENT BOARD STATE ===");
    Serial.println("  a b c d e f g h");
    for (int row = 0; row < 8; row++) {
        Serial.print(8 - row);
        Serial.print(" ");
        for (int col = 0; col < 8; col++) {
            char piece = board[row][col];
            if (piece == ' ') {
                Serial.print(". ");
            } else {
                Serial.print(piece);
                Serial.print(" ");
            }
        }
        Serial.print(" ");
        Serial.println(8 - row);
    }
    Serial.println("  a b c d e f g h");
    Serial.println("White pieces (uppercase): R N B Q K P");
    Serial.println("Black pieces (lowercase): r n b q k p");
    Serial.println("========================");
}

void ChessBot::setDifficulty(BotDifficulty diff) {
    difficulty = diff;
    switch(difficulty) {
        case BOT_EASY: settings = StockfishSettings::easy(); break;
        case BOT_MEDIUM: settings = StockfishSettings::medium(); break;
        case BOT_HARD: settings = StockfishSettings::hard(); break;
        case BOT_EXPERT: settings = StockfishSettings::expert(); break;
    }
    
    Serial.print("Bot difficulty changed to: ");
    switch(difficulty) {
        case BOT_EASY: Serial.println("Easy"); break;
        case BOT_MEDIUM: Serial.println("Medium"); break;
        case BOT_HARD: Serial.println("Hard"); break;
        case BOT_EXPERT: Serial.println("Expert"); break;
    }
}

void ChessBot::getBoardState(char boardState[8][8]) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            boardState[row][col] = board[row][col];
        }
    }
}

void ChessBot::setBoardState(char newBoardState[8][8]) {
    Serial.println("Board state updated via WiFi edit");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = newBoardState[row][col];
        }
    }
    // Update sensor previous state to match new board
    _boardDriver->readSensors();
    // Note: We might need to update FEN state if bot is active
    // For now, just update the board state
}