#include "chess_lichess.h"
#include "chess_utils.h"
#include "led_colors.h"
#include "wifi_manager_esp32.h"
#include <Arduino.h>

// Dummy BotConfig for parent constructor (not used in Lichess mode)
static BotConfig dummyBotConfig = {StockfishSettings::medium(), false};

ChessLichess::ChessLichess(BoardDriver* bd, ChessEngine* ce, WiFiManagerESP32* wm, LichessConfig cfg)
    : ChessBot(bd, ce, wm, nullptr, dummyBotConfig),
      lichessConfig(cfg),
      currentGameId(""),
      myColor('w'),
      lastKnownMoves(""),
      lastSentMove(""),
      lastPollTime(0),
      stopAnimation(nullptr) {}

void ChessLichess::begin() {
  Serial.println("=== Starting Lichess Mode ===");

  if (!wifiManager->connectToWiFi(wifiManager->getWiFiSSID(), wifiManager->getWiFiPassword())) {
    Serial.println("Failed to connect to WiFi. Lichess mode unavailable.");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gameOver = true;
    return;
  }

  if (lichessConfig.apiToken.length() == 0) {
    Serial.println("No Lichess API token configured!");
    Serial.println("Please set your Lichess API token via the web interface.");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gameOver = true;
    return;
  }

  String username;
  LichessAPI::setToken(lichessConfig.apiToken);
  if (!LichessAPI::verifyToken(username)) {
    Serial.println("Invalid Lichess API token!");
    boardDriver->flashBoardAnimation(LedColors::Red);
    gameOver = true;
    return;
  }

  Serial.println("Logged in as: " + username);
  Serial.println("Waiting for a Lichess game to start...");
  Serial.println("Start a game on lichess.org or accept a challenge!");
  Serial.println("====================================");

  waitForLichessGame();
}

void ChessLichess::waitForLichessGame() {
  Serial.println("Searching for active Lichess games...");
  std::atomic<bool>* stopAnimation = boardDriver->startWaitingAnimation();
  LichessEvent event;
  event.type = LichessEventType::UNKNOWN;
  while (!gameOver) {
    if (!LichessAPI::pollForGameEvent(event) || event.type != LichessEventType::GAME_START) {
      delay(2000);
      continue;
    }
    break;
  }
  if (stopAnimation) stopAnimation->store(true);
  currentGameId = event.gameId;
  myColor = event.myColor;

  Serial.println("=== Game Found! ===");
  Serial.println("Game ID: " + currentGameId);
  Serial.printf("Playing as: %s\n", myColor == 'w' ? "White" : "Black");

  // Get full game state
  LichessGameState state;
  state.myColor = myColor;
  state.gameId = currentGameId;
  state.fen = event.fen; // Use FEN from initial event as fallback

  if (LichessAPI::pollGameStream(currentGameId, state)) {
    Serial.println("Got full game state from stream");
  } else {
    // Fallback: Use data from the initial event
    Serial.println("Warning: Could not get full game state, using initial event data");
    state.gameStarted = true;
    state.gameEnded = false;
    state.lastMove = "";
    // Determine turn from FEN (6th field) or assume White starts
    if (event.fen.length() > 0) {
      int spaceCount = 0;
      for (size_t i = 0; i < event.fen.length(); i++) {
        if (event.fen[i] == ' ') spaceCount++;
        if (spaceCount == 1) {
          state.isMyTurn = (event.fen[i + 1] == 'w' && myColor == 'w') || (event.fen[i + 1] == 'b' && myColor == 'b');
          break;
        }
      }
    } else {
      state.isMyTurn = (myColor == 'w'); // White moves first
    }
  }

  // Sync the board with the current game state
  syncBoardWithLichess(state);

  // Wait for board setup with the current position
  waitForBoardSetup(board);

  Serial.println("Board synchronized! Game starting...");
  wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
}

void ChessLichess::syncBoardWithLichess(const LichessGameState& state) {
  initializeBoard();

  myColor = state.myColor;
  currentGameId = state.gameId;

  // If FEN is provided, use it directly
  if (state.fen.length() > 0 && state.fen != "startpos")
    setBoardStateFromFEN(state.fen);
  else
    Serial.println("No FEN provided, assuming starting position");

  lastKnownMoves = "";
  currentTurn = state.isMyTurn ? myColor : (myColor == 'w' ? 'b' : 'w');

  Serial.printf("My color: %s, Is my turn: %s\n", myColor == 'w' ? "White" : "Black", state.isMyTurn ? "Yes" : "No");
}

