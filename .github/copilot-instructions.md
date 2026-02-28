# LibreChess - Project Instructions

> **Sync directive**: This file is mirrored in `.claude/CLAUDE.md`. When modifying either file, always apply the same changes to the other. Both files must remain identical in content.

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
`ChessGame` (abstract base) → `ChessMoves` (human v human) → inherited by `ChessBot` (v Stockfish) → inherited by `ChessLichess` (online play). Each mode implements `begin()` and `update()` called from the main loop. `BoardDriver` and `ChessEngine` are shared via pointer injection — never duplicated.

### Key Components
- **`BoardDriver`** — hardware abstraction: LED strip (NeoPixelBus), sensor grid (shift register), calibration, async animation queue (FreeRTOS task + queue).
- **`ChessBoard`** (`lib/core/`) — complete chess game-state manager. Owns the board array, current turn, game-over flag, result, all position state via `PositionState` (castling rights, en passant target, halfmove/fullmove clocks), and position history (Zobrist hash-based threefold repetition detection). Public API: `newGame()`, `loadFEN()`, `makeMove()` → `MoveResult`, `endGame()`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`, callback/batching. All chess mutations flow through `ChessBoard`; the firmware never modifies the board or turn directly.
- **`ChessRules`** (`lib/core/`) — pure, **stateless** chess logic: all methods are `static`, no member variables. Move generation, validation, check/checkmate/stalemate, castling, en passant. Position-dependent state (castling rights, en passant target) is passed in via `const PositionState&`. No hardware dependencies. Called by `ChessBoard` with `state_` where context is needed.
- **`ChessUtils`** (`lib/core/`) — namespace with utility functions: FEN ↔ board conversion, piece color detection, material evaluation, and inline helper functions. Uses `std::string` (not Arduino `String`). Firmware call sites bridge with `.c_str()` / `std::string()`.
- **`ChessCodec`** (`lib/core/`) — namespace with encoding/decoding helpers: UCI move strings, castling rights strings, compact 2-byte move encoding for binary storage.
- **`types.h`** (`lib/core/`) — shared chess types: `PositionState` struct (castling rights, en passant target, halfmove/fullmove clocks — passed to `ChessRules` static methods and owned by `ChessBoard`), `GameResult` enum, `MoveResult` struct (returned by `ChessBoard::makeMove()`).
- **`SystemUtils`** (`src/`) — Arduino/ESP32 utility functions: `colorLed()`, `printBoard()`, `ensureNvsInitialized()`. Not available in native tests.
- **`WiFiManagerESP32`** — async web server (`ESPAsyncWebServer`), serves gzipped pages from LittleFS, handles API endpoints, WiFi management, and NVS-persisted settings.
- **`MoveHistory`** — LittleFS-based game recording and resume. Binary format with packed headers, UCI-encoded moves, and FEN snapshots. Uses `ChessGame::beginReplay()`/`replayMove()`/`endReplay()` public API for game restoration.
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

## Testing

Unit tests run natively on the host machine (no ESP32 required) using the PlatformIO Unity test framework and the `[env:native]` build environment.

### Running Tests

| Action | Command |
|--------|--------|
| Run all tests | `pio test -e native` |
| Run the suite | `pio test -e native -f test_core` |

### Architecture

The `lib/core/` library contains all natively-compilable chess logic (`ChessBoard`, `ChessRules`, `ChessCodec`, `ChessUtils`, types, Zobrist keys) with zero Arduino dependencies. Tests include `lib/core/` headers directly. Shared test utilities live in `test/test_helpers.h`.

All tests are consolidated under `test/test_core/` — one suite, one binary. Test groups within the suite:
- **`test_rules_moves.cpp`** — move generation for each piece type, blocked/capture scenarios, initial position.
- **`test_rules_check.cpp`** — check detection, checkmate, stalemate, move legality under check, king position.
- **`test_rules_special.cpp`** — castling (rights, blocking, through-check), en passant, promotion, helper functions.
- **`test_utils.cpp`** — FEN round-trip, material evaluation, piece color helpers, 50-move rule, legal move queries.
- **`test_codec.cpp`** — UCI move conversion, castling rights strings.
- **`test_board.cpp`** — ChessBoard lifecycle: new game, moves, captures, en passant, castling, promotion, check, checkmate, stalemate, draws, FEN loading, endGame, callbacks/batching, codec encode/decode.

### Testing Principles

- **Tests guard correctness** — never modify a test to make it pass. If a test fails, fix the production code.
- **Tests must stay in sync** — when changing chess logic in `lib/core/`, update or add tests in the same change.
- **Always test changes** — every logic change must be validated by running the test suite before committing.

For more details, see the [PlatformIO Unit Testing documentation](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html).

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
Settings stored via Arduino `Preferences`. Always call `SystemUtils::ensureNvsInitialized()` before first use.

### Frontend
- `api.js` — low-level fetch wrappers (`getApi`, `postApi`, `deleteApi`).
- `provider.js` — `Api` object with named methods for every endpoint. All pages use `Api.*` — no raw `fetch()` calls.
- When adding a new API endpoint, add a corresponding method to `provider.js`.

## Engineering Principles

- **Separation of Concerns** — each class owns a single responsibility. Hardware in `BoardDriver`, chess rules in `ChessRules`, network in `WiFiManagerESP32`. Never mix concerns.
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
- **Test Discipline** — never modify a test to accommodate a bug. Tests guard correctness; fix the code, not the test. Keep tests in sync with logic changes.
- **Documentation** — keep `README.md`, `.claude/CLAUDE.md`, `.github/copilot-instructions.md`, and `docs/` in sync with changes. See `docs/development/additional-topics.md` for sync triggers. Code comments explain *why*, not *what*.

## Documentation Reference

Before analyzing or modifying a component, read the relevant documentation below to understand existing design decisions and constraints. Match the doc to the task: architecture changes → `architecture.md`, API work → `api.md`, hardware interactions → `docs/guides/`, etc.

For detailed information beyond this summary:
- `docs/development/architecture.md` — component internals, data flows, concurrency, storage, security
- `docs/development/project-standards.md` — coding conventions, naming, frontend patterns
- `docs/development/api.md` — all HTTP API endpoints
- `docs/development/project-structure.md` — file and folder layout
- `docs/development/installation.md` — build setup, dependencies, flashing
- `docs/development/additional-topics.md` — AI-dev workflow, doc sync rules, CLI reference
- `docs/guides/` — board interactions, menus, features, calibration
- `docs/hardware/` — components list, assembly, wiring