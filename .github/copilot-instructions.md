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
Sensors are polled every `SENSOR_READ_DELAY_MS` (40ms) with `DEBOUNCE_MS` (125ms) debounce. The game selection screen implements a two-phase debounce (must see empty→occupied transition). Always call `boardDriver.readSensors()` before reading state.

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
Each mode follows: `begin()` (setup board, wait for pieces) → `update()` (poll sensors, process moves) → `isGameOver()` triggers return to selection screen. The main loop in `main.cpp` orchestrates this state machine.

## Pin Configuration
GPIO pins are `#define`d in `board_driver.h`. The calibration system maps physical pin order to logical board coordinates, so **pin assignment order doesn't matter** — calibration handles it.
