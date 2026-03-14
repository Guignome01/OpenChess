# LibreChess - Project Instructions

## Project Overview
ESP32 Arduino smart chessboard: detects piece movements via hall-effect sensors + shift register, provides LED feedback via WS2812B strip, and communicates with Stockfish API / Lichess API over WiFi. Built with PlatformIO (`esp32dev` board, Arduino framework).

## Architecture

### Class Hierarchy
**Core (`lib/core/`)**: `ChessGame` is the central game orchestrator composing `ChessBoard` (board representation + position logic), `ChessHistory` (in-memory move log + persistent game recording), and optionally `IGameObserver`. All chess-state mutations flow through `ChessGame`.

**Firmware (`src/`)**: `GameMode` (abstract base, `src/game_mode/`) → `PlayerMode` (human v human) | `BotMode` (concrete, composes `EngineProvider*`). `EngineProvider` (base class with FreeRTOS task lifecycle, `src/engine/`) → `StockfishProvider` (`src/engine/stockfish/`) / `LichessProvider` (`src/engine/lichess/`). `BotMode::update()` drives a non-blocking state machine (`BotState::PLAYER_TURN` / `BotState::ENGINE_THINKING`); engine requests are async via FreeRTOS tasks. `BoardDriver` is shared via pointer injection. Each `GameMode` holds a `ChessGame*` — no global chess state.

### Key Components
**Core library** (`lib/core/`): `ChessGame` (orchestrator, lifecycle owner), `ChessBoard` (position container with `BitboardSet` + `Piece mailbox[64]`, move execution, game-end detection), `ChessBitboard` (bitboard types, LERF square mapping, `BitboardSet` struct), `ChessAttacks` (precomputed leaper tables + classical ray functions), `ChessRules` (stateless static logic, bitboard-based attack detection), `ChessPiece` (type-safe piece representation, all constexpr), `ChessUtils` (board-level utility namespace), `ChessEval` (position evaluation: material + piece-square tables), `ChessIterator` (bitboard-based iteration: `forEachSquare`, `forEachPiece`, `somePiece`, `findPiece`), `ChessHash` (constexpr Zobrist key generation + incremental hashing), `ChessFEN` (FEN parse/serialize), `ChessNotation` (coordinate/SAN/LAN conversion), `ChessHistory` (move log + persistent recording), `types.h` (shared structs/enums), `IGameStorage`/`IGameObserver`/`ILogger` (DI interfaces). Full API details in `.github/instructions/core-library.instructions.md` (auto-loaded for `lib/core/`).

**Firmware** (`src/`): `BoardDriver` (LED + sensors + calibration), `WiFiManagerESP32` (web server + API + WiFi + NVS), `LittleFSStorage` (`IGameStorage` impl), `SerialLogger` (`ILogger` impl), `SystemUtils` (Arduino helpers), `SensorTest` (standalone sensor testing), `BoardMenu`/`MenuNavigator` (board-as-GUI).

### Coordinate System
Board arrays use `[row][col]` where **row 0 = rank 8** (black's back rank), **col 0 = file a**. Internally, bitboards use LERF (Little-Endian Rank-File): a1=0, h8=63. Conversion: `squareOf(row, col) = (7 - row) * 8 + col`.

## Build & Flash

### Commands
PlatformIO CLI (`pio`) is not on PATH by default. Use the full path:
- **Windows**: `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`
- **Linux**: `~/.platformio/penv/bin/pio`

| Action | CLI |
|--------|-----|
| Build | `pio run` |
| Upload | `pio run -t upload` |
| Serial Monitor | `pio device monitor` (115200 baud) |
| Factory reset | Add `-DFACTORY_RESET` to `build_flags` in `platformio.ini`, then flash |

### Build Pipeline
Three Python scripts run automatically (defined in `platformio.ini`): `minify.py` (HTML/CSS/JS minification), `prepare_littlefs.py` (gzip + place in `data/`), `upload_fs.py` (hash-based conditional filesystem upload). The `data/` directory is committed to git. Edit source files in `src/web/`, never in `data/`. Files named `*.nogz.*` skip gzip compression.

## Testing

Unit tests run natively on the host machine (no ESP32 required) using the PlatformIO Unity test framework and the `[env:native]` build environment.

### Running Tests

| Action | Command |
|--------|--------|
| Run all tests | `pio test -e native` |
| Run the suite | `pio test -e native -f test_core` |

For test architecture, file mirroring conventions, and per-file test group details, see `.github/instructions/testing.instructions.md` (auto-loaded when editing `test/` files).

## Code Style

C++ formatting: `.clang-format` (Google style base, no column limit). Run clang-format before committing.

## Engineering Principles

These principles are **non-negotiable** and apply to every code change — including refactors, new features, and internal restructuring. No change may violate them regardless of scope or complexity.

- **Separation of Concerns** — each class owns a single responsibility. Hardware in `BoardDriver`, chess rules in `ChessRules`, network in `WiFiManagerESP32`. Never mix concerns.
- **Loose Coupling** — pointer injection, no global state. Expose minimal public APIs, keep internals private.
- **Orchestrator via ChessGame** — firmware accesses chess logic exclusively through `ChessGame`. Never include `ChessBoard`, `ChessHistory`, `ChessRules`, or `ChessIterator` directly from firmware (`src/`). `ChessGame` re-exports iterator helpers (`forEachSquare`, `forEachPiece`, `somePiece`). Native tests may include internal `lib/core/` headers.
- **Dependency Minimization** — prefer bundled ESP-IDF/Arduino functionality (`mbedtls`, FreeRTOS) over external libraries.
- **DRY** — extract shared logic into helpers, base classes, or utilities. No duplication.
- **Reuse Before Creating** — check existing functions/patterns first. Build on existing infrastructure.
- **Cohesive Data Types** — when a fixed-size array and its count are always passed together, bundle them into a struct (e.g. `MoveList` for move generation output, `HashHistory` for Zobrist position history). Data and its length should travel as one unit, not as coupled out-params with raw index conventions.
- **Lookup Tables over Branching** — when mapping a small set of discrete inputs to outputs, prefer a `constexpr` array or lookup function over if/switch cascades. Simpler, faster, and less error-prone.
- **ESP32 Awareness** — `constexpr` and compile-time computation, minimize heap (fixed arrays, file-scoped statics), `enum class` over raw integers. Be mindful of stack sizes and watchdog timers.
- **Performance & Readability** — optimize for performance, but prioritize readability over cleverness.
- **Security by Default** — validate inputs at boundaries, never expose secrets in APIs, hash credentials, use TLS.
- **Code Quality** — validate at boundaries, trust internally. Prefer compile-time checks.
- **Well-Documented Code** — every function, struct, namespace, and non-trivial block must have clear documentation. Use section banners (`// ---------------------------------------------------------------------------`) to visually separate logical groups within a file. Document *why* a design choice was made, not just *what* the code does. Public APIs require doc comments explaining purpose, parameters, return values, and caller context. File-local helpers and anonymous-namespace types need comments explaining their role in the larger algorithm. When code flow is non-obvious (e.g. bitwise tricks, pin/check mask filtering), add inline comments explaining the reasoning.
- **Documentation & Tests** — every code change must update affected docs and tests in the same operation, never deferred. When changing `lib/core/`: update `core-library.instructions.md`, `architecture.md`, `project-structure.md`, `testing.instructions.md`, and unit tests as needed. See the Completion Checklist in `core-library.instructions.md` and `docs/development/additional-topics.md` for full sync triggers.