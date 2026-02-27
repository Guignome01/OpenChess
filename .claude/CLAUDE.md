# LibreChess - Project Instructions

> **Sync directive**: This file is mirrored in `.github/copilot-instructions.md`. When modifying either file, always apply the same changes to the other. Both files must remain identical in content.

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
`ChessGame` (abstract base) → `ChessMoves` (human v human) → inherited by `ChessBot` (v Stockfish) → inherited by `ChessLichess` (online play). Each mode implements `begin()` and `update()` called from the main loop. `BoardDriver` and `ChessEngine` are shared via pointer injection — never duplicated.

### Key Components
- **`BoardDriver`** — hardware abstraction: LED strip (NeoPixelBus), sensor grid (shift register), calibration, async animation queue (FreeRTOS task + queue).
- **`ChessEngine`** — pure chess logic: move generation, validation, check/checkmate/stalemate, castling, en passant, repetition detection via Zobrist hashing. No hardware dependencies.
- **`WiFiManagerESP32`** — async web server (`ESPAsyncWebServer`), serves gzipped pages from LittleFS, handles API endpoints, WiFi management, and NVS-persisted settings.
- **`ChessUtils`** — static helpers: FEN ↔ board conversion, material evaluation, NVS init.
- **`MoveHistory`** — LittleFS-based game recording and resume. Binary format with packed headers, UCI-encoded moves, and FEN snapshots. `friend` of `ChessGame` for replay access.
- **`SensorTest`** — standalone sensor testing mode (does not inherit `ChessGame`). Same `begin()`/`update()`/`isComplete()` lifecycle.
- **`BoardMenu` / `MenuNavigator`** — board-as-GUI system: `constexpr MenuItem` arrays, two-phase debounce, stack-based navigation (depth 4). Config in `menu_config.h/cpp`.

For detailed component internals, data flows, concurrency, and storage: see `docs/development/architecture.md`.

### Coordinate System
Board arrays use `[row][col]` where **row 0 = rank 8** (black's back rank), **col 0 = file a**. All internal logic uses this convention consistently.

## Build & Flash

### Prerequisites
- VS Code + PlatformIO IDE extension
- For web asset minification (optional): `npm install -g html-minifier-terser clean-css-cli terser`

### Commands
PlatformIO CLI (`pio`) is not on PATH by default. Use the full path:
- **Windows**: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`
- **Linux**: `~/.platformio/penv/bin/pio`

| Action | VS Code shortcut | CLI |
|--------|-----------------|-----|
| Build | `Ctrl+Alt+B` | `pio run` |
| Upload | `Ctrl+Alt+U` | `pio run -t upload` |
| Serial Monitor | — | `pio device monitor` (115200 baud) |
| Factory reset | — | Add `-DFACTORY_RESET` to `build_flags` in `platformio.ini`, then flash |

### Build Pipeline
Three Python scripts run automatically (defined in `platformio.ini`): `minify.py` (HTML/CSS/JS minification), `prepare_littlefs.py` (gzip + place in `data/`), `upload_fs.py` (hash-based conditional filesystem upload). The `data/` directory is committed to git. Edit source files in `src/web/`, never in `data/`. Files named `*.nogz.*` skip gzip compression.

## Patterns & Conventions

### Code Style
C++ formatting: `.clang-format` (Google style base, no column limit). Run clang-format before committing.

### LED Access
- Multi-step LED updates require a scoped `BoardDriver::LedGuard` (RAII mutex).
- Single animations (`blinkSquare`, `captureAnimation`) are queued and acquire the mutex automatically.
- Long-running animations return `std::atomic<bool>*` — cancel via `stopAndWaitForAnimation(flag)`.
- Use `waitForAnimationQueueDrain()` as a barrier before writing LEDs directly.

### Sensors
Polled every 40ms with 125ms debounce. Always call `boardDriver.readSensors()` before reading state.

### Color Semantics
Colors in `LedColors` (`led_colors.h`) have fixed meanings: `White` = valid move, `Red` = capture/error, `Green` = confirmation, `Yellow` = check/promotion, `Cyan` = piece origin, `Purple` = en passant, `Blue` = thinking, `Orange` = resign. Use consistently. `scaleColor(color, factor)` adjusts brightness.

### NVS Persistence
Settings stored via Arduino `Preferences`. Always call `ChessUtils::ensureNvsInitialized()` before first use.

### Frontend
- `api.js` — low-level fetch wrappers (`getApi`, `postApi`, `deleteApi`).
- `provider.js` — `Api` object with named methods for every endpoint. All pages use `Api.*` — no raw `fetch()` calls.
- When adding a new API endpoint, add a corresponding method to `provider.js`.

## Engineering Principles

- **Separation of Concerns** — each class owns a single responsibility. Hardware in `BoardDriver`, chess rules in `ChessEngine`, network in `WiFiManagerESP32`. Never mix concerns.
- **Loose Coupling** — pointer injection, no global state. Expose minimal public APIs, keep internals private.
- **Dependency Minimization** — prefer bundled ESP-IDF/Arduino functionality (`mbedtls`, FreeRTOS) over external libraries.
- **DRY** — extract shared logic into helpers, base classes, or utilities. No duplication.
- **Reuse Before Creating** — check existing functions/patterns first. Build on existing infrastructure.
- **ESP32 Awareness** — `constexpr` and compile-time computation, minimize heap (fixed arrays, file-scoped statics), `enum class` over raw integers. Be mindful of stack sizes and watchdog timers.
- **Performance & Readability** — optimize for performance, but prioritize readability over cleverness.
- **Simplicity Without Compromise** — keep code simple, but never at the cost of correctness. Don't overlook edge cases.
- **Security by Default** — validate inputs at boundaries, never expose secrets in APIs, hash credentials, use TLS.
- **Code Quality** — validate at boundaries, trust internally. Prefer compile-time checks.
- **Dead Code Cleanup** — remove unused functions, unreachable branches, stale variables after every change.
- **Documentation** — keep `README.md`, `.claude/CLAUDE.md`, `.github/copilot-instructions.md`, and `docs/` in sync with changes. See `docs/development/additional-topics.md` for sync triggers. Code comments explain *why*, not *what*.

## Documentation Reference

For detailed information beyond this summary:
- `docs/development/architecture.md` — component internals, data flows, concurrency, storage, security
- `docs/development/project-standards.md` — coding conventions, naming, frontend patterns
- `docs/development/api.md` — all HTTP API endpoints
- `docs/development/project-structure.md` — file and folder layout
- `docs/development/installation.md` — build setup, dependencies, flashing
- `docs/development/additional-topics.md` — AI-dev workflow, doc sync rules, CLI reference
- `docs/guides/` — board interactions, menus, features, calibration
- `docs/hardware/` — components list, assembly, wiring