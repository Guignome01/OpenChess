# LibreChess - Project Instructions

> **Sync directive**: This file is mirrored in `.claude/CLAUDE.md`. When modifying either file, always apply the same changes to the other. Both files must remain identical in content.

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
**Core (`lib/core/`)**: `ChessGame` is the central game orchestrator composing `ChessBoard` (board representation + position logic), `ChessHistory` (in-memory move log + persistent game recording), and optionally `IGameObserver`. All chess-state mutations flow through `ChessGame`.

**Firmware (`src/game/`)**: `GameMode` (abstract base) → `PlayerMode` (human v human) | `BotMode` (abstract, Template Method) → `StockfishMode` (v Stockfish) / `LichessMode` (online play). `BotMode::update()` defines the game loop skeleton; subclasses override hooks (`requestEngineMove()`, `onPlayerMoveApplied()`, `getEngineEvaluation()`). `BoardDriver` is shared via pointer injection. Each `GameMode` holds a `ChessGame*` — no global chess state.

### Key Components
- **`BoardDriver`** — hardware abstraction: LED strip (NeoPixelBus), sensor grid (shift register), calibration, async animation queue (FreeRTOS task + queue).
- **`ChessBoard`** (`lib/core/`) — board representation and position-level chess logic. Owns the board array, current turn, game-over flag, result, and all position state via `PositionState` (castling rights, en passant target, halfmove/fullmove clocks). Game-end detection via `detectGameEnd()`: checkmate, stalemate, 50-move draw, insufficient material (K vs K, K+B/N vs K, K+B vs K+B same-color), and threefold repetition (via Zobrist hashing). `getFen()` and `getEvaluation()` use dirty-flag caching to avoid recomputation. Public API: `newGame()`, `loadFEN()` → `bool` (validates FEN before applying; returns false and leaves board unchanged on invalid input), `makeMove()` → `MoveResult`, `endGame()`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`. Undo/redo support: `reverseMove(const MoveEntry&)` restores board state before a move (pieces, captured piece, castling rook, position state, Zobrist history, game-over flag), `applyMoveEntry(const MoveEntry&)` replays a move from a `MoveEntry`. Convenience wrappers: `getPossibleMoves()`, `isKingInCheck()`, `inCheck()`, `isCheckmate()`, `isStalemate()`, `isInsufficientMaterial()`, `isAttacked()`, `findPiece()`, `findKingPosition()`, `checkEnPassant()`, `checkCastling()`, `moveNumber()`. Note: move history, callbacks, undo/redo, and batching live in `ChessGame`.
- **`ChessRules`** (`lib/core/`) — pure, **stateless** chess logic: all methods are `static`, no member variables. Move generation, validation, check/checkmate/stalemate, castling, en passant. Position-dependent state (castling rights, en passant target) is passed in via `const PositionState&`. No hardware dependencies. Called by `ChessBoard` with `state_` where context is needed.
- **`ChessUtils`** (`lib/core/`) — namespace with utility functions: FEN ↔ board conversion, piece color detection, material evaluation, castling rights string formatting/parsing, inline helper functions (`opponentColor`, `squareName`, `colorName`, `fileChar`, `rankChar`, `fileIndex`, `rankIndex`), and struct-returning analyzers (`checkEnPassant()` → `EnPassantInfo`, `checkCastling()` → `CastlingInfo`, `updateCastlingRights()`). Uses `std::string` (not Arduino `String`). Firmware call sites bridge with `.c_str()` / `std::string()`.
- **`ChessNotation`** (`lib/core/`) — namespace with move notation conversion: coordinate (`"e2e4"`), SAN (`"Nf3"`), and LAN (`"Ng1-f3"`) output and parsing. All functions are pure — board state and position are passed in as parameters (consistent with the `ChessRules` stateless pattern). Output functions omit check/checkmate suffixes; the caller (`ChessGame::getHistory`) appends those after applying the move. `MoveFormat` enum (`COORDINATE`, `SAN`, `LAN`) in `types.h` selects the format.
- **`ChessHistory`** (`lib/core/`) — in-memory game history and persistent game recording. Move log (`MoveEntry` structs with full metadata including previous position state for undo) with cursor-based undo/redo: `addMove()`, `undoMove()`, `redoMove()`, `canUndo()`, `canRedo()`. Branch-on-undo: `addMove()` at a branch point wipes future moves and truncates storage via `IGameStorage::truncateMoveData()`. Recording is automatic when `IGameStorage*` is present and a header has been set via `setHeader()`. Recording API: `setHeader()`, `snapshotPosition()`, `save()`, `discard()`. Compact 2-byte move encoding via public static `encodeMove()`/`decodeMove()` methods. Manages `GameHeader`, delegates persistence to `IGameStorage`, flushes header every full turn to reduce flash wear, validates moves during replay, replays games directly into a `ChessBoard`. Fixed-size arrays (ESP32-friendly). Composed by `ChessGame`.
- **`ChessGame`** (`lib/core/`) — central game orchestrator. Composes `ChessBoard`, `ChessHistory`, and optionally `IGameObserver`. Constructor: `(IGameStorage*, IGameObserver*, ILogger*)`. All chess-state mutations (`makeMove`, `loadFEN`, `endGame`, `newGame`) flow through this class. Handles move history recording (automatic via `ChessHistory::addMove()`), callback dispatch, batching, and undo/redo (`undoMove()`, `redoMove()`, `canUndo()`, `canRedo()` — step the history cursor and update the board via `reverseMove()`/`applyMoveEntry()`). Navigation queries: `currentMoveIndex()` (0 = start position), `moveCount()`, `getHistory(out[], maxMoves, MoveFormat)` (populates array with formatted move strings — COORDINATE, SAN, or LAN). Exposes the full public API: lifecycle methods, board queries, draw detection (`isDraw()`, `isThreefoldRepetition()`), history access, and notation convenience methods (`makeMove(string)`, `toCoordinate()`, `parseCoordinate()`, `parseMove()`). Firmware game modes interact with chess state exclusively through `ChessGame*`.
- **`types.h`** (`lib/core/`) — shared chess types: `PositionState` struct (castling rights, en passant target, halfmove/fullmove clocks — passed to `ChessRules` static methods and owned by `ChessBoard`) with `initial()` static factory, `GameResult` enum class, `GameModeId` enum class, `MoveFormat` enum class (`COORDINATE`, `SAN`, `LAN`), `MoveResult` struct (returned by `ChessBoard::makeMove()`), `MoveEntry` struct (move log record), `GameHeader` packed struct (16 bytes), storage constants, `MAX_MOVES_PER_PIECE`, `invalidMoveResult()` factory.
- **`SystemUtils`** (`src/`) — Arduino/ESP32 utility functions: `colorLed()`, `printBoard()`, `ensureNvsInitialized()`. Not available in native tests.
- **`WiFiManagerESP32`** — async web server (`ESPAsyncWebServer`), serves gzipped pages from LittleFS, handles API endpoints, WiFi management, and NVS-persisted settings. Holds a `const ChessGame*` reference for read-only navigation queries. Caches board state and navigation data in `onBoardStateChanged()` for the web UI. `POST /nav` endpoint for move history navigation (undo/redo/first/last) with flag-based relay to the main loop.
- **`IGameStorage`** / **`IGameObserver`** / **`ILogger`** (`lib/core/`) — abstract interfaces for persistence, board-state notification, and logging. Enable DI and native testing.
- **`LittleFSStorage`** (`src/`) — concrete `IGameStorage` backed by LittleFS on ESP32. Manages `/games/` directory, binary game files, JSON game list API.
- **`SerialLogger`** (`src/`) — concrete `ILogger` using Arduino `Serial`.
- **`SensorTest`** — standalone sensor testing mode (does not inherit `GameMode`). Same `begin()`/`update()`/`isComplete()` lifecycle.
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

The `lib/core/` library contains all natively-compilable chess logic (`ChessGame`, `ChessBoard`, `ChessHistory`, `ChessRules`, `ChessNotation`, `ChessUtils`, types, Zobrist keys) with zero Arduino dependencies. Tests include `lib/core/` headers directly. Shared test utilities live in `test/test_helpers.h`.

All tests are consolidated under `test/test_core/` — one suite, one binary. Test groups within the suite:
- **`test_chess_rules_moves.cpp`** — move generation for each piece type, blocked/capture scenarios, initial position.
- **`test_chess_rules_check.cpp`** — check detection, checkmate, stalemate, move legality under check, king position.
- **`test_chess_rules_special.cpp`** — castling (rights, blocking, through-check), en passant, promotion, helper functions.
- **`test_chess_utils.cpp`** — FEN round-trip, material evaluation, piece color helpers, 50-move rule, legal move queries, castling rights strings.
- **`test_chess_notation.cpp`** — coordinate/SAN/LAN output and parsing, auto-format detection, roundtrip verification.
- **`test_chess_board.cpp`** — ChessBoard: new game, moves, captures, en passant, castling, promotion, check, checkmate, stalemate, 50-move draw, insufficient material, threefold repetition, FEN loading, endGame, encode/decode, API queries, reverseMove (normal/capture/en passant/castling/promotion/game-over), applyMoveEntry.
- **`test_chess_game.cpp`** — ChessGame: draw detection, callbacks/batching, history integration, undo/redo, getHistory (coordinate/SAN/LAN).
- **`test_chess_history.cpp`** — ChessHistory: move log with undo/redo, branch-on-undo.
- **`test_chess_history_recording.cpp`** — ChessHistory recording: persistence lifecycle, header flush, replay, branch-truncation, MockGameStorage. ChessGame recording integration: startNewGame, endGame, resume, auto-finish, observer notification. Compact encode/decode, on-disk format stability.

### Testing Principles

- **Tests guard correctness** — never modify a test to make it pass. If a test fails, fix the production code.
- **Tests must stay in sync** — when changing chess logic in `lib/core/`, update or add tests in the same change.
- **Always test changes** — every logic change must be validated by running the test suite before committing.
- **Test file mirroring** — each `lib/core/src/` source file has a corresponding `test/test_core/test_*.cpp` file (e.g., `chess_notation.cpp` → `test_chess_notation.cpp`). Place tests in the file that mirrors the tested functionality. When creating a new core source file, create a matching test file and register it in `test_core.cpp`.

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
- **Facade via ChessGame** — firmware accesses chess logic exclusively through `ChessGame`. Never include `ChessBoard`, `ChessHistory`, or `ChessRules` directly from firmware (`src/`). Native tests may include internal `lib/core/` headers.
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