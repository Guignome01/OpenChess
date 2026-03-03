# LibreChess - Project Instructions

> **Sync directive**: This file is mirrored in `.claude/CLAUDE.md`. When modifying either file, always apply the same changes to the other. Both files must remain identical in content.

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
`ChessGame` (abstract base) → `ChessPlayer` (human v human) | `ChessBot` (abstract, Template Method) → `ChessStockfish` (v Stockfish) / `ChessLichess` (online play). `ChessBot::update()` defines the game loop skeleton; subclasses override hooks (`requestEngineMove()`, `onPlayerMoveApplied()`, `getEngineEvaluation()`). `BoardDriver` is shared via pointer injection. Each `ChessGame` owns a `ChessBoard` (which internally owns a `ChessRules`) — no global chess state.

### Key Components
- **`BoardDriver`** — hardware abstraction: LED strip (NeoPixelBus), sensor grid (shift register), calibration, async animation queue (FreeRTOS task + queue).
- **`ChessBoard`** (`lib/core/`) — complete chess game-state manager. Owns the board array, current turn, game-over flag, result, all position state via `PositionState` (castling rights, en passant target, halfmove/fullmove clocks), and position history (Zobrist hash-based threefold repetition detection). Insufficient material detection (K vs K, K+B/N vs K, K+B vs K+B same-color). `getFen()` and `getEvaluation()` use dirty-flag caching to avoid recomputation. Public API: `newGame()`, `loadFEN()` → `bool` (validates FEN before applying; returns false and leaves board unchanged on invalid input), `makeMove()` → `MoveResult`, `endGame()`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`, callback/batching. Convenience wrappers: `getPossibleMoves()`, `isKingInCheck()`, `findKingPosition()`, `checkEnPassant()`, `checkCastling()`. All chess mutations flow through `ChessBoard`; the firmware never modifies the board or turn directly.
- **`ChessRules`** (`lib/core/`) — pure, **stateless** chess logic: all methods are `static`, no member variables. Move generation, validation, check/checkmate/stalemate, castling, en passant. Position-dependent state (castling rights, en passant target) is passed in via `const PositionState&`. No hardware dependencies. Called by `ChessBoard` with `state_` where context is needed.
- **`ChessUtils`** (`lib/core/`) — namespace with utility functions: FEN ↔ board conversion, piece color detection, material evaluation, inline helper functions (`opponentColor`, `squareName`, `colorName`), and struct-returning analyzers (`checkEnPassant()` → `EnPassantInfo`, `checkCastling()` → `CastlingInfo`, `updateCastlingRights()`). Uses `std::string` (not Arduino `String`). Firmware call sites bridge with `.c_str()` / `std::string()`.
- **`ChessCodec`** (`lib/core/`) — namespace with encoding/decoding helpers: UCI move strings, castling rights strings, compact 2-byte move encoding for binary storage.
- **`types.h`** (`lib/core/`) — shared chess types: `PositionState` struct (castling rights, en passant target, halfmove/fullmove clocks — passed to `ChessRules` static methods and owned by `ChessBoard`) with `initial()` static factory, `GameResult` enum class, `GameMode` enum class, `MoveResult` struct (returned by `ChessBoard::makeMove()`), `GameHeader` packed struct (16 bytes), storage constants, `MAX_MOVES_PER_PIECE`, `invalidMoveResult()` factory.
- **`SystemUtils`** (`src/`) — Arduino/ESP32 utility functions: `colorLed()`, `printBoard()`, `ensureNvsInitialized()`. Not available in native tests.
- **`WiFiManagerESP32`** — async web server (`ESPAsyncWebServer`), serves gzipped pages from LittleFS, handles API endpoints, WiFi management, and NVS-persisted settings.
- **`GameRecorder`** (`lib/core/`) — pure recording orchestration: encodes moves via `ChessCodec`, manages `GameHeader`, delegates persistence to `IGameStorage`. Flushes header every full turn (2 half-moves) to reduce flash wear. Validates moves during replay. Replays games directly into a `ChessBoard` (no game-layer involvement). Depends only on core interfaces.
- **`GameController`** (`lib/core/`) — thin facade coordinating `ChessBoard` + `GameRecorder` + `IGameObserver`. Atomically applies board mutations, records moves, auto-finishes on game-end, and notifies the observer. `startNewGame(mode, ...)` atomically resets the board and begins recording. `endGame()` is guarded against double-calls. Exposes convenience pass-throughs: `getPossibleMoves()`, `isKingInCheck()`, `findKingPosition()`, `checkEnPassant()`, `checkCastling()`. Does not expose `positionState()` — that remains on `ChessBoard` only (used in tests). All game classes interact with chess state through this facade.
- **`IGameStorage`** / **`IGameObserver`** / **`ILogger`** (`lib/core/`) — abstract interfaces for persistence, board-state notification, and logging. First abstract interfaces in the project; enable DI and native testing.
- **`LittleFSStorage`** (`src/`) — concrete `IGameStorage` backed by LittleFS on ESP32. Manages `/games/` directory, binary game files, JSON game list API.
- **`SerialLogger`** (`src/`) — concrete `ILogger` using Arduino `Serial`.
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