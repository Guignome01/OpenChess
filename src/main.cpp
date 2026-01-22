#include "board_driver.h"
#include "chess_bot.h"
#include "chess_engine.h"
#include "chess_moves.h"
#include "chess_utils.h"
#include "sensor_test.h"
#include "wifi_manager_esp32.h"

#define WiFiManager WiFiManagerESP32

// ---------------------------
// Game State and Configuration
// ---------------------------

enum GameMode {
  MODE_SELECTION = 0,
  MODE_CHESS_MOVES = 1,
  MODE_BOT = 2,
  MODE_SENSOR_TEST = 3
};

BotConfig botConfig = {StockfishSettings::medium(), true};

BoardDriver boardDriver;
ChessEngine chessEngine;
WiFiManager wifiManager(&boardDriver);
ChessMoves chessMoves(&boardDriver, &chessEngine, &wifiManager);
ChessBot* chessBot = nullptr;
SensorTest sensorTest(&boardDriver);

GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;

// ---------------------------
// Function Prototypes
// ---------------------------
void showGameSelection();
void handleGameSelection();
void handleBotConfigSelection();
void initializeSelectedMode(GameMode mode);

// ---------------------------
// SETUP
// ---------------------------
void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("================================================");
  Serial.println("         OpenChess Starting Up");
  Serial.println("================================================");
  if (!ChessUtils::ensureNvsInitialized())
    Serial.println("WARNING: NVS init failed (Preferences may not work)");
  boardDriver.begin();
  wifiManager.begin();
  Serial.println();
  Serial.println("=== Game Selection Mode ===");
  showGameSelection();
  Serial.println("Three LEDs are lit in the center of the board:");
  Serial.println("Gold:  Chess Moves (Human vs Human)");
  Serial.println("White: Chess Bot (Human vs AI)");
  Serial.println("Red:   Sensor Test");
  Serial.println();
  Serial.println("Place any chess piece on a LED to select that mode");
  Serial.println("================================================");
}

void loop() {
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

  // Check for WiFi game selection
  int selectedMode = wifiManager.getSelectedGameMode();
  if (selectedMode > 0) {
    Serial.printf("WiFi game selection detected: %d\n", selectedMode);
    switch (selectedMode) {
      case 1:
        currentMode = MODE_CHESS_MOVES;
        break;
      case 2:
        currentMode = MODE_BOT;
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
      wifiManager.resetGameSelection();
      boardDriver.clearAllLEDs();
    }
  }

  if (currentMode == MODE_SELECTION) {
    handleGameSelection();
    return;
  }
  // Game mode selected
  if (!modeInitialized) {
    initializeSelectedMode(currentMode);
    modeInitialized = true;
  }

  bool gameEnded = false;
  switch (currentMode) {
    case MODE_CHESS_MOVES:
      if (chessMoves.isGameOver())
        showGameSelection();
      else
        chessMoves.update();
      break;
    case MODE_BOT:
      if (chessBot != nullptr) {
        if (chessBot->isGameOver())
          showGameSelection();
        else
          chessBot->update();
      }
      break;
    case MODE_SENSOR_TEST:
      sensorTest.update();
      break;
    default:
      showGameSelection();
      break;
  }

  delay(SENSOR_READ_DELAY_MS);
}

// ---------------------------
// GAME SELECTION FUNCTIONS
// ---------------------------

