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
ChessMoves* chessMoves = nullptr;
ChessBot* chessBot = nullptr;
ChessLichess* chessLichess = nullptr;
SensorTest* sensorTest = nullptr;

GameMode currentMode = MODE_SELECTION;
bool modeInitialized = false;
bool resumingGame = false;

void enterGameSelection();
void handleMenuResult(int result);
void initializeSelectedMode(GameMode mode);

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
  uint8_t resumeMode = 0, resumePlayerColor = 0, resumeBotDepth = 0;
  if (moveHistory.hasLiveGame() && moveHistory.getLiveGameInfo(resumeMode, resumePlayerColor, resumeBotDepth)) {
    Serial.println("========== Live game found on flash ==========");
    switch (resumeMode) {
      case GAME_MODE_CHESS_MOVES:
        Serial.println("Resuming Chess Moves game...");
        currentMode = MODE_CHESS_MOVES;
        resumingGame = true;
        break;
      case GAME_MODE_BOT:
        Serial.printf("Resuming Bot game (player=%c, depth=%d)...\n", (char)resumePlayerColor, resumeBotDepth);
        currentMode = MODE_BOT;
        resumingGame = true;
        botConfig.playerIsWhite = (resumePlayerColor == 'w');
        botConfig.stockfishSettings = StockfishSettings(resumeBotDepth);
        break;
      default:
        Serial.println("Unknown live game mode, discarding");
        moveHistory.discardLiveGame();
        break;
    }
    Serial.println("================================================");
    if (currentMode != MODE_SELECTION)
      return; // Skip showing game selection
  }

  enterGameSelection();
}

void loop() {
  // Check for pending board edits from WiFi (FEN-based)
  String editFen;
  if (wifiManager.getPendingBoardEdit(editFen)) {
    Serial.println("Applying board edit from WiFi interface...");

    if (currentMode == MODE_CHESS_MOVES && modeInitialized && chessMoves != nullptr) {
      chessMoves->setBoardStateFromFEN(editFen);
      Serial.println("Board edit applied to Chess Moves mode");
    } else if (currentMode == MODE_BOT && modeInitialized && chessBot != nullptr) {
      chessBot->setBoardStateFromFEN(editFen);
      Serial.println("Board edit applied to Chess Bot mode");
    } else if (currentMode == MODE_LICHESS && modeInitialized && chessLichess != nullptr) {
      chessLichess->setBoardStateFromFEN(editFen);
      Serial.println("Board edit applied to Lichess mode");
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
      if (chessMoves != nullptr) {
        if (chessMoves->isGameOver())
          enterGameSelection();
        else
          chessMoves->update();
      }
      break;
    case MODE_BOT:
      if (chessBot != nullptr) {
        if (chessBot->isGameOver())
          enterGameSelection();
        else
          chessBot->update();
      }
      break;
    case MODE_LICHESS:
      if (chessLichess != nullptr) {
        if (chessLichess->isGameOver())
          enterGameSelection();
        else
          chessLichess->update();
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

    // Bot difficulty menu
    case MenuId::EASY:
      Serial.println("Difficulty: Easy");
      botConfig.stockfishSettings = StockfishSettings::easy();
      navigator.push(&botColorMenu);
      break;
    case MenuId::MEDIUM:
      Serial.println("Difficulty: Medium");
      botConfig.stockfishSettings = StockfishSettings::medium();
      navigator.push(&botColorMenu);
      break;
    case MenuId::HARD:
      Serial.println("Difficulty: Hard");
      botConfig.stockfishSettings = StockfishSettings::hard();
      navigator.push(&botColorMenu);
      break;
    case MenuId::EXPERT:
      Serial.println("Difficulty: Expert");
      botConfig.stockfishSettings = StockfishSettings::expert();
      navigator.push(&botColorMenu);
      break;

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
  switch (mode) {
    case MODE_CHESS_MOVES:
      Serial.println("Starting 'Chess Moves'...");
      if (chessMoves != nullptr)
        delete chessMoves;
      chessMoves = new ChessMoves(&boardDriver, &chessEngine, &wifiManager, &moveHistory);
      chessMoves->begin();
      break;
    case MODE_BOT:
      Serial.printf("Starting 'Chess Bot' (Depth: %d, Player is %s)...\n", botConfig.stockfishSettings.depth, botConfig.playerIsWhite ? "White" : "Black");
      if (chessBot != nullptr)
        delete chessBot;
      chessBot = new ChessBot(&boardDriver, &chessEngine, &wifiManager, &moveHistory, botConfig);
      chessBot->begin();
      break;
    case MODE_LICHESS:
      Serial.println("Starting 'Lichess Mode'...");
      if (chessLichess != nullptr)
        delete chessLichess;
      chessLichess = new ChessLichess(&boardDriver, &chessEngine, &wifiManager, lichessConfig);
      chessLichess->begin();
      break;
    case MODE_SENSOR_TEST:
      Serial.println("Starting 'Sensor Test'...");
      if (sensorTest != nullptr)
        delete sensorTest;
      sensorTest = new SensorTest(&boardDriver);
      sensorTest->begin();
      break;
    default:
      enterGameSelection();
      break;
  }
}