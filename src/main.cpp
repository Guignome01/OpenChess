#include "board_driver.h"
#include "chess_bot.h"
#include "chess_engine.h"
#include "chess_lichess.h"
#include "chess_moves.h"
#include "chess_utils.h"
#include "led_colors.h"
#include "menu_config.h"
#include "move_history.h"
#include "sensor_test.h"
#include "wifi_manager_esp32.h"
#include <LittleFS.h>
#include <time.h>

// ---------------------------
// Game State and Configuration
// ---------------------------

enum GameMode {
  MODE_SELECTION = 0,
  MODE_CHESS_MOVES = 1,
  MODE_BOT = 2,
  MODE_LICHESS = 3,
  MODE_SENSOR_TEST = 4
};

BotConfig botConfig = {StockfishSettings::medium(), true};
LichessConfig lichessConfig = {""};

BoardDriver boardDriver;
ChessEngine chessEngine;
MoveHistory moveHistory;
WiFiManagerESP32 wifiManager(&boardDriver, &moveHistory);
ChessGame* activeGame = nullptr;
SensorTest* sensorTest = nullptr;

GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;
bool resumingGame = false;

void enterGameSelection();
void handleMenuResult(int result);
void initializeSelectedMode(GameMode mode);
void checkForResumableGame();

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println();
  Serial.println("================================================");
  Serial.println("         OpenChess Starting Up");
  Serial.println("================================================");
  if (!ChessUtils::ensureNvsInitialized())
    Serial.println("WARNING: NVS init failed (Preferences may not work)");
  if (!LittleFS.begin(true))
    Serial.println("ERROR: LittleFS mount failed!");
  else
    Serial.println("LittleFS mounted successfully");
  moveHistory.begin();
  boardDriver.begin();
  wifiManager.begin();
  Serial.println();

  // Configure menu system
  initMenus(&boardDriver);

  // Kick off NTP time sync (non-blocking, will resolve in background)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // Check for a live game that can be resumed
  checkForResumableGame();
  if (currentMode != MODE_SELECTION)
    return; // Resuming a game — skip showing game selection

  enterGameSelection();
}

void checkForResumableGame() {
  uint8_t resumeMode = 0, resumePlayerColor = 0, resumeBotDepth = 0;
  if (!moveHistory.hasLiveGame() || !moveHistory.getLiveGameInfo(resumeMode, resumePlayerColor, resumeBotDepth))
    return;

  Serial.println("========== Live game found on flash ==========");

  LedRGB indicatorColor = LedColors::White;
  const char* modeName = "Unknown";
  bool flipped = false;

  switch (resumeMode) {
    case GAME_MODE_CHESS_MOVES:
      indicatorColor = LedColors::Blue;
      modeName = "Chess Moves";
      break;
    case GAME_MODE_BOT:
      indicatorColor = LedColors::Green;
      modeName = "Bot";
      flipped = (resumePlayerColor == 'b');
      Serial.printf("  Mode: Bot (player=%c, depth=%d)\n", (char)resumePlayerColor, resumeBotDepth);
      break;
    default:
      Serial.println("Unknown live game mode, discarding");
      moveHistory.discardLiveGame();
      Serial.println("================================================");
      return;
  }

  Serial.printf("  Found: %s game — confirm resume?\n", modeName);
  Serial.println("  Green = Resume, Red = Discard");
  boardDriver.blinkSquare(3, 3, indicatorColor, 2);
  boardDriver.waitForAnimationQueueDrain();

  if (boardConfirm(&boardDriver, flipped)) {
    Serial.println("  -> Player chose to RESUME");
    switch (resumeMode) {
      case GAME_MODE_CHESS_MOVES:
        currentMode = MODE_CHESS_MOVES;
        resumingGame = true;
        break;
      case GAME_MODE_BOT:
        currentMode = MODE_BOT;
        resumingGame = true;
        botConfig.playerIsWhite = (resumePlayerColor == 'w');
        botConfig.stockfishSettings = StockfishSettings(resumeBotDepth);
        break;
    }
  } else {
    Serial.println("  -> Player chose to DISCARD");
    moveHistory.discardLiveGame();
  }

  Serial.println("================================================");
}

void loop() {
  // Check for pending board edits from WiFi (FEN-based)
  String editFen;
  if (wifiManager.getPendingBoardEdit(editFen)) {
    Serial.println("Applying board edit from WiFi interface...");

    if (activeGame != nullptr && modeInitialized) {
      activeGame->setBoardStateFromFEN(editFen);
      Serial.println("Board edit applied");
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
        currentMode = MODE_LICHESS;
        lichessConfig = wifiManager.getLichessConfig();
        break;
      case 4:
        currentMode = MODE_SENSOR_TEST;
        break;
      default:
        Serial.println("Invalid game mode selected via WiFi");
        selectedMode = 0;
        break;
    }
    if (selectedMode > 0) {
      modeInitialized = false;
      navigator.clear();
      wifiManager.resetGameSelection();
      boardDriver.clearAllLEDs();
    }
  }

  if (currentMode == MODE_SELECTION) {
    boardDriver.readSensors();
    int result = navigator.poll();
    if (result != BoardMenu::RESULT_NONE)
      handleMenuResult(result);
    delay(SENSOR_READ_DELAY_MS);
    return;
  }
  // Game mode selected
  if (!modeInitialized) {
    initializeSelectedMode(currentMode);
    modeInitialized = true;
    delay(1); // HACK: Ensure any starting animations acquire the LED mutex before proceeding
  }

  switch (currentMode) {
    case MODE_CHESS_MOVES:
    case MODE_BOT:
    case MODE_LICHESS:
      if (activeGame != nullptr) {
        if (activeGame->isGameOver())
          enterGameSelection();
        else
          activeGame->update();
      }
      break;
    case MODE_SENSOR_TEST:
      if (sensorTest != nullptr) {
        if (sensorTest->isComplete())
          enterGameSelection();
        else
          sensorTest->update();
      }
      break;
    default:
      enterGameSelection();
      break;
  }

  delay(SENSOR_READ_DELAY_MS);
}

