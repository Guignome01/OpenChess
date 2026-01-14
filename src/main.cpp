#include "board_driver.h"
#include "chess_bot.h"
#include "chess_engine.h"
#include "chess_moves.h"
#include "sensor_test.h"

// Uncomment the next line to enable WiFi features (requires compatible board)
#define ENABLE_WIFI
#ifdef ENABLE_WIFI
// Use different WiFi manager based on board type
#if defined(ESP32) || defined(ESP8266)
#include "wifi_manager_esp32.h" // Full WiFi implementation for ESP32/ESP8266
#define WiFiManager WiFiManagerESP32
#endif
#endif

// ---------------------------
// Game State and Configuration
// ---------------------------

// Game Mode Definitions
enum GameMode {
  MODE_SELECTION = 0,
  MODE_CHESS_MOVES = 1,
  MODE_BOT = 2,
  MODE_SENSOR_TEST = 3
};

// Bot configuration instance
BotConfig botConfig = {StockfishSettings::medium(), true};

// Global instances
BoardDriver boardDriver;
#ifdef ENABLE_WIFI
WiFiManager wifiManager(&boardDriver);
#endif
ChessEngine chessEngine;
ChessMoves chessMoves(&boardDriver, &chessEngine);
SensorTest sensorTest(&boardDriver);
ChessBot* chessBot = nullptr;

// Current game state
GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;

// ---------------------------
// Function Prototypes
// ---------------------------
void showGameSelection();
void handleGameSelection();
void showBotConfigSelection();
void handleBotConfigSelection();
void initializeSelectedMode(GameMode mode);

// ---------------------------
// SETUP
// ---------------------------
void setup() {
  // Initialize Serial with extended timeout
  Serial.begin(115200);

  // Wait for Serial to be ready (critical for RP2040)
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 10000)) {
    // Wait up to 10 seconds for Serial connection
    delay(100);
  }

  // Force a delay to ensure Serial is stable
  delay(2000);

  Serial.println();
  Serial.println("================================================");
  Serial.println("         OpenChess Starting Up");
  Serial.println("================================================");
  Serial.println("DEBUG: Serial communication established");
  Serial.printf("DEBUG: Millis since boot: %lu\n", millis());

  // Debug board type detection
  Serial.println("DEBUG: Board type detection:");
#if defined(ESP32)
  Serial.println("  - Detected: ESP32");
#elif defined(ESP8266)
  Serial.println("  - Detected: ESP8266");
#elif defined(ARDUINO_SAMD_MKRWIFI1010)
  Serial.println("  - Detected: ARDUINO_SAMD_MKRWIFI1010");
#elif defined(ARDUINO_SAMD_NANO_33_IOT)
  Serial.println("  - Detected: ARDUINO_SAMD_NANO_33_IOT");
#elif defined(ARDUINO_NANO_RP2040_CONNECT)
  Serial.println("  - Detected: ARDUINO_NANO_RP2040_CONNECT");
#else
  Serial.println("  - Detected: Unknown/Other board type");
#endif

  // Check which mode is compiled
#ifdef ENABLE_WIFI
  Serial.println("DEBUG: Compiled with ENABLE_WIFI defined");
#else
  Serial.println("DEBUG: Compiled without ENABLE_WIFI (local mode only)");
#endif

  Serial.println("DEBUG: About to initialize board driver...");
  // Initialize board driver
  boardDriver.begin();
  Serial.println("DEBUG: Board driver initialized successfully");

#ifdef ENABLE_WIFI
  Serial.println();
  Serial.println("=== WiFi Mode Enabled ===");
  Serial.println("DEBUG: About to initialize WiFi Manager...");
  Serial.println("DEBUG: This will attempt to create Access Point");

  // Initialize WiFi Manager
  wifiManager.begin();

  Serial.println("DEBUG: WiFi Manager initialization completed");
  Serial.println("If WiFi AP was created successfully, you should see:");
  Serial.println("- Network name: OpenChessBoard");
  Serial.println("- Password: chess123");
  Serial.println("- Web interface: http://192.168.4.1");
  Serial.println("Or place a piece on the board for local selection");
#else
  Serial.println();
  Serial.println("=== Local Mode Only ===");
  Serial.println("WiFi features are disabled in this build");
  Serial.println("To enable WiFi: Uncomment #define ENABLE_WIFI and recompile");
#endif

  Serial.println();
  Serial.println("=== Game Selection Mode ===");
  Serial.println("DEBUG: About to show game selection LEDs...");

  // Show game selection interface
  showGameSelection();

  Serial.println("DEBUG: Game selection LEDs should now be visible");
  Serial.println("Three LEDs should be lit in the center of the board:");
  Serial.println("Position 1 (3,3): Chess Moves (Human vs Human)");
  Serial.println("Position 2 (3,4): Chess Bot (Human vs AI)");
  Serial.println("Position 3 (4,4): Sensor Test");
  Serial.println();
  Serial.println("Place any chess piece on a white LED to select that mode");
  Serial.println("================================================");
  Serial.println("         Setup Complete - Entering Main Loop");
  Serial.println("================================================");
}