void showGameSelection() {
  currentMode = MODE_SELECTION;
  modeInitialized = false;
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
  bool currState[3] = {boardDriver.getSensorState(3, 3), boardDriver.getSensorState(3, 4), boardDriver.getSensorState(4, 4)};

  struct SelectorState {
    int emptyCount;
    int occupiedCount;
    bool readyForSelection;
  };
  const int DEBOUNCE_CYCLES = (DEBOUNCE_MS / SENSOR_READ_DELAY_MS) + 2;
  static SelectorState selectorStates[3] = {};
  static bool initializedStates = false;
  if (!initializedStates) {
    for (int i = 0; i < 3; ++i) {
      selectorStates[i].emptyCount = 0;
      selectorStates[i].occupiedCount = 0;
      selectorStates[i].readyForSelection = false;
    }
    initializedStates = true;
  }
  for (int i = 0; i < 3; ++i) {
    if (!currState[i]) {
      if (selectorStates[i].emptyCount < DEBOUNCE_CYCLES)
        selectorStates[i].emptyCount++;
      selectorStates[i].occupiedCount = 0;
      if (selectorStates[i].emptyCount >= DEBOUNCE_CYCLES)
        selectorStates[i].readyForSelection = true;
    } else {
      selectorStates[i].emptyCount = 0;
      if (selectorStates[i].readyForSelection) {
        if (selectorStates[i].occupiedCount < DEBOUNCE_CYCLES)
          selectorStates[i].occupiedCount++;
      } else {
        selectorStates[i].occupiedCount = 0;
      }
    }
  }

  // Check for valid rising edge (empty for DEBOUNCE_CYCLES, then occupied for DEBOUNCE_CYCLES)
  for (int i = 0; i < 3; ++i) {
    if (selectorStates[i].readyForSelection && selectorStates[i].occupiedCount >= DEBOUNCE_CYCLES) {
      switch (i) {
        case 0:
          Serial.println("Mode: 'Chess Moves' selected!");
          currentMode = MODE_CHESS_MOVES;
          modeInitialized = false;
          boardDriver.clearAllLEDs();
          break;
        case 1:
          Serial.println("Mode: 'Chess Bot' Selected! Showing bot configuration...");
          currentMode = MODE_BOT;
          modeInitialized = false;
          boardDriver.clearAllLEDs();
          handleBotConfigSelection();
          break;
        case 2:
          Serial.println("Mode: 'Sensor Test' Selected!");
          currentMode = MODE_SENSOR_TEST;
          modeInitialized = false;
          boardDriver.clearAllLEDs();
          break;
      }
      initializedStates = false;
      break;
    }
  }

  delay(SENSOR_READ_DELAY_MS);
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
      if (chessBot != nullptr)
        delete chessBot;
      // Create new bot with current configuration
      chessBot = new ChessBot(&boardDriver, &chessEngine, &wifiManager, botConfig);
      chessBot->begin();
      break;
    case MODE_SENSOR_TEST:
      Serial.println("Starting 'Sensor Test'...");
      sensorTest.begin();
      break;
    default:
      showGameSelection();
      break;
  }
}

void handleBotConfigSelection() {
  boardDriver.clearAllLEDs();

  Serial.println("====== Bot Configuration Selection ======");
  Serial.println("Select Bot Color:");
  Serial.println("- Rank 6: Bot is Black");
  Serial.println("- Rank 3: Bot is White");
  Serial.println("Select Difficulty:");
  Serial.println("- File B: Easy");
  Serial.println("- File D: Medium");
  Serial.println("- File F: Hard");
  Serial.println("- File H: Expert");
  Serial.println("Example: Place piece at Rank 3, File D = White Bot Medium");

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
  Serial.println("Waiting for bot configuration selection...");

  static bool prevState[2][8] = {};
  bool firstLoop = true;

  while (true) {
    boardDriver.readSensors();

    for (int rowIdx = 0; rowIdx < 2; ++rowIdx) {
      int row = (rowIdx == 0) ? 2 : 5;
      for (int col : {1, 3, 5, 7}) {
        bool curr = boardDriver.getSensorState(row, col);
        // Only accept selection if square was previously empty and is now occupied
        if (!firstLoop && !prevState[rowIdx][col] && curr) {
          botConfig.playerIsWhite = (row == 2);
          const char* colorName = botConfig.playerIsWhite ? "White" : "Black";
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
          }
          firstLoop = true;
          boardDriver.clearAllLEDs();
          return;
        }
        prevState[rowIdx][col] = curr;
      }
    }

    firstLoop = false;
    delay(SENSOR_READ_DELAY_MS);
  }
}