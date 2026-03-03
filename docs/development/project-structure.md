# Project Structure

A comprehensive map of the codebase, covering firmware, web frontend, build tools, and runtime storage.

## Top-Level Layout

```
├── src/                    Firmware source code and web frontend sources
├── lib/core/               Natively-compilable chess engine library (no Arduino deps)
├── test/                   Native unit tests (PlatformIO Unity framework)
├── data/                   Pre-built web assets (gzip-compressed) for LittleFS
├── docs/                   Project documentation
├── BuildGuide/             Build photos and schematics (to be updated)
├── platformio.ini          PlatformIO build configuration
├── LibreChess.code-workspace VS Code workspace file
├── grid_scan_test.cpp      Standalone sensor grid debugging utility
├── .clang-format           C++ code formatting rules (Google style base)
├── .github/                GitHub Copilot instructions
├── .claude/                Claude AI instructions
├── LICENSE                 Project license
└── README.md               Project overview and quick start
```

## Firmware (`src/`)

### Core

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point: `setup()` and `loop()`. Game mode selection, menu routing, WiFi/resign/board-edit relay, and game lifecycle management. |
| `board_driver.h/.cpp` | Hardware abstraction: LED strip (NeoPixelBus, I2S DMA), sensor grid (shift register scan + GPIO reads), calibration (NVS-persisted), LED settings (brightness, dimming), and async animation queue (FreeRTOS task + queue). GPIO pin definitions. |
| `system_utils.h/.cpp` | Arduino/ESP32 utility functions: `colorLed()` (piece char → LED color), `printBoard()` (Serial debug), `ensureNvsInitialized()` (Preferences guard). Not available in native tests. |
| `led_colors.h` | `LedRGB` struct and named color constants (Cyan, White, Red, Green, Yellow, Purple, Orange, Blue, etc.) with `scaleColor()` brightness helper. |

### Game Modes

| File | Purpose |
|------|---------|
| `game/base.h/.cpp` | Abstract base class for all game modes. Holds a `GameController*` that coordinates chess state, recording, and observer notification. Implements shared logic: `tryPlayerMove()`, `applyMove()` (delegates to `GameController::makeMove()`), `waitForBoardSetup()`, `tryResumeGame()`, resign gesture handling, and LED feedback helpers. |
| `game/bot.h/.cpp` | Abstract intermediate class for remote-engine modes (Template Method pattern). Concrete `update()` drives the game loop: player turn → `tryPlayerMove()` → `applyMove()` → `onPlayerMoveApplied()` hook; engine turn → `requestEngineMove()`. Provides thinking animation helpers, remote-move LED guidance (`waitForRemoteMoveCompletion()`), and resign with animation lifecycle. |
| `game/player.h/.cpp` | Human vs Human mode. Minimal subclass of `ChessGame` — implements `begin()` (board setup, game recording) and `update()` (sensor polling, move processing). |
| `game/stockfish.h/.cpp` | Human vs Stockfish mode. Extends `ChessBot` — overrides `requestEngineMove()` to call the Stockfish API and `getEngineEvaluation()` to return the engine's score. |
| `game/lichess.h/.cpp` | Lichess online mode. Extends `ChessBot` — overrides `requestEngineMove()` with Lichess game stream polling, `onPlayerMoveApplied()` to send moves to Lichess, and `handleResign()` to also resign on the Lichess API. |
| `sensor_test.h/.cpp` | Standalone sensor diagnostic mode (does not inherit `ChessGame`). Tracks visited squares, lights them white, completes when all 64 are visited. |

### External APIs

| File | Purpose |
|------|---------|
| `stockfish_api.h/.cpp` | Stockfish API client. Builds request URLs, parses JSON responses (evaluation, best move, continuation). Connects to `stockfish.online` over HTTPS. |
| `stockfish_settings.h` | 8 difficulty presets (beginner through master, depths 3–17, scaled timeouts 10s–65s). `StockfishSettings::fromLevel(int)` factory. |
| `lichess_api.h/.cpp` | Lichess API client. Token management, game event polling, game stream polling, move submission, and resignation. Connects to `lichess.org` over HTTPS. |

### Infrastructure

| File | Purpose |
|------|---------|
| `wifi_manager_esp32.h/.cpp` | WiFi connection management (state machine with AP/STA modes), async web server (ESPAsyncWebServer), all HTTP API endpoints, mDNS, known-networks registry (NVS), OTA password management, and board state relay to the web UI. |
| `littlefs_storage.h/.cpp` | Concrete `IGameStorage` backed by LittleFS. Manages `/games/` directory, binary game files (header + moves + FEN table), storage limits enforcement, and JSON game list API for the web UI. |
| `serial_logger.h/.cpp` | Concrete `ILogger` using Arduino `Serial`. |
| `board_menu.h/.cpp` | Reusable board menu primitive. Displays options as colored LEDs, uses two-phase debounce for selection, supports orientation flipping, back buttons, and blink feedback. Also provides `boardConfirm()` dialog. |
| `menu_navigator.h/.cpp` | Stack-based menu orchestrator (max depth 4). Push/pop navigation, auto back-button handling, parent menu re-display. |
| `menu_config.h/.cpp` | Menu layout definitions. `MenuId` namespace with ID ranges per level, `constexpr MenuItem[]` arrays for each menu, extern menu/navigator instances, and `initMenus()` two-phase initializer. |

## Web Frontend (`src/web/`)

The ESP32 serves a web interface directly from flash storage. The frontend is built with vanilla HTML, CSS, and JavaScript — no build framework or SPA router. Each page is a self-contained HTML file that includes shared scripts.

### Pages

| File | Purpose |
|------|---------|
| `index.html` | Home and settings page. WiFi network management, Lichess token, LED brightness/dimming, board recalibration, OTA firmware upload, and OTA password security. |
| `board.html` | Board view and interaction page. Live board display with evaluation bar, move history with navigation, board editor (drag-and-drop, FEN, castling/en-passant controls), game history browser, game review mode, settings popup (themes, colors, sounds), focus mode, and resign button. |
| `game.html` | Game mode selection page. Four mode cards with bot configuration panel (color, difficulty). Redirects to board page after selection. |

### Scripts (`src/web/scripts/`)

| File | Purpose |
|------|---------|
| `api.js` | Low-level HTTP utilities: `getApi()`, `postApi()`, `deleteApi()` fetch wrappers and `pollHealth()` for OTA reboot polling. |
| `provider.js` | Domain-specific API client. `Api` object with named methods for every backend endpoint (e.g., `Api.getNetworks()`, `Api.resign()`, `Api.selectGame()`). All pages use `Api.*` methods — no page contains raw fetch calls. |
| `chessboard-1.0.0.min.js` | Third-party chessboard rendering library. |
| `jquery-4.0.0.min.js` | jQuery (dependency of chessboard.js). |

### Styles (`src/web/css/`)

| File | Purpose |
|------|---------|
| `styles.css` | Application styles. Dark theme, responsive layout, game mode cards, board controls, evaluation bar, FEN editor, OTA dropzone, settings popup, game history cards, review panel. |
| `chessboard-1.0.0.min.css` | Third-party chessboard styles. |

### Assets

- `src/web/pieces/` — SVG chess piece images (12 files: `wK.svg`, `bQ.svg`, etc.)
- `src/web/sounds/` — Move sounds (`move.nogz.mp3`, `capture.nogz.mp3`). The `.nogz.` naming convention prevents gzip compression in the build pipeline — these are served as raw binary files.

## Chess Engine Library (`lib/core/`)

The `lib/core/` library contains all natively-compilable chess logic with zero Arduino dependencies. PlatformIO's Library Dependency Finder auto-discovers it for both the ESP32 and native test environments. All code uses `std::string` (not Arduino `String`); firmware call sites bridge with `.c_str()` / `std::string()`.

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library descriptor. |
| `src/rules.h/.cpp` | `ChessRules` class: stateless, all-static chess logic. Move generation, legal move filtering, check/checkmate/stalemate, castling validation, en passant, promotion. Position-dependent state (castling rights, en passant target) is passed in via `const PositionState&`. |
| `src/board.h/.cpp` | `ChessBoard` class: complete game-state manager. Owns the board array, current turn, game-over flag, result, and all position state via `PositionState` (castling rights, en passant, halfmove/fullmove clocks). Zobrist hashing for threefold repetition. Public API: `newGame()`, `loadFEN()`, `makeMove()` → `MoveResult`, `endGame()`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`, callback/batching. |
| `src/utils.h/.cpp` | `ChessUtils` namespace: FEN ↔ board conversion, piece color detection, material evaluation, and inline helper functions (`getPieceColor`, `isWhitePiece`, `isBlackPiece`, `isEnPassantMove`, `getEnPassantCapturedPawnRow`, `isCastlingMove`, `colorName`). |
| `src/codec.h/.cpp` | `ChessCodec` namespace: UCI move encoding/parsing, castling rights strings, compact 2-byte move encoding for binary storage. |
| `src/types.h` | Shared chess types: `PositionState` struct (castling rights, en passant target, halfmove/fullmove clocks — passed to `ChessRules` methods and owned by `ChessBoard`), `GameResult` enum, `MoveResult` struct (returned by `ChessBoard::makeMove()`), `GameHeader` packed struct (16 bytes), `GameModeCode` enum, storage constants (`FORMAT_VERSION`, `FEN_MARKER`, `MAX_GAMES`, `MAX_USAGE_PERCENT`), `invalidMoveResult()` factory. |
| `src/zobrist_keys.h` | Pre-computed Zobrist hash tables (~6.2KB flash) with PROGMEM guard for ESP32/native portability. |
| `src/recorder.h/.cpp` | `GameRecorder` class: pure recording orchestration. Encodes moves via `ChessCodec`, manages `GameHeader`, delegates persistence to `IGameStorage`. Replays games directly into a `ChessBoard` using batch mode. |
| `src/game_controller.h/.cpp` | `GameController` class: thin facade coordinating `ChessBoard` + `GameRecorder` + `IGameObserver`. Atomically applies board mutations, records moves, auto-finishes on game-end, and notifies the observer. |
| `src/logger.h` | `ILogger` abstract interface: `info()`, `error()`, and `infof()`/`errorf()` formatted helpers. |
| `src/game_observer.h` | `IGameObserver` abstract interface: `onBoardStateChanged(fen, evaluation)`. |
| `src/game_storage.h` | `IGameStorage` abstract interface: game file lifecycle (begin, append, finalize, discard), read-back for replay, and storage management. |

## Unit Tests (`test/`)

Native unit tests using the PlatformIO Unity framework. All engine tests are consolidated in a single `test_core/` suite.

```
test/
├── test_helpers.h                  Shared utilities (setupInitialBoard, clearBoard, placePiece, etc.)
└── test_core/
    ├── test_core.cpp             Main entry: setUp/tearDown, shared globals, register calls
    ├── test_rules_moves.cpp      Move generation per piece type, captures, initial position
    ├── test_rules_check.cpp      Check detection, checkmate, stalemate, move legality
    ├── test_rules_special.cpp    Castling, en passant, promotion, helper functions
    ├── test_utils.cpp            FEN round-trip, evaluation, piece color helpers, 50-move rule
    ├── test_codec.cpp            UCI move conversion, castling rights strings
    ├── test_board.cpp            ChessBoard lifecycle, moves, special moves, draws, FEN, callbacks
    └── test_recorder.cpp         GameRecorder + GameController: recording, replay, observer notification
```

Run with `pio test -e native`. See [PlatformIO Unit Testing docs](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html).

## Build Scripts (`src/web/build/`)

| File | Purpose |
|------|---------|
| `minify.py` | Pre-build: minifies HTML/CSS/JS from `src/web/` → `src/web/build/`. Skips gracefully if npm tools aren't installed. |
| `prepare_littlefs.py` | Pre-build: gzip-compresses web assets into `data/` for LittleFS. Respects the `.nogz.` convention. Cleans intermediate files after. |
| `upload_fs.py` | Build hook: hashes `data/` contents and only uploads the LittleFS image when assets change. |

## Filesystem (`data/`)

The `data/` directory contains pre-built, gzip-compressed web assets ready for LittleFS upload. This directory is committed to the repository so the project can be built and flashed without npm minification tools.

```
data/
├── index.html.gz
├── board.html.gz
├── game.html.gz
├── favicon.svg.gz
├── css/          *.css.gz
├── scripts/      *.js.gz
├── pieces/       *.svg.gz
└── sounds/       *.mp3 (raw, not gzipped)
```

`ESPAsyncWebServer` detects `.gz` files and serves them with `Content-Encoding: gzip` automatically. Sound files are served raw with `setTryGzipFirst(false)`.

## LittleFS Runtime Layout

At runtime, the firmware creates additional files on the LittleFS partition:

```
/games/
├── live.bin        Active game data (header + moves) — crash recovery
├── live_fen.bin    Active game FEN snapshots — crash recovery
├── 0001.bin        Completed game #1
├── 0001_fen.bin    FEN table for game #1
├── ...
```

Storage limits: maximum 50 saved games, capped at 80% of LittleFS capacity.

## Configuration

LibreChess has no editable configuration file. All settings are persisted in ESP32 NVS (non-volatile storage) and managed through the web UI or code constants:

| Setting | Storage | How to Change |
|---------|---------|---------------|
| WiFi networks (up to 3) | NVS `"wifiNets"` | Web UI WiFi Settings |
| Lichess API token | NVS `"wifiCreds"` | Web UI Lichess Settings |
| OTA password | NVS `"ota"` (salted SHA-256) | Web UI Security Settings |
| LED brightness | NVS `"boardSettings"` | Web UI Board Settings |
| Dark square dimming | NVS `"boardSettings"` | Web UI Board Settings |
| Calibration data | NVS `"calibration"` | Auto (first boot) or Web UI recalibrate button |
| GPIO pin assignments | `board_driver.h` `#define`s | Edit source code |
| Board, framework, libraries | `platformio.ini` | Edit file |
| Factory reset | `platformio.ini` build flag | Add `-DFACTORY_RESET` to `build_flags` |