void ChessLichess::update() {
  if (gameOver)
    return;

  boardDriver->readSensors();

  int fromRow, fromCol, toRow, toCol;
  char promotion = ' ';

  if ((currentTurn == myColor) && tryPlayerMove(myColor, fromRow, fromCol, toRow, toCol)) {
    // Player's turn - handle physical move
    if (chessEngine->isPawnPromotion(board[fromRow][fromCol], toRow))
      promotion = 'q'; // Default promotion to queen, can be enhanced to allow player choice later
    // Process locally FIRST - show animations immediately
    applyMove(fromRow, fromCol, toRow, toCol, promotion);
    updateGameStatus();
    wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
    // Then send move to Lichess (blocking)
    sendMoveToLichess(fromRow, fromCol, toRow, toCol, promotion);
    boardDriver->updateSensorPrev();
  }

  // Start thinking animation when it's remote player's turn and not already running
  if (currentTurn != myColor && stopAnimation == nullptr && !gameOver)
    stopAnimation = boardDriver->startThinkingAnimation();

  // Polling interval check
  if ((currentTurn == myColor) || millis() - lastPollTime < POLL_INTERVAL_MS) {
    boardDriver->updateSensorPrev();
    return;
  }
  lastPollTime = millis();

  // Remote player's turn - poll Lichess for updates
  LichessGameState state;
  state.myColor = myColor;
  state.gameId = currentGameId;
  if (LichessAPI::pollGameStream(currentGameId, state)) {
    if (state.gameEnded) {
      Serial.println("Game ended! Status: " + state.status);
      if (state.winner.length() > 0)
        Serial.println("Winner: " + state.winner);
      if (stopAnimation) {
        stopAnimation->store(true);
        stopAnimation = nullptr;
      }
      if (state.status == "draw" || state.status == "stalemate" || state.winner == "draw")
        boardDriver->fireworkAnimation(LedColors::Cyan);
      else
        boardDriver->fireworkAnimation(ChessUtils::colorLed((state.winner == "white") ? 'w' : 'b'));
      gameOver = true;
      return;
    }
    // Check if there's a new move
    if (state.lastMove.length() > 0 && state.lastMove != lastKnownMoves) {
      lastKnownMoves = state.lastMove;
      // Skip if this is the move we just sent (avoid processing our own move)
      if (state.lastMove == lastSentMove) {
        Serial.println("Skipping own move echo: " + state.lastMove);
      } else {
        Serial.println("Lichess move received: " + state.lastMove);
        if (ChessUtils::parseUCIMove(state.lastMove, fromRow, fromCol, toRow, toCol, promotion)) {
          if (stopAnimation) {
            stopAnimation->store(true);
            stopAnimation = nullptr;
          }
          Serial.printf("Lichess UCI move: %s = (%d,%d) -> (%d,%d)%s%c\n", state.lastMove.c_str(), fromRow, fromCol, toRow, toCol, promotion == ' ' ? "" : " Promotion to: ", promotion);
          applyMove(fromRow, fromCol, toRow, toCol, promotion, true);
          updateGameStatus();
          wifiManager->updateBoardState(ChessUtils::boardToFEN(board, currentTurn, chessEngine), ChessUtils::evaluatePosition(board));
        } else {
          Serial.println("Failed to parse Lichess UCI move: " + state.lastMove);
        }
      }
    }
  }
  boardDriver->updateSensorPrev();
}

void ChessLichess::sendMoveToLichess(int fromRow, int fromCol, int toRow, int toCol, char promotion) {
  String uciMove = ChessUtils::toUCIMove(fromRow, fromCol, toRow, toCol, promotion);
  Serial.println("Sending move to Lichess: " + uciMove);

  // Track this move so we don't process it as a remote move when it echoes back
  lastSentMove = uciMove;

  // Retry up to 3 times if sending fails
  const int maxRetries = 3;
  int attempt = 0;
  bool sent = false;
  while (attempt < maxRetries && !sent) {
    if (LichessAPI::makeMove(currentGameId, uciMove)) {
      sent = true;
      break;
    } else {
      Serial.printf("ERROR: Failed to send move to Lichess! Attempt %d/%d\n", attempt + 1, maxRetries);
      delay(500);
      attempt++;
    }
  }
  if (!sent) {
    gameOver = true;
    Serial.println("ERROR: All attempts to send move to Lichess failed, ending game!");
    boardDriver->flashBoardAnimation(LedColors::Red);
    lastSentMove = "";
  }
}