void enterGameSelection() {
  currentMode = MODE_SELECTION;
  modeInitialized = false;
  navigator.clear();
  navigator.push(&gameMenu);
  Serial.println("=============== Game Selection Mode ===============");
  Serial.println("Four LEDs are lit in the center of the board:");
  Serial.println("  Blue:   Chess Moves (Human vs Human)");
  Serial.println("  Green:  Chess Bot (Human vs AI)");
  Serial.println("  Yellow: Lichess (Play online games)");
  Serial.println("  Red:    Sensor Test");
  Serial.println("Place any chess piece on a LED to select that mode");
  Serial.println("===================================================");
}

void handleMenuResult(int result) {
  switch (result) {
    // Game selection menu
    case MenuId::CHESS_MOVES:
      Serial.println("Mode: 'Chess Moves' selected!");
      currentMode = MODE_CHESS_MOVES;
      modeInitialized = false;
      navigator.clear();
      break;
    case MenuId::BOT:
      Serial.println("Mode: 'Chess Bot' selected! Choose difficulty...");
      navigator.push(&botDifficultyMenu);
      break;
    case MenuId::LICHESS:
      Serial.println("Mode: 'Lichess' selected!");
      currentMode = MODE_LICHESS;
      modeInitialized = false;
      lichessConfig = wifiManager.getLichessConfig();
      navigator.clear();
      break;
    case MenuId::SENSOR_TEST:
      Serial.println("Mode: 'Sensor Test' selected!");
      currentMode = MODE_SENSOR_TEST;
      modeInitialized = false;
      navigator.clear();
      break;

    // Bot difficulty menu (ids 10–17 → level 1–8)
    case MenuId::DIFF_1: case MenuId::DIFF_2: case MenuId::DIFF_3: case MenuId::DIFF_4:
    case MenuId::DIFF_5: case MenuId::DIFF_6: case MenuId::DIFF_7: case MenuId::DIFF_8: {
      int level = result - MenuId::DIFF_1 + 1;
      botConfig.stockfishSettings = StockfishSettings::fromLevel(level);
      Serial.printf("Difficulty: Level %d (depth %d)\n", level, botConfig.stockfishSettings.depth);
      navigator.push(&botColorMenu);
      break;
    }

    // Bot color menu
    case MenuId::PLAY_WHITE:
      Serial.println("Playing as White");
      botConfig.playerIsWhite = true;
      currentMode = MODE_BOT;
      modeInitialized = false;
      navigator.clear();
      break;
    case MenuId::PLAY_BLACK:
      Serial.println("Playing as Black");
      botConfig.playerIsWhite = false;
      currentMode = MODE_BOT;
      modeInitialized = false;
      navigator.clear();
      break;
    case MenuId::PLAY_RANDOM:
      Serial.println("Playing as Random");
      botConfig.playerIsWhite = (random(2) == 0);
      Serial.printf("  -> Assigned: %s\n", botConfig.playerIsWhite ? "White" : "Black");
      currentMode = MODE_BOT;
      modeInitialized = false;
      navigator.clear();
      break;

    default:
      Serial.printf("Unknown menu result: %d\n", result);
      break;
  }
}

void initializeSelectedMode(GameMode mode) {
  if (resumingGame)
    resumingGame = false;
  else
    moveHistory.discardLiveGame(); // Discard any incomplete live game that wasn't properly finished or resumed (finishGame already removes live files for completed games)

  // Clean up previous game/test
  delete activeGame;
  activeGame = nullptr;
  delete sensorTest;
  sensorTest = nullptr;

  switch (mode) {
    case MODE_CHESS_MOVES:
      Serial.println("Starting 'Chess Moves'...");
      activeGame = new ChessMoves(&boardDriver, &chessEngine, &wifiManager, &moveHistory);
      activeGame->begin();
      break;
    case MODE_BOT:
      Serial.printf("Starting 'Chess Bot' (Depth: %d, Player is %s)...\n", botConfig.stockfishSettings.depth, botConfig.playerIsWhite ? "White" : "Black");
      activeGame = new ChessBot(&boardDriver, &chessEngine, &wifiManager, &moveHistory, botConfig);
      activeGame->begin();
      break;
    case MODE_LICHESS:
      Serial.println("Starting 'Lichess Mode'...");
      activeGame = new ChessLichess(&boardDriver, &chessEngine, &wifiManager, lichessConfig);
      activeGame->begin();
      break;
    case MODE_SENSOR_TEST:
      Serial.println("Starting 'Sensor Test'...");
      sensorTest = new SensorTest(&boardDriver);
      sensorTest->begin();
      break;
    default:
      enterGameSelection();
      break;
  }
}