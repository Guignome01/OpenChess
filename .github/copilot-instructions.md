# OpenChess - Copilot Instructions

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
`ChessGame` (abstract base) → `ChessMoves` (human v human) → inherited by `ChessBot` (v Stockfish) → inherited by `ChessLichess` (online play). Each mode implements `begin()` and `update()` called from the main loop. `BoardDriver` and `ChessEngine` are shared via pointer injection — never duplicated.

### Key Components
- **`BoardDriver`** — hardware abstraction: LED strip (NeoPixelBus, I2S DMA output), sensor grid (shift register column scan + row GPIO reads), calibration (NVS-persisted), and async animation queue (FreeRTOS task + queue).
- **`ChessEngine`** — pure chess logic: move generation, validation, check/checkmate/stalemate, castling rights, en passant, 50-move rule. No hardware dependencies.
- **`WiFiManagerESP32`** — async web server (`ESPAsyncWebServer`), serves gzipped pages from LittleFS via `serveStatic`, handles API endpoints for board state, game selection, settings. Also manages WiFi credentials and Lichess token persistence via `Preferences` (NVS).
- **`ChessUtils`** — static helpers: FEN ↔ board conversion, material evaluation, NVS init.
- **`BoardMenu`** — reusable board GUI primitive: displays selectable options as colored LEDs on the 8×8 grid. Uses two-phase debounce (empty→occupied transition over `DEBOUNCE_CYCLES`) for reliable piece-placement selection. Supports orientation flipping (`setFlipped`) for black-side players, configurable back button, and blink feedback on selection. Items are `constexpr MenuItem` arrays stored in flash — no heap allocation. Provides both non-blocking `poll()` and blocking `waitForSelection()`.
- **`MenuNavigator`** — stack-based menu orchestrator (fixed depth of 4). Manages push/pop navigation across `BoardMenu` instances, auto-handles back-button pops, and re-displays parent menus. Does not own menu objects — they are file-scoped statics.

### Coordinate System
Board arrays use `[row][col]` where **row 0 = rank 8** (black's back rank), **col 0 = file a**. All internal logic uses this convention consistently.

## Build & Flash

### Prerequisites
- VS Code + PlatformIO IDE extension
- For web asset minification (optional): `npm install -g html-minifier-terser clean-css-cli terser`

### Build Pipeline
PlatformIO runs two **pre-build Python scripts** and one **extra script** (defined in `platformio.ini`):
1. `src/web/build/minify.py` — minifies HTML/CSS/JS from `src/web/` → `src/web/build/` (gracefully skips if npm tools absent)
2. `src/web/build/prepare_littlefs.py` — gzip-compresses assets and places them in `data/` for LittleFS filesystem upload, then **deletes** all minified files from `src/web/build/`
3. `src/web/build/upload_fs.py` — hooks into `pio run -t upload`: hashes `data/` contents, compares with `.littlefs_hash`, and uploads the filesystem image only when web assets change

The `data/` directory is **committed to git** so users without minification tools can still build and flash. `.littlefs_hash` is git-ignored. Edit source HTML/CSS/JS in `src/web/` instead.

### Commands
PlatformIO CLI (`pio`) is not on PATH by default. Use the full path:
- **Windows**: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`
- **Linux**: `~/.platformio/penv/bin/pio`

| Action | VS Code shortcut | CLI |
|--------|-----------------|-----|
| Build | `Ctrl+Alt+B` | `pio run` |
| Upload | `Ctrl+Alt+U` | `pio run -t upload` |
| Serial Monitor | — | `pio device monitor` (115200 baud) |

## Patterns & Conventions

### LED Mutex
LED strip access is guarded by a FreeRTOS mutex via `BoardDriver::LedGuard` (RAII). For multi-step LED updates, use a scoped `{ BoardDriver::LedGuard guard(boardDriver); ... }` block. Single animation calls (e.g., `blinkSquare`, `captureAnimation`) are queued and acquire the mutex automatically.

### Async Animations
Animations run on a dedicated FreeRTOS task via a queue (`AnimationJob`). Long-running animations (`waiting`, `thinking`) return an `std::atomic<bool>*` stop flag — use `stopAndWaitForAnimation(flag)` to cancel, block until finished, and free the flag (single-owner lifecycle). Use `waitForAnimationQueueDrain()` as a barrier before writing LEDs directly, to ensure all queued animations have completed.

### Sensor Debouncing
Sensors are polled every `SENSOR_READ_DELAY_MS` (40ms) with `DEBOUNCE_MS` (125ms) debounce. Always call `boardDriver.readSensors()` before reading state.

### Menu System
The board doubles as a primitive 8×8 pixel GUI via `BoardMenu` and `MenuNavigator` (defined in `board_menu.h/cpp` and `menu_navigator.h/cpp`).

- **`MenuItem`** — `{row, col, color, id}` struct. Positions authored in white-side orientation (row 7 = rank 1). Use `constexpr` file-scoped arrays so data lives in flash.
- **Two-phase debounce** — every menu square must be seen empty for `DEBOUNCE_CYCLES` (5) consecutive polls, then occupied for the same count. This prevents false triggers from pieces already on the board. Implemented in `BoardMenu::updateDebounce()`.
- **Selection feedback** — on confirmed selection, the square blinks once in its own color via `blinkSquare()` before returning the id.
- **Back button** — set via `setBackButton(row, col)`, lit in `LedColors::White`. Omit for root menus. The navigator auto-pops on back and re-shows the parent.
- **Orientation** — `setFlipped(true)` mirrors row coordinates (`row' = 7 - row`) so menus face a black-side player. Default is white-side.
- **ID routing** — use distinct id ranges per menu level (0–9 root, 10–19 sub-menu, 20–29 sub-sub, etc.) so the `loop()` handler can identify which menu produced a result without callbacks or virtual dispatch.
- **`boardConfirm()`** — standalone yes/no dialog (green/red squares). Blocking. Returns `bool`.
- **No heap allocation** — menus are file-scoped statics, item arrays are `constexpr`, navigator uses a fixed `std::array<BoardMenu*, 4>` stack.

### Color Semantics (LedColors namespace)
Colors have fixed meanings in `led_colors.h`: `White` = valid move, `Red` = capture/error, `Green` = confirmation, `Yellow` = check/promotion, `Cyan` = piece origin, `Purple` = en passant, `Blue` = bot thinking. Use these consistently.

### Web Assets & `.nogz.` Convention
Files named `*.nogz.*` (e.g., `capture.nogz.mp3`) skip gzip compression in the build pipeline — used for binary files that don't benefit from gzip or need raw serving.

### NVS Persistence
Settings (WiFi creds, Lichess token, LED brightness, calibration) are stored in ESP32 NVS via Arduino `Preferences`. Always call `ChessUtils::ensureNvsInitialized()` before first use.

### External APIs
- **Stockfish**: HTTPS calls to `stockfish.online` — see `StockfishAPI` for request/response handling. Difficulty is controlled by search `depth` (5/8/11/15).
- **Lichess**: HTTPS to `lichess.org` — polling-based game stream, move submission. Token stored in NVS.

### Game Mode Lifecycle
Each mode follows: `begin()` (setup board, wait for pieces) → `update()` (poll sensors, process moves) → `isGameOver()` triggers return to selection screen. The main loop in `main.cpp` uses `MenuNavigator` to push/pop menus (game selection → bot difficulty → bot color), then calls `initializeSelectedMode()` once a final mode is chosen.

## Pin Configuration
GPIO pins are `#define`d in `board_driver.h`. The calibration system maps physical pin order to logical board coordinates, so **pin assignment order doesn't matter** — calibration handles it.