// ---------------------------
// MAIN LOOP
// ---------------------------
void loop() {
  static unsigned long lastDebugPrint = 0;
  static bool firstLoop = true;

  if (firstLoop) {
    Serial.println("DEBUG: Entered main loop - system is running");
    firstLoop = false;
  }

  // Print periodic status every 20 seconds
  if (millis() - lastDebugPrint > 20000) {
    Serial.printf("DEBUG: Loop running, uptime: %lu seconds\n", millis() / 1000);
    lastDebugPrint = millis();
  }

#ifdef ENABLE_WIFI
  // Check for pending board edits from WiFi
  char editBoard[8][8];
  if (wifiManager.getPendingBoardEdit(editBoard)) {
    Serial.println("Applying board edit from WiFi interface...");

    if (currentMode == MODE_CHESS_MOVES && modeInitialized) {
      chessMoves.setBoardState(editBoard);
      Serial.println("Board edit applied to Chess Moves mode");
    } else if (currentMode == MODE_BOT && modeInitialized && chessBot != nullptr) {
      chessBot->setBoardState(editBoard);
      Serial.println("Board edit applied to Chess Bot mode");
    } else {
      Serial.println("Warning: Board edit received but no active game mode");
    }

    wifiManager.clearPendingEdit();
  }

  // Update board state for WiFi display
  static unsigned long lastBoardUpdate = 0;
  if (millis() - lastBoardUpdate > 100) { // Update every 100ms (that's really stupid, should immediately upate when there is any change instead of polling)
    char currentBoard[8][8];
    bool boardUpdated = false;

    float evaluation = 0.0;
    if (currentMode == MODE_CHESS_MOVES && modeInitialized) {
      chessMoves.getBoardState(currentBoard);
      boardUpdated = true;
    } else if (currentMode == MODE_BOT && modeInitialized && chessBot != nullptr) {
      chessBot->getBoardState(currentBoard);
      evaluation = chessBot->getEvaluation();
      boardUpdated = true;
    }

    if (boardUpdated) {
      wifiManager.updateBoardState(currentBoard, evaluation);
    }

    lastBoardUpdate = millis();
  }

  // Check for WiFi game selection
  int selectedMode = wifiManager.getSelectedGameMode();
  if (selectedMode > 0) {
    Serial.printf("DEBUG: WiFi game selection detected: %d\n", selectedMode);

    switch (selectedMode) {
      case 1:
        currentMode = MODE_CHESS_MOVES;
        break;
      case 2:
        currentMode = MODE_BOT;
        // Bot config (difficulty and color) should be sent separately
        botConfig = wifiManager.getBotConfig();
        break;
      case 3:
        currentMode = MODE_SENSOR_TEST;
        break;
      default:
        Serial.println("Invalid game mode selected via WiFi");
        selectedMode = 0;
        break;
    }

    if (selectedMode > 0) {
      modeInitialized = false;
      boardDriver.clearAllLEDs();
      wifiManager.resetGameSelection();

      // Brief confirmation animation
      for (int i = 0; i < 3; i++) {
        boardDriver.setSquareLED(3, 3, 0, 255, 0, 0);
        boardDriver.setSquareLED(3, 4, 0, 255, 0, 0);
        boardDriver.setSquareLED(4, 3, 0, 255, 0, 0);
        boardDriver.setSquareLED(4, 4, 0, 255, 0, 0);
        boardDriver.showLEDs();
        delay(200);
        boardDriver.clearAllLEDs();
        delay(200);
      }
    }
  }
#endif

  if (currentMode == MODE_SELECTION) {
    handleGameSelection();
  } else {
    static bool modeChangeLogged = false;
    if (!modeChangeLogged) {
      Serial.printf("DEBUG: Mode changed to: %d\n", currentMode);
      modeChangeLogged = true;
    }
    if (!modeInitialized) {
      initializeSelectedMode(currentMode);
      modeInitialized = true;
    }

    // Run the current game mode
    switch (currentMode) {
      case MODE_CHESS_MOVES:
        chessMoves.update();
        break;
      case MODE_BOT:
        if (chessBot != nullptr) {
          chessBot->update();
        }
        break;
      case MODE_SENSOR_TEST:
        sensorTest.update();
        break;
      default:
        currentMode = MODE_SELECTION;
        modeInitialized = false;
        showGameSelection();
        break;
    }
  }

  delay(25); // Small delay to prevent overwhelming the system
}

// ---------------------------
// GAME SELECTION FUNCTIONS
// ---------------------------

void showGameSelection() {
  boardDriver.clearAllLEDs();
  // Light up the 3 selector positions in the middle of the board
  // Each mode has a different color for easy identification
  // Position 1: Chess Moves (row 3, col 3) - Orange
  boardDriver.setSquareLED(3, 3, 255, 165, 0);
  // Position 2: Chess Bot (row 3, col 4) - White
  boardDriver.setSquareLED(3, 4, 0, 0, 0, 255);
  // Position 3: Sensor Test (row 4, col 4) - Red
  boardDriver.setSquareLED(4, 4, 255, 0, 0);
  boardDriver.showLEDs();
}

void handleGameSelection() {
  boardDriver.readSensors();

  // Check for piece placement on selector squares
  if (boardDriver.getSensorState(3, 3)) {
    Serial.println("Mode: 'Chess Moves' selected!");
    currentMode = MODE_CHESS_MOVES;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  } else if (boardDriver.getSensorState(3, 4)) {
    Serial.println("Mode: 'Chess Bot' Selected! Showing bot configuration...");
    currentMode = MODE_BOT;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
    // Show bot configuration selection instead of starting immediately
    showBotConfigSelection();
  } else if (boardDriver.getSensorState(4, 4)) {
    Serial.println("Mode: 'Sensor Test' Selected!");
    currentMode = MODE_SENSOR_TEST;
    modeInitialized = false;
    boardDriver.clearAllLEDs();
    delay(500);
  }

  delay(100);
}

void initializeSelectedMode(GameMode mode) {
  switch (mode) {
    case MODE_CHESS_MOVES:
      Serial.println("Starting 'Chess Moves'...");
      chessMoves.begin();
      break;
    case MODE_BOT:
      Serial.printf("Starting 'Chess Bot' (Depth: %d, Player is %s)...\n", botConfig.stockfishSettings.depth, botConfig.playerIsWhite ? "White" : "Black");
      // Clean up any existing bot instance
      if (chessBot != nullptr) {
        delete chessBot;
      }
      // Create new bot with current configuration
      chessBot = new ChessBot(&boardDriver, &chessEngine, &wifiManager, botConfig);
      chessBot->begin();
      break;
    case MODE_SENSOR_TEST:
      Serial.println("Starting 'Sensor Test'...");
      sensorTest.begin();
      break;

    default:
      currentMode = MODE_SELECTION;
      modeInitialized = false;
      showGameSelection();
      break;
  }
}

// Bot configuration selection functions
void showBotConfigSelection() {
  boardDriver.clearAllLEDs();

  Serial.println("=== Bot Configuration Selection ===");
  Serial.println("Select Bot Color:");
  Serial.println("Row 2 (any square): Play as White (bot is Black)");
  Serial.println("Row 5 (any square): Play as Black (bot is White)");
  Serial.println("");
  Serial.println("Select Difficulty:");
  Serial.println("Col 1: Easy");
  Serial.println("Col 3: Medium");
  Serial.println("Col 5: Hard");
  Serial.println("Col 7: Expert");
  Serial.println("");
  Serial.println("Example: Place piece at row 2, col 3 = White + Medium");

  // Only show LEDs for valid difficulty selections (don't light up all squares)
  // Row 2 = Player plays White, Row 5 = Player plays Black
  // Easy (col 1) - Green
  boardDriver.setSquareLED(2, 1, 0, 255, 0);
  boardDriver.setSquareLED(5, 1, 0, 255, 0);

  // Medium (col 3) - Orange/Gold
  boardDriver.setSquareLED(2, 3, 255, 165, 0);
  boardDriver.setSquareLED(5, 3, 255, 165, 0);

  // Hard (col 5) - Red
  boardDriver.setSquareLED(2, 5, 255, 0, 0);
  boardDriver.setSquareLED(5, 5, 255, 0, 0);

  // Expert (col 7) - Purple
  boardDriver.setSquareLED(2, 7, 128, 0, 255);
  boardDriver.setSquareLED(5, 7, 128, 0, 255);

  boardDriver.showLEDs();

  // Wait for selection
  handleBotConfigSelection();
}

void handleBotConfigSelection() {
  Serial.println("Waiting for bot configuration selection...");

  while (currentMode == MODE_BOT && !modeInitialized) {
    boardDriver.readSensors();

    // Check rows 2 and 5 for selections
    for (int row : {2, 5}) {
      for (int col = 0; col < 8; col++) {
        if (boardDriver.getSensorState(row, col)) {
          // Determine player color based on row
          botConfig.playerIsWhite = (row == 2);
          const char* colorName = botConfig.playerIsWhite ? "White" : "Black";

          // Determine difficulty based on column
          if (col == 1) {
            botConfig.stockfishSettings = StockfishSettings::easy();
            Serial.printf("Configuration: Play as %s, Easy difficulty\n", colorName);
          } else if (col == 3) {
            botConfig.stockfishSettings = StockfishSettings::medium();
            Serial.printf("Configuration: Play as %s, Medium difficulty\n", colorName);
          } else if (col == 5) {
            botConfig.stockfishSettings = StockfishSettings::hard();
            Serial.printf("Configuration: Play as %s, Hard difficulty\n", colorName);
          } else if (col == 7) {
            botConfig.stockfishSettings = StockfishSettings::expert();
            Serial.printf("Configuration: Play as %s, Expert difficulty\n", colorName);
          } else {
            Serial.println("Invalid column for difficulty selection. Please try again.");
            continue;
          }

          // Configuration complete
          boardDriver.clearAllLEDs();
          delay(500);
          return;
        }
      }
    }

    delay(100);
  }
}
