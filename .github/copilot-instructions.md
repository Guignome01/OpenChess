# OpenChess - AI Coding Instructions

## Project Overview
ESP32-based smart chess board with hall effect sensors, WS2812B LEDs, and Stockfish AI integration via WiFi. Built with PlatformIO + Arduino framework.

## Architecture

### Core Components
- **BoardDriver** ([board_driver.h](../src/board_driver.h)): Hardware abstraction - LEDs, sensors, shift registers, animations, calibration
- **ChessEngine** ([chess_engine.h](../src/chess_engine.h)): Pure chess logic - move validation, check/checkmate detection, castling/en passant rules
- **ChessGame** ([chess_game.h](../src/chess_game.h)): Base class for game modes with shared board state and turn management
- **WiFiManagerESP32** ([wifi_manager_esp32.h](../src/wifi_manager_esp32.h)): Async web server, settings persistence via NVS Preferences

### Game Mode Inheritance
```
ChessGame (base) ─┬─ ChessMoves (human vs human)
                  └─ ChessBot (human vs Stockfish AI)
```
Both inherit from `ChessGame` and implement `begin()` and `update()` virtual methods.

### Data Flow
1. `main.cpp` owns all core objects, handles game selection via physical piece placement or web UI
2. `BoardDriver::readSensors()` applies debouncing before state changes propagate
3. FEN strings are the interchange format between web UI ↔ game modes ↔ Stockfish API
4. `wifiManager.updateBoardState(fen, eval)` pushes state to web clients

## Build System

### Commands
- **Build & Upload**: `Ctrl+Alt+U` in VS Code with PlatformIO extension
- **Monitor Serial**: `pio device monitor` (115200 baud)

### Web Asset Pipeline (runs automatically on build)
1. `minify.py` - Minifies HTML/CSS/JS in `src/web/` → `src/web/build/` (requires npm packages)
2. `generate_pages.py` - Gzip compresses and embeds into `web_pages.cpp`/`page_router.cpp`

**To edit web UI**: Modify files in `src/web/`, NOT `src/web/build/` (auto-generated).

Install minifiers (optional but recommended):
```bash
npm install -g html-minifier-terser clean-css-cli terser
```

## Conventions

### Coordinate System
- Board array: `board[row][col]` where row 0 = rank 8, col 0 = file a
- Piece chars: uppercase = white (`KQRBNP`), lowercase = black (`kqrbnp`), empty = `' '`

### State Management
- Castling rights: bitmask in `ChessEngine` (0x01=K, 0x02=Q, 0x04=k, 0x08=q)
- Turn tracking: `currentTurn` char (`'w'` or `'b'`) in game classes
- Persistence: Use `Preferences` API with `ChessUtils::ensureNvsInitialized()` first

### Hardware Pins (configurable in board_driver.h)
```cpp
LED_PIN 32          // WS2812B data
SR_CLK_PIN 14       // 74HC595 shift clock
SR_LATCH_PIN 26     // 74HC595 latch
SR_SER_DATA_PIN 33  // 74HC595 serial data
ROW_PIN_0-7         // Hall sensor rows
```

### LED Colors (defined in led_colors.h)
Use named constants: `COLOR_MOVE_INDICATOR`, `COLOR_ATTACK`, `COLOR_CHECK`, etc.

## Key Patterns

### Adding a New Game Mode
1. Create class inheriting `ChessGame`
2. Implement `begin()` for initialization, `update()` for main loop
3. Add enum value in `main.cpp` `GameMode`
4. Handle in `initializeSelectedMode()` and main `loop()` switch

### Stockfish Integration
```cpp
// See chess_bot.cpp for full example
StockfishSettings::medium()  // Depth presets: easy(5), medium(8), hard(11), expert(15)
StockfishAPI::buildRequestURL(fen, depth)
StockfishAPI::parseResponse(json, response)  // Returns StockfishResponse struct
```

### Web API Endpoints (defined in wifi_manager_esp32.cpp)
- `GET /board-update` - Current FEN + evaluation POST: apply move from web UI
- `POST /board-update` - Apply move from web UI
- `GET /wifi-info` - Current WiFi status
- `POST /connect-wifi` - Connect to specified WiFi network
- `POST /gameselect` - Select game mode + bot config