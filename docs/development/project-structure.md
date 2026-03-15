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
├── .github/                Copilot instructions, scoped instruction files, and workflow skills
├── LICENSE                 Project license
└── README.md               Project overview and quick start
```

## Firmware (`src/`)

### Core

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point: `setup()` and `loop()`. Game mode selection, menu routing, WiFi/resign/board-edit relay, and game lifecycle management. |
| `board_driver.h/.cpp` | Hardware abstraction: LED strip (NeoPixelBus, I2S DMA), sensor grid (shift register scan + GPIO reads), calibration (NVS-persisted), LED settings (brightness, dimming), and async animation queue (FreeRTOS task + queue). GPIO pin definitions. |
| `system_utils.h/.cpp` | Arduino/ESP32 utility functions: `colorLed()` (piece char → LED color), `ensureNvsInitialized()` (Preferences guard). Not available in native tests. |
| `led_colors.h` | `LedRGB` struct and named color constants (Cyan, White, Red, Green, Yellow, Purple, Orange, Blue, etc.) with `scaleColor()` brightness helper. |

### Game Modes

| File | Purpose |
|------|---------|
| `game_mode/game_mode.h/.cpp` | Abstract base class for all game modes. Holds a `ChessGame*` that orchestrates chess state, recording, and observer notification. Implements shared logic: `tryPlayerMove()`, `applyMove()` (delegates to `ChessGame::makeMove()`; the string overload parses coordinate notation), `waitForBoardSetup()`, `tryResumeGame()`, resign gesture handling, and LED feedback helpers. No longer includes `chess_rules.h` directly — all chess queries go through the game orchestrator. |
| `game_mode/bot_mode.h/.cpp` | Concrete class for human-vs-engine play (composition pattern). Composes an `EngineProvider*` via strategy injection. Non-blocking `update()` drives an async state machine (`BotState::PLAYER_TURN` / `BotState::ENGINE_THINKING`): player turn → `tryPlayerMove()` → `applyMove()` → `provider_->onPlayerMoveApplied()`; engine turn → `provider_->requestMove()` (spawns FreeRTOS task) → polls `provider_->checkResult()` each tick. Provides thinking animation helpers, remote-move LED guidance (`waitForRemoteMoveCompletion()`), and resign hooks that delegate to the provider. |
| `game_mode/player_mode.h/.cpp` | Human vs Human mode. Minimal subclass of `GameMode` — implements `begin()` (board setup, game recording) and `update()` (sensor polling, move processing). |
| `sensor_test.h/.cpp` | Standalone sensor diagnostic mode (does not inherit `GameMode`). Tracks visited squares, lights them white, completes when all 64 are visited. |

### Engine Providers

| File | Purpose |
|------|---------|
| `engine/engine_provider.h` | `EngineProvider` base class. Defines the contract for all chess engines: `initialize()`, `requestMove()` / `checkResult()` / `cancelRequest()` (async move computation), `onPlayerMoveApplied()`, `onResignConfirmed()`, `getEvaluation()`. Also owns the shared FreeRTOS task lifecycle (`BaseTaskContext`, `spawnTask()`, `pollResult()`, `peekResult()`, `finishTask()`). Defines `EngineResult` (move/game-end data) and `EngineInitResult` (player color, FEN, mode, depth, resume flag). |
| `engine/stockfish/stockfish_provider.h/.cpp` | `StockfishProvider`: extends `EngineProvider`, spawns a one-shot FreeRTOS task per move that calls `StockfishAPI`. `TaskContext` extends `BaseTaskContext` with FEN, depth, and evaluation fields. Retry logic with cancel checking between attempts. |
| `engine/stockfish/stockfish_api.h/.cpp` | Stockfish API client. Builds request URLs, parses JSON responses (evaluation, best move, continuation). Connects to `stockfish.online` over HTTPS. |
| `engine/stockfish/stockfish_settings.h` | 8 difficulty presets (beginner through master, depths 1–15, scaled timeouts 10s–65s). `StockfishSettings::fromLevel(int)` factory. |
| `engine/lichess/lichess_provider.h/.cpp` | `LichessProvider`: extends `EngineProvider`, blocking `initialize()` discovers active games (token verification + event polling). `requestMove()` spawns a FreeRTOS task that opens a persistent NDJSON stream and reads events; reconnects with exponential backoff on connection loss. `onPlayerMoveApplied()` sends moves to Lichess with retries. `onResignConfirmed()` resigns on the server. |
| `engine/lichess/lichess_api.h/.cpp` | Lichess API client. Token management, game event polling, persistent game stream (`connectGameStream()` / `readStreamEvent()`), move submission, and resignation. Connects to `lichess.org` over HTTPS. |
| `engine/lichess/lichess_config.h` | `LichessConfig` struct: holds the Lichess API token. |

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
| `src/chess_pieces.h/.cpp` | `ChessPiece` namespace: type-safe piece representation (Piece/Color/PieceType enums, bit extraction, construction, predicates, color helpers, FEN char conversion, Zobrist index, material values). `ChessPieces` namespace: pawn-structure mask infrastructure (`PASSED_PAWN_MASK`, `ISOLATED_PAWN_MASK`, `FORWARD_FILE_MASK`) with query helpers (`isPassed`, `isIsolated`, `isDoubled`, `isBackward`). |
| `src/chess_bitboard.h` | `ChessBitboard` namespace (header-only): bitboard types (`Bitboard = uint64_t`, `Square = int`), LERF square mapping (`squareOf`, `rowOf`, `colOf`), bit manipulation (`popcount`, `lsb`, `popLsb`), file/rank masks, directional shifts (compass rose convention), `BitboardSet` struct (12 piece + 2 color + occupancy bitboards with `setPiece`/`removePiece`/`movePiece`). |
| `src/chess_movegen.h/.cpp` | `ChessMovegen` namespace: precomputed leaper tables (`KNIGHT_ATTACKS[64]`, `KING_ATTACKS[64]`, `PAWN_ATTACKS[2][64]`, ~2.5 KiB), classical ray functions (`rookAttacks`, `bishopAttacks`, `queenAttacks`), x-ray functions (`xrayRookAttacks`, `xrayBishopAttacks`), ray geometry (`rayBetween`, `lineBB`), `AttackInfo` struct + `computeAttackInfo(bb)` (per-piece-type/color attack maps for evaluation/search), `initAttacks()` initializer. |
| `src/chess_rules.h/.cpp` | `ChessRules` class: stateless, all-static chess logic. Move generation (fills `MoveList&`), legal move filtering via copy-make on `BitboardSet`, check/checkmate/stalemate detection. All functions take `const BitboardSet& bb` + `const Piece mailbox[]`; `isSquareUnderAttack(bb, sq, defendingColor)` uses bitboard-only attack table lookups. Position-dependent state (castling rights, en passant target) is passed in via `const PositionState&`. |
| `src/chess_board.h/.cpp` | `ChessBoard` class: board representation and position-level chess logic — a pure position container with no lifecycle state. Owns `BitboardSet bb_` (12 piece bitboards + 2 color + occupancy), `Piece mailbox_[64]` (flat array for O(1) piece identity), current turn, and all position state via `PositionState` (castling rights, en passant, halfmove/fullmove clocks). Incremental Zobrist hash (`hash_`). King cache (`Square kingSquare_[2]`). Insufficient material detection (K vs K, K+B/N vs K, K+B vs K+B same-color). Public API: `newGame()`, `loadFEN()` → `bool` (validates FEN before applying; returns false on invalid input), `makeMove()` → `MoveResult`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`, `bitboards()`, `mailbox()`. Convenience wrappers: `getPossibleMoves()`, `isCheck()`, `inCheck()`, `isCheckmate()`, `isStalemate()`, `isInsufficientMaterial()`, `isDraw()`, `isFiftyMoves()`, `isAttacked()`, `findPiece()`, `checkEnPassant()`, `checkCastling()`, `moveNumber()`. Note: lifecycle state (`gameOver_`, `gameResult_`, `winnerColor_`) and lifecycle methods (`endGame()`, `isGameOver()`) live in `ChessGame`. Move history, observer notification, undo/redo, and batching also live in `ChessGame`. |
| `src/chess_utils.h/.cpp` | `ChessUtils` namespace: board-level helpers — board queries (`isValidSquare`), coordinate helpers (`squareName`, `fileChar`, `rankChar`, `fileIndex`, `rankIndex`), validation (`isValidPromotionChar`), castling rights formatting/parsing, special-move analysis structs (`checkEnPassant`, `checkCastling`, `updateCastlingRights`), `applyBoardTransform` (applies move to `BitboardSet` + `mailbox` + `hash`), `boardToText(mailbox)`, `gameResultName`. |
| `src/chess_evaluation.h/.cpp` | `ChessEval` namespace: tapered position evaluation — `evaluatePosition(bb)` computes separate midgame and endgame scores (material + phase-specific PSTs + pawn structure via `ChessPieces` masks), then interpolates based on game phase (non-pawn material: N=1, B=1, R=2, Q=4; max 24). Endgame PSTs for king (centralization) and pawn (advancement) differ from midgame; other pieces share PSTs. Returns score in centipawns. |
| `src/chess_iterator.h` | `ChessIterator` namespace (header-only): board iteration helpers built on bitboard serialization. `forEachSquare(mailbox, fn)` loops 0–63. `forEachPiece(bb, mailbox, fn)` iterates occupied squares via `popLsb`. `somePiece(bb, mailbox, fn)` early-exit variant. `findPiece(bb, piece, positions, max)` locates pieces via their bitboard. All callbacks receive `(row, col, piece)`. |
| `src/chess_fen.h/.cpp` | `ChessFEN` namespace: FEN string handling — `boardToFEN()` (mailbox → FEN string), `fenToBoard()` (FEN string → `BitboardSet` + `mailbox` + state), `validateFEN()` (format validation: rank structure, piece chars, turn, castling, en passant, clocks). |
| `src/chess_notation.h/.cpp` | `ChessNotation` namespace: move notation conversion — coordinate (`"e2e4"`), SAN (`"Nf3"`), LAN (`"Ng1-f3"`) output and parsing. All functions are pure (`const BitboardSet&` + `const Piece mailbox[]` passed in). |
| `src/types.h` | Shared chess types: `Piece`/`Color`/`PieceType` enums, `PositionState` struct (castling rights, en passant target, halfmove/fullmove clocks) with `initial()` static factory, `GameResult` enum class, `MoveResult` struct (returned by `ChessBoard::makeMove()`), `MoveEntry` struct (move log record), `Move` struct (3 bytes: from/to/flags with capture, EP, castling, promotion bits), `ScoredMove` struct (Move + int16_t score), `MoveList` struct (fixed-size `Move[218]` array + count, used by both per-piece and bulk move generation, with `targetRow`/`targetCol`/`target` adapter accessors for UI), `HashHistory` struct (Zobrist hash array + count, used by `ChessBoard`), `GameHeader` packed struct (16 bytes), `GameModeId` enum class, `MoveFormat` enum class (`COORDINATE`, `SAN`, `LAN`), storage constants (`FORMAT_VERSION`, `FEN_MARKER`, `MAX_GAMES`, `MAX_USAGE_PERCENT`), `invalidMoveResult()` factory. |
| `src/chess_hash.h` | `ChessHash` namespace (header-only): constexpr-generated Zobrist keys, piece-index mapping, and full-board hash computation (`computeHash(bb, mailbox, turn, state)`). `ChessBoard` uses incremental hashing via XOR deltas in `applyMoveToBoard()`; `computeHash()` is retained for debug verification. |
| `src/chess_history.h/.cpp` | `ChessHistory` class: in-memory game history and persistent game recording. Ordered move log (`MoveEntry` structs with full move metadata including previous position state), Zobrist hash tracking for threefold repetition detection, and game recording lifecycle (compact 2-byte move encoding via static `encodeMove()`/`decodeMove()`, manages `GameHeader`, delegates persistence to `IGameStorage`, flushes header every full turn to reduce flash wear, validates moves during replay, replays games directly into a `ChessBoard`). Fixed-size arrays (ESP32-friendly). Composed by `ChessGame`. |
| `src/chess_game.h/.cpp` | `ChessGame` class: central game orchestrator. Composes `ChessBoard`, `ChessHistory`, and optionally `IGameObserver`. Constructor: `(IGameStorage*, IGameObserver*, ILogger*)`. All chess-state mutations flow through this class. Handles threefold repetition detection (via Zobrist hashing), move history recording, persistent game recording (delegated to `ChessHistory`), observer notification, and batching. Exposes `bitboards()` and `mailbox()` accessors, iterator helpers (`forEachSquare`, `forEachPiece`, `somePiece`), and notation convenience methods (`makeMove(string)`, `toCoordinate()`, `parseCoordinate()`, `parseMove()`, `getHistory(format)`) so firmware never needs to include core headers directly. |
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
    ├── test_chess_rules_moves.cpp  Move generation per piece type, captures, initial position
    ├── test_chess_rules_check.cpp  Check detection, checkmate, stalemate, move legality
    ├── test_chess_rules_special.cpp Castling, en passant, promotion, helper functions
    ├── test_chess_utils.cpp        Evaluation (via ChessEval), piece color helpers, 50-move rule, castling rights strings
    ├── test_chess_fen.cpp          FEN round-trip, boardToFEN/fenToBoard, validateFEN (valid/invalid positions, fields)
    ├── test_chess_notation.cpp      Coordinate/SAN/LAN output and parsing, auto-format detection, roundtrip
    ├── test_chess_board.cpp        ChessBoard: moves, special moves, draws, FEN, API queries
    ├── test_chess_game.cpp         ChessGame: threefold repetition, draw detection, observer notification/batching, history
    ├── test_chess_history.cpp      ChessHistory: move log with undo/redo, branch-on-undo
    ├── test_chess_history_recording.cpp  ChessHistory recording: persistence lifecycle, header flush, replay, branch-truncation, compact encode/decode
    ├── test_chess_bitboard.cpp     ChessBitboard: square mapping roundtrip, bit ops, square-color masks, BitboardSet mutations
    ├── test_chess_movegen.cpp      ChessMovegen: leaper tables, slider rays, x-ray attacks, geometry rays, AttackInfo
    └── test_chess_pieces.cpp       ChessPiece + ChessPieces: piece types, bit extraction, FEN chars, material values, Zobrist index, pawn-structure masks
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
