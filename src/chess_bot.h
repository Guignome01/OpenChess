#ifndef CHESS_BOT_H
#define CHESS_BOT_H

#include "board_driver.h"
#include "chess_engine.h"
#include "stockfish_settings.h"
#include "arduino_secrets.h"

// Platform-specific WiFi includes
#if defined(ESP32) || defined(ESP8266)
  // ESP32/ESP8266 use built-in WiFi libraries
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define WiFiSSLClient WiFiClientSecure
#elif defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_NANO_RP2040_CONNECT)
  // Boards with WiFiNINA module
  #include <WiFiNINA.h>
  #include <WiFiSSLClient.h>
#else
  // Other boards - WiFi not supported
  #warning "WiFi not supported on this board - Chess Bot will not work"
#endif

class ChessBot {
private:
    BoardDriver* _boardDriver;
    ChessEngine* _chessEngine;
    
    char board[8][8];
    const char INITIAL_BOARD[8][8] = {
        {'R','N','B','K','Q','B','N','R'},  // row 0 (rank 8 - black pieces at top)
        {'P','P','P','P','P','P','P','P'},  // row 1 (rank 7)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 2 (rank 6)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 3 (rank 5)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 4 (rank 4)
        {' ',' ',' ',' ',' ',' ',' ',' '},  // row 5 (rank 3)
        {'p','p','p','p','p','p','p','p'},  // row 6 (rank 2)
        {'r','n','b','k','q','b','n','r'}   // row 7 (rank 1 - white pieces at bottom)
    };
    
    StockfishSettings settings;
    BotDifficulty difficulty;
    
    bool isWhiteTurn;
    bool playerIsWhite;  // true = player plays White, false = player plays Black
    bool gameStarted;
    bool botThinking;
    bool wifiConnected;
    float currentEvaluation;  // Stockfish evaluation (in centipawns, positive = white advantage)
    
    // FEN notation handling
    String boardToFEN();
    void fenToBoard(String fen);
    
    // WiFi and API
    bool connectToWiFi();
    String makeStockfishRequest(String fen);
    bool parseStockfishResponse(String response, String &bestMove, float &evaluation);
    
    // Move handling
    bool parseMove(String move, int &fromRow, int &fromCol, int &toRow, int &toCol);
    void executeBotMove(int fromRow, int fromCol, int toRow, int toCol);
    
    // URL encoding helper
    String urlEncode(String str);
    
    // Game flow
    void initializeBoard();
    void waitForBoardSetup();
    void processPlayerMove(int fromRow, int fromCol, int toRow, int toCol, char piece);
    void makeBotMove();
    void showBotThinking();
    void showConnectionStatus();
    void showBotMoveIndicator(int fromRow, int fromCol, int toRow, int toCol);
    void waitForBotMoveCompletion(int fromRow, int fromCol, int toRow, int toCol);
    void confirmMoveCompletion();
    void confirmSquareCompletion(int row, int col);
    void printCurrentBoard();
    
public:
    ChessBot(BoardDriver* boardDriver, ChessEngine* chessEngine, BotDifficulty diff = BOT_MEDIUM, bool playerWhite = true);
    void begin();
    void update();
    void setDifficulty(BotDifficulty diff);
    
    // Get current board state for WiFi display
    void getBoardState(char boardState[8][8]);
    
    // Set board state for editing/corrections
    void setBoardState(char newBoardState[8][8]);
    
    // Get current evaluation
    float getEvaluation() { return currentEvaluation; }
};

#endif // CHESS_BOT_H
