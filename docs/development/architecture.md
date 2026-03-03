# Architecture

Deep technical documentation of LibreChess internals. This document covers how the system works under the hood — component design, data flows, concurrency, storage, networking, and security. It is the authoritative reference for anyone modifying or extending the firmware.

## Class Hierarchy

```
ChessGame (abstract base)
 ├─ ChessPlayer (human vs human)
 └─ ChessBot (abstract, Template Method pattern)
     ├─ ChessStockfish (human vs Stockfish API)
     └─ ChessLichess (online Lichess play)

SensorTest (standalone, does not inherit ChessGame)
```

`ChessGame` defines the shared game infrastructure and common logic: `tryPlayerMove()`, `applyMove()` (delegates to `ChessBoard::makeMove()`), `waitForBoardSetup()`, `tryResumeGame()`, resign gesture handling, and LED feedback helpers. Each `ChessGame` instance owns a `ChessBoard` (`gm_`) which owns the board array, current turn, game-over state, and all position state (castling rights, en passant target, clocks). All chess mutations flow through the `ChessBoard`; the firmware never modifies the board or turn directly. Each subclass overrides `begin()` and `update()` to implement mode-specific behavior.

`ChessBot` is an abstract intermediate class using the Template Method pattern. Its concrete `update()` defines the game loop skeleton: on the player's turn it calls `tryPlayerMove()` → `applyMove()` → `onPlayerMoveApplied()` hook; on the engine's turn it calls `requestEngineMove()`. Subclasses override only the engine-specific hooks: `requestEngineMove()` (pure virtual — compute/fetch the engine move), `onPlayerMoveApplied()` (react to player moves, e.g. send to server), and `getEngineEvaluation()`. `ChessBot` also provides shared infrastructure for all engine modes: thinking animation management (`startThinking()`/`stopThinking()`), remote move guidance (`waitForRemoteMoveCompletion()` — LED cues + sensor blocking for physically executing engine moves), and resign hooks (`isFlipped()`, `onBeforeResignConfirm()`, `onResignCancelled()`).

`ChessStockfish` overrides `requestEngineMove()` to call the Stockfish API, and `getEngineEvaluation()` to return the Stockfish evaluation score. `ChessLichess` overrides `requestEngineMove()` to poll the Lichess game stream for opponent moves, `onPlayerMoveApplied()` to send moves to the Lichess server, and `onResignConfirmed()` to also resign on the Lichess API.

`SensorTest` follows the same `begin()`/`update()`/`isComplete()` lifecycle but is not a `ChessGame` subclass — it doesn't need chess logic, FEN state, or move history.

### Dependency Injection

Components are wired through pointer injection at construction time. No global state or singletons:

```cpp
BoardDriver boardDriver;
SerialLogger logger;
LittleFSStorage storage(&logger);
GameRecorder recorder(&storage, &logger);
WiFiManagerESP32 wifiManager(&boardDriver, &storage);
GameController controller(&recorder, &wifiManager);
ChessStockfish stockfishGame(&boardDriver, &wifiManager, &controller, playerColor, stockfishSettings);
```

`GameController` owns the `ChessBoard` internally — there is no shared chess state. All game classes interact with chess state through the controller facade. `GameRecorder` is nullable inside the controller — `ChessLichess` uses a controller with a null recorder since Lichess games are recorded on the server, not locally.

## Coordinate System

Board arrays use `[row][col]` where **row 0 = rank 8** (black's back rank) and **col 0 = file a**. This convention is consistent across all internal logic: `board[0][0]` is square a8, `board[7][7]` is square h1.

The LED strip is wired in a serpentine (zigzag) pattern across the physical board, but the `BoardDriver` calibration system maps physical LED indices to logical `[row][col]` coordinates. The calibration mapping is stored in NVS and applied transparently — all code above `BoardDriver` works exclusively in logical coordinates.

## Component Details

### BoardDriver

Hardware abstraction layer. Owns three subsystems:

**LED strip** — a 64-LED WS2812B strip driven by `NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp32I2s0800KbpsMethod>`. The I2S peripheral with DMA offloads timing-critical signal generation to hardware, avoiding conflicts with WiFi interrupts and keeping the main loop responsive. The strip is connected to GPIO 32 (`LED_PIN`). Global brightness is adjustable (0–255, default 255), and dark squares are automatically dimmed by a configurable multiplier (default 70%, stored in NVS as `dimMultiplier`). The `currentColors[8][8]` array tracks the current color of every square so dim multiplier changes can be applied retroactively.

**Sensor grid** — 64 A3144 hall-effect sensors arranged in an 8×8 matrix, read through column-scanning multiplexing. A 74HC595 shift register activates one column at a time (via transistor switches), and 8 row GPIOs are read simultaneously. This uses only 11 GPIO pins (3 shift register control + 8 row inputs) to scan all 64 sensors. Sensor state is triple-buffered: `sensorRaw[8][8]` (latest physical read), `sensorState[8][8]` (debounced current state), and `sensorPrev[8][8]` (snapshot for change detection). The `lastEnabledCol` field enables efficient sequential column shifting — instead of clocking through all 8 bits each time, the driver detects sequential column advances and shifts by one bit.

GPIO pin definitions are `#define`d at the top of `board_driver.h`:
- Shift register: `SR_CLK_PIN` (14), `SR_LATCH_PIN` (26), `SR_SER_DATA_PIN` (33)
- Row inputs: `ROW_PIN_0` through `ROW_PIN_7` (GPIOs 4, 16, 17, 18, 19, 21, 22, 23)
- LED data: `LED_PIN` (32)
- `SR_INVERT_OUTPUTS`: set to 1 if using PNP transistors instead of NPN

The physical order of pin connections **does not matter** — the calibration process maps physical pins to logical board coordinates.

**Calibration** — an interactive serial-guided process that runs on first boot (or when triggered via the web UI). It maps physical sensor/LED positions to logical `[row][col]` coordinates by asking the user to place pieces in specific patterns. The resulting mapping tables (`toLogicalRow[]`, `toLogicalCol[]`, `ledIndexMap[8][8]`, `swapAxes`) are persisted in NVS namespace `"calibration"`. The `swapAxes` flag handles boards where the shift register and row pins are wired to the opposite physical axis. Until calibration completes, the board repeats the calibration prompt on every boot (with a `skip` option that defers but doesn't persist).

**Sensor polling parameters**: `SENSOR_READ_DELAY_MS` = 40ms (polling interval), `DEBOUNCE_MS` = 125ms (state change debounce window). A piece must be present (or absent) for the full debounce duration before the change is registered, preventing false triggers from sliding pieces or magnetic interference. Always call `boardDriver.readSensors()` before reading state — the state arrays are only updated on explicit read calls.

### ChessRules

Lives in `lib/core/` — a standalone PlatformIO library with zero Arduino dependencies. This makes it natively compilable for host-based unit testing while still being auto-discovered by the ESP32 build via PlatformIO's Library Dependency Finder.

Pure, **stateless** chess logic. All methods are `static` — `ChessRules` has no member variables, no constructor, and no instance state. Position-dependent context (castling rights, en passant target) is passed in via `const PositionState&` (defined in `types.h`). Implements:

- **Move generation** — `getPossibleMoves(board, row, col, flags, ...)` returns all legal moves for a piece at a given position. Internally generates pseudo-legal moves per piece type, then filters out any move that would leave the player's king in check (`wouldMoveLeaveKingInCheck()`).
- **Castling** — the `flags.castlingRights` bitmask (bit 0 = White kingside, bit 1 = White queenside, bit 2 = Black kingside, bit 3 = Black queenside) is passed in by the caller. `addCastlingMoves()` checks rights, empty intermediate squares, and that the king doesn't pass through or land on an attacked square.
- **En passant** — the target square (`flags.epRow`, `flags.epCol`) is passed in by the caller. `hasLegalEnPassantCapture()` checks whether an adjacent pawn can legally execute the capture (used by `ChessBoard` for Zobrist hashing).
- **Game state checks** — `isKingInCheck(board, color)`, `isCheckmate(board, color, flags)`, `isStalemate(board, color, flags)`, `hasAnyLegalMove(board, color, flags)`, `isPawnPromotion(piece, row)`, `isSquareUnderAttack(board, row, col, defendingColor)`, `findKingPosition(board, color, row, col)`.

`ChessBoard` owns all position state (castling rights, en passant target, halfmove/fullmove clocks) via a `PositionState` struct and passes it to `ChessRules` methods as needed.

### ChessBoard

Also in `lib/core/` (`board.h/cpp`). Complete chess game-state manager — the "chess.js" of the project. Owns the board array, current turn, game-over state, and all position state via a `PositionState` struct (castling rights, en passant target, halfmove clock, fullmove clock). Public API: `newGame()`, `loadFEN()`, `makeMove()` → `MoveResult`, `endGame()`, `getFen()`, `getEvaluation()`, `getCastlingRights()`, `positionState()`, callback/batching. Calls `ChessRules` static methods for rule evaluation, passing `state_` where position-dependent context is needed.

- **FEN / evaluation cache** — `getFen()` and `getEvaluation()` use dirty-flag caching (`mutable` fields) to avoid recomputing on repeated calls. The cache is invalidated automatically on every mutation (`newGame()`, `loadFEN()`, `makeMove()`, `endGame()`).
- **Position tracking** — Zobrist hashing with pre-computed random tables (~6.2KB flash, defined in `zobrist_keys.h`). Each piece-square combination has a unique 64-bit hash. `positionHistory_[MAX_POSITION_HISTORY]` (128 entries, fixed array) stores hashes, cleared on irreversible moves (pawn moves, captures) for memory efficiency. `isThreefoldRepetition()` scans the history for 3 occurrences of the current hash. En passant is only hashed when a legal capture exists (verified via `ChessRules::hasLegalEnPassantCapture()`). Positions are recorded automatically after each move and on `newGame()`/`loadFEN()`.

### ChessUtils & SystemUtils

`ChessUtils` (in `lib/core/`, `utils.h/cpp`) is a namespace providing helper functions for chess logic: FEN ↔ board conversion, piece color detection, material evaluation, and inline utility functions (`getPieceColor`, `isWhitePiece`, `isBlackPiece`, `isEnPassantMove`, `isCastlingMove`, `colorName`). `ChessCodec` (in `lib/core/`, `codec.h/cpp`) is a companion namespace providing encoding/decoding helpers: UCI move strings, castling rights string formatting/parsing, and compact 2-byte move encoding for binary storage. All functions use `std::string` (not Arduino `String`). Internal APIs (`updateBoardState`, `addFen`, `setBoardStateFromFEN`) accept `std::string` directly; Arduino `String` conversion happens only at the hardware/network boundary (e.g., HTTP responses, LittleFS reads).

`SystemUtils` (in `src/`) contains the Arduino/ESP32-dependent functions that were separated from the core library: `colorLed()` (piece char → LED color), `printBoard()` (Serial debug output), and `ensureNvsInitialized()` (Arduino Preferences guard). These are not available in native tests.

### WiFiManagerESP32

Manages WiFi connectivity, the web server, and all HTTP API endpoints. Key subsystems:

**WiFi state machine** — event-driven `WiFiState` enum (`AP_ONLY`, `CONNECTING`, `CONNECTED`, `RECONNECTING`) managed via `WiFi.onEvent()` callbacks. Uses a singleton `instance` pointer for the static event callback to access instance state.

- **AP lifecycle**: The access point (`LibreChess`, password `chess123`, IP `192.168.4.1`) starts immediately on boot. After a stable STA connection is maintained for `AP_STABILIZATION_MS` (10 seconds), a FreeRTOS timer callback (`apStabilizationCallback`) disables the AP. If the STA connection drops, the AP re-enables immediately. This stabilization window prevents flapping when WiFi is intermittent.
- **Reconnection**: On STA disconnect, the state transitions to `RECONNECTING`. The firmware cycles through all saved networks with exponential backoff (starting at `RECONNECT_INITIAL_MS` = 5 seconds, capped at `RECONNECT_MAX_MS` = 60 seconds). `reconnectNetworkIndex` tracks which network to try next.
- **mDNS**: hostname `librechess` (defined as `MDNS_HOSTNAME`), started in `begin()` and restarted in `handleWiFiConnected()` to rebind to the STA interface. Enables `http://librechess.local` access.

**Known-networks registry** — up to `MAX_SAVED_NETWORKS` (3) WiFi networks stored in NVS namespace `"wifiNets"` (keys: `"count"`, `"ssid0"`/`"pass0"` through `"ssid2"`/`"pass2"`). On boot, networks are loaded and tried in order.

**Web server** — `AsyncWebServer` on port 80. Serves gzipped static files from LittleFS via `serveStatic`. API endpoints handle JSON requests for board state, game selection, settings, WiFi management, Lichess token, OTA updates, game history, board editing, and resign. All configuration getters and setters are exposed as `public` methods for the main loop to relay state between the web layer and game logic (e.g., `getSelectedGameMode()`, `getPendingBoardEdit()`, `getPendingResign()`).

**Board state relay** — `WiFiManagerESP32` implements `IGameObserver`. `GameController` calls `onBoardStateChanged(fen, evaluation)` automatically after every board mutation. The web UI polls `GET /board/update` which returns the current FEN, evaluation, move list, and game state as JSON.

**Board editing** — `handleBoardEditSuccess()` stores a pending FEN string from the web UI's board editor. The main loop checks `getPendingBoardEdit()` each cycle and applies it to the active game via `setBoardStateFromFEN()`, then calls `clearPendingEdit()`.

**Web resign** — the web UI's resign button sends `POST /resign`. The handler sets `hasPendingResign = true`. The main `loop()` relays this to the active game via `setResignPending(true)`, then clears the web flag. The game's `processResign()` picks it up on the next `update()` call.

### GameRecorder / GameController / LittleFSStorage

Game recording and crash recovery follow a layered architecture with clean separation:

- **`GameRecorder`** (`lib/core/`) — pure recording orchestration. Encodes moves via `ChessCodec`, manages the `GameHeader`, delegates persistence to `IGameStorage`. Flushes the header to storage every full turn (2 half-moves) to reduce flash wear; FEN snapshots always trigger an immediate flush. Validates moves during replay — rejects corrupted recordings with invalid moves. Replays games directly into a `ChessBoard` using batch mode — no game-layer involvement, no `replaying` flag.
- **`GameController`** (`lib/core/`) — thin facade coordinating `ChessBoard` + `GameRecorder` + `IGameObserver`. Each mutation (move, load FEN, end game) atomically updates the board, records, and notifies. Provides `startNewGame(mode, ...)` for atomic board-reset + recording-start. `endGame()` is guarded against double-calls.
- **`LittleFSStorage`** (`src/`) — concrete `IGameStorage` backed by LittleFS.

**Binary format** — each game consists of two files:
- `<id>.bin` (or `live.bin`) — 16-byte packed `GameHeader` followed by 2-byte UCI-encoded move entries
- `<id>_fen.bin` (or `live_fen.bin`) — FEN snapshot table for efficient position reconstruction

The `GameHeader` struct (exactly 16 bytes, `__attribute__((packed))`) contains:
| Field | Type | Description |
|-------|------|-------------|
| `version` | `uint8_t` | Format version (currently 1) |
| `mode` | `GameModeCode` | Game mode (1 = ChessMoves, 2 = Bot, 3 = Lichess) |
| `result` | `GameResult` | Game outcome enum (0 = in-progress, 1 = checkmate, 2 = stalemate, 3 = draw_50, 4 = draw_3fold, 5 = resignation, 6 = draw_insufficient, 7 = draw_agreement, 8 = timeout, 9 = aborted) |
| `winnerColor` | `uint8_t` | `'w'`, `'b'`, `'d'` (draw), or `'?'` (in-progress) |
| `playerColor` | `uint8_t` | Human's color for bot mode, `'?'` for ChessMoves |
| `botDepth` | `uint8_t` | Stockfish depth for bot mode, 0 for ChessMoves |
| `moveCount` | `uint16_t` | Number of 2-byte entries (including FEN markers) |
| `fenEntryCnt` | `uint16_t` | Number of FEN table entries |
| `lastFenOffset` | `uint16_t` | Byte offset of last FEN entry within the FEN table |
| `timestamp` | `uint32_t` | Unix epoch from NTP (0 if unavailable) |

**Move encoding** — each move is packed into 2 bytes: `[from_square(6 bits)][to_square(6 bits)][promotion(4 bits)]`. Square index = `row * 8 + col`. Promotion codes: 0 = none, 1 = queen, 2 = rook, 3 = bishop, 4 = knight. The special marker `0xFFFF` (`FEN_MARKER`) indicates that a FEN snapshot was recorded at this point in the move sequence.

**FEN snapshots** — periodic FEN strings are appended to the FEN table file. During replay, the system finds the last FEN snapshot, restores the board to that position, and replays only the moves that follow. This bounds replay time regardless of game length.

**Crash recovery** — during gameplay, moves are appended to `live.bin` and FEN snapshots to `live_fen.bin` in real time. The header is flushed every full turn (2 half-moves) rather than every move, reducing flash write cycles by half. On boot, `hasActiveGame()` checks if these files exist. If so, `getActiveGameInfo()` reads the header to determine the mode and configuration, and `GameRecorder::replayInto()` restores the full game state directly into the `ChessBoard` via batch mode. Move data size is derived from the file size (not the header's `moveCount`) for robustness against mid-turn crashes. Each move is validated during replay — corrupted or invalid moves cause the replay to abort.

**Storage limits** — `MAX_GAMES` = 50 games, `MAX_USAGE_PERCENT` = 80% of LittleFS capacity. `enforceStorageLimits()` is called after each game finishes and deletes the oldest games (lowest ID) until both limits are satisfied.

**Game list API** — `getGameListJSON()` returns a JSON array of all completed games with metadata (id, mode, result, winner, move count, timestamp, bot config). Used by the web UI's game history panel.

### ChessRules Interaction

The `ChessGame` base class coordinates between `BoardDriver` (hardware) and `GameController` (chess state + recording + notification):

1. `tryPlayerMove()` — polls sensors for a piece lift, shows valid moves via `ChessRules::getPossibleMoves(board, row, col, controller_->positionState(), ...)`, waits for placement, validates the move, then calls `applyMove()`.
2. `applyMove()` — calls `controller_->makeMove()` which atomically updates the board, records the move, and notifies the observer. Returns a `MoveResult` struct with all move metadata. The firmware then uses the `MoveResult` to drive LED feedback, sounds, remote-move guidance, and game-end animations.
3. Turn advancement, castling rights, and game-end detection are all handled internally by `ChessBoard::makeMove()` — the firmware never modifies the board or turn directly.

## Game Mode Lifecycle

### Boot Sequence

1. `Serial.begin(115200)`, NVS initialization
2. (Optional) Factory reset if `-DFACTORY_RESET` build flag is set
3. `LittleFS.begin()` — mount filesystem
4. `storage.initialize()` — create `/games/` directory if needed
5. `boardDriver.begin()` — initialize LED strip, GPIO pins, calibration (may block for interactive serial calibration on first boot), and start the animation FreeRTOS task
6. `wifiManager.begin()` — start AP, load saved networks, begin STA connection attempts, start web server, configure mDNS
7. `initMenus(&boardDriver)` — two-phase menu initialization (set `BoardDriver*` on all menus, configure items and back buttons)
8. `configTime(0, 0, "pool.ntp.org", "time.nist.gov")` — non-blocking NTP sync
9. `checkForResumableGame()` — if a live game exists on flash, show a confirm dialog and optionally resume
10. If not resuming, `enterGameSelection()` — push the root menu onto the navigator stack

### Main Loop

Every iteration of `loop()`:

1. `wifiManager.update()` — handle WiFi reconnection state machine
2. Check for pending board edits from the web UI (`getPendingBoardEdit()`)
3. Check for web-based game mode selection (`getSelectedGameMode()`)
4. If in `MODE_SELECTION`: poll the menu navigator, handle results via `handleMenuResult()`
5. If in a game mode and not yet initialized: call `initializeSelectedMode()` (creates the game object, calls `begin()`)
6. If in a game mode: relay web resign flag, check `isGameOver()`, call `update()`
7. `delay(SENSOR_READ_DELAY_MS)` — 40ms pause for sensor polling cadence

### Mode Initialization

`initializeSelectedMode()` performs cleanup and setup:

1. If not resuming, discard any leftover live game file
2. `delete` the previous `activeGame` and `sensorTest` objects
3. Create the new game object via `new` (the only heap allocation for game modes)
4. Call `begin()` — which typically calls `waitForBoardSetup()` to wait for correct piece placement, then `controller_->startRecording()` to begin recording

For game resume: `begin()` detects the `resumingGame` flag, skips piece setup, calls `controller_->resumeGame()` which delegates to `GameRecorder::replayInto()` to restore the full game state directly into the `ChessBoard` via batch mode, then continues with normal `update()` calls.

### Menu Navigation

The `MenuNavigator` manages a stack of `BoardMenu` instances (max depth 4):

- **Game selection** (root) → 4 center squares: Blue (ChessMoves), Green (Bot), Yellow (Lichess), Red (SensorTest)
- **Bot difficulty** (pushed on Bot selection) → 8 squares across row 3, colors green→blue, depths 3→17
- **Bot color** (pushed on difficulty selection) → 3 squares: White, DimWhite (play as Black), Yellow (random)

Menu IDs use distinct ranges per level (0–9 root, 10–19 difficulty, 20–29 color) so `handleMenuResult()` can route by ID value alone — no callbacks or virtual dispatch.

The web UI can also trigger game selection via `POST /game/select`, setting `gameMode` and `botConfig` on `WiFiManagerESP32`. The main loop detects this, bypasses the physical menu, and proceeds directly to mode initialization.

## Resign System

### Physical Resign Gesture

The gesture runs inline inside `tryPlayerMove()` — no separate state machine. The flow:

1. Player lifts their king on their own turn. `tryPlayerMove()` detects the lift and starts a timer.
2. If the king stays off the board for `RESIGN_HOLD_MS` (3000ms), the resign sequence begins. The origin square shows orange at 25% brightness via `showResignProgress(row, col, 0)`.
3. Player returns the king. Orange increases to 50% (`showResignProgress(row, col, 1, clearFirst=true)`). All other LEDs are cleared.
4. `continueResignGesture()` takes over — a blocking loop that waits for 2 more quick lift-and-return cycles, each within `RESIGN_LIFT_WINDOW_MS` (1000ms). Orange progresses to 75% then 100%.
5. If all lifts complete in time, `boardConfirm()` shows a yes/no dialog (green/red squares).
6. On confirm, `handleResign(resignColor)` is called. The base implementation ends the game with `RESULT_RESIGNATION` and plays a firework animation in the opponent's color. `ChessLichess` overrides this to also call `LichessAPI::resignGame()` and manages the thinking animation stop flag.

If any step times out (king not returned within the window), the gesture is silently canceled — no error feedback, just a return to normal play. The progressive orange brightness (25% → 50% → 75% → 100%) uses `LedColors::scaleColor(LedColors::Orange, factor)` with factors from `RESIGN_BRIGHTNESS_LEVELS`.

LED helper functions encapsulate the mutex pattern:
- `showResignProgress(row, col, level, clearFirst)` — acquires `LedGuard`, optionally clears all LEDs, sets the square color, shows
- `clearResignFeedback(row, col)` — acquires `LedGuard`, turns off the square
- `showIllegalMoveFeedback(row, col)` — queues a red blink for illegal moves

### Web Resign

1. Web UI: ⚑ button → JS `confirm()` → `POST /resign` via `Api.resign()`
2. `WiFiManagerESP32`: sets `hasPendingResign = true`
3. `main.cpp loop()`: relays `wifiManager.getPendingResign()` → `activeGame->setResignPending(true)`, clears web flag
4. Game `update()` → `processResign()` checks `resignPending` flag → calls `handleResign(currentTurn)`

### Turn Restriction

Resign is only processed on the current player's turn in all modes, matching real chess conventions.

## LED System

### Animation Queue

Animations run on a dedicated FreeRTOS task (`animationWorkerTask`) with its own queue (`animationQueue`, type `QueueHandle_t`). The task runs in an infinite loop: dequeue an `AnimationJob`, acquire the LED mutex, execute the animation, release the mutex, and signal the done semaphore if applicable.

`AnimationJob` is a struct with a `type` field (`AnimationType` enum: `CAPTURE`, `PROMOTION`, `BLINK`, `WAITING`, `THINKING`, `FIREWORK`, `FLASH`, `SYNC`) and a `params` union containing type-specific data. The `SYNC` type is a no-op barrier — `waitForAnimationQueueDrain()` enqueues a SYNC job and blocks on the `animationDoneSemaphore` until the worker reaches it.

**Short animations** (capture, promotion, blink, firework, flash) — fire-and-forget. The caller enqueues the job and returns immediately. The animation task dequeues and executes it.

**Long-running animations** (thinking, waiting) — return an `std::atomic<bool>*` stop flag (heap-allocated). The animation task checks the flag on each frame. The caller owns the flag and must use `stopAndWaitForAnimation(flag)` to:
1. Set the flag to `true`
2. Block on `animationDoneSemaphore` until the worker finishes the current frame and releases the LED mutex
3. `delete` the flag and null the pointer

Never set the flag directly or `delete` it without waiting — the animation task may still be mid-frame with the LED mutex held.

### LedGuard (RAII Mutex)

The LED strip is a shared resource between the main loop and the animation task, guarded by `ledMutex` (FreeRTOS semaphore). `BoardDriver::LedGuard` is an RAII wrapper:

```cpp
{
    BoardDriver::LedGuard guard(boardDriver);
    boardDriver.clearAllLEDs();
    boardDriver.setSquareLED(row, col, LedColors::Cyan);
    boardDriver.showLEDs();
}
```

Single animation calls (`blinkSquare`, `captureAnimation`, etc.) are queued and acquire the mutex inside the animation task — no guard needed by the caller.

**Critical rule**: before writing LEDs directly from the main loop, call `waitForAnimationQueueDrain()` to ensure all queued animations have completed. Otherwise a stale queued animation can execute after your writes and overwrite them.

### Color Semantics

Colors in the `LedColors` namespace (`led_colors.h`) have fixed meanings:

| Color | RGB | Meaning |
|-------|-----|---------|
| Cyan | (0, 255, 255) | Piece origin — "pick up from here" |
| White | (255, 255, 255) | Valid move destination, menu back button |
| DimWhite | (40, 40, 40) | "Play as Black" option in bot color menu |
| Red | (255, 0, 0) | Capture square, illegal move, error |
| Green | (0, 255, 0) | Move confirmed, "yes" in confirm dialogs |
| Yellow | (255, 200, 0) | King in check, pawn promotion, random option |
| Purple | (128, 0, 255) | En passant captured pawn square |
| Orange | (255, 80, 0) | Resign gesture progress |
| Blue | (0, 0, 255) | Bot thinking, Human vs Human mode indicator |
| Lime | (100, 200, 0) | Easy difficulty level |
| Crimson | (200, 0, 50) | Hard difficulty level |
| Off | (0, 0, 0) | LED off |

`scaleColor(color, factor)` is an `inline constexpr` helper that multiplies RGB components by a float factor (0.0–1.0), clamped to 255. Used for brightness progression effects like the resign gesture.

### Animation Types

| Type | Duration | Description |
|------|----------|-------------|
| Capture | ~1s | Concentric wave rings from capture square. Red/yellow alternating with quadratic intensity falloff. |
| Promotion | ~1.6s | Yellow waterfall cascading down the promotion column. |
| Blink | Configurable | Square blinks in a given color N times. Used for check warnings (yellow, 3x), move confirmation (green, 1x), illegal move (red, 2x). |
| Firework | ~2.4s | Ring of light contracts from board edges to center, then expands back. Color matches the winner or event type. |
| Flash | Configurable | Entire board flashes a color N times. Used for critical errors (red, 3x). |
| Thinking | Continuous | Four corner squares pulse blue with sinusoidal breathing (8%–100% brightness). Slight purple hue shift at low brightness. |
| Waiting | Continuous | White chase animation traces 28 perimeter squares clockwise. Two groups of 8 LEDs travel diametrically opposite. |
| Connecting | One-shot | Two center rows fill with blue from left to right, column by column. |

## Menu System

### BoardMenu

A reusable menu primitive for the 8×8 LED grid. State is stack-allocated — no heap usage.

**Item definition** — `MenuItem` struct: `{row, col, color, id}`. Coordinates are authored in white-side orientation (row 7 = rank 1). Arrays are `constexpr` file-scoped statics in `menu_config.h`, stored in flash with zero RAM cost. The menu does not copy the array — the pointer must outlive the menu.

**Two-phase debounce** — prevents pieces already on the board from triggering selections when a menu appears:
1. Phase 1 (empty): the square must read empty for `DEBOUNCE_CYCLES` (5) consecutive sensor polls (~200ms)
2. Phase 2 (occupied): the square must then read occupied for another 5 consecutive polls
Only a deliberate "place a piece on an empty square" action registers.

After confirmed selection, the square blinks once in its own color (`blinkSquare()`) for visual feedback, then the system waits for the piece to be removed before returning — preventing input bleed into the next menu or game.

**Back button** — set via `setBackButton(row, col)`, lit in `LedColors::White`. Omit for root menus. The navigator auto-pops on back and re-shows the parent.

**Orientation** — `setFlipped(true)` mirrors row coordinates (`row' = 7 - row`) so menus face a player on the black side. Applied to bot games where the player chose black, and to the resign confirm dialog on black's turn.

**`boardConfirm()`** — standalone yes/no dialog (green at d4, red at e4). Blocking. Returns `bool`. Supports orientation flipping.

### MenuNavigator

Stack-based orchestrator with max depth 4 (`std::array<BoardMenu*, MAX_DEPTH>`). Push/pop navigation with automatic back-button handling and parent re-display with fresh debounce state. `clear()` hides the current menu and empties the stack (used when an external event like WiFi game selection overrides the menu).

### Menu Configuration

All menu layout data lives in `menu_config.h/cpp`:
- `MenuId` namespace: distinct ID ranges per menu level (0–9 root, 10–19 difficulty, 20–29 color)
- `constexpr MenuItem[]` arrays: `gameMenuItems`, `botDifficultyItems`, `botColorItems`
- `extern` instances: `gameMenu`, `botDifficultyMenu`, `botColorMenu`, `navigator`
- `initMenus(BoardDriver* bd)`: two-phase initializer — sets `BoardDriver*`, configures items, sets back buttons. Called once in `setup()`.

## External API Integration

### Stockfish

`StockfishAPI` (in `stockfish_api.h/cpp`) handles HTTPS requests to `stockfish.online` using `WiFiClientSecure`:
- Builds request URLs with FEN and depth parameters
- Parses JSON responses for best move, evaluation, and continuation line
- Connection uses TLS with `setInsecure()` (no certificate pinning)

`StockfishSettings` (in `stockfish_settings.h`) defines 8 difficulty presets:

| Level | Name | Depth | Timeout |
|-------|------|-------|---------|
| 1 | Beginner | 3 | 10s |
| 2 | Easy | 5 | 15s |
| 3 | Intermediate | 7 | 20s |
| 4 | Medium | 9 | 25s |
| 5 | Advanced | 11 | 35s |
| 6 | Hard | 13 | 45s |
| 7 | Expert | 15 | 55s |
| 8 | Master | 17 | 65s |

`StockfishSettings::fromLevel(int)` is a factory that selects by 1-based level. `BotConfig` bundles `StockfishSettings` + `playerIsWhite` flag and is passed to `ChessBot` at construction.

### Lichess

`LichessAPI` (in `lichess_api.h/cpp`) handles HTTPS requests to `lichess.org`:
- **Game event polling** — checks for active or incoming games
- **Game stream polling** — retrieves the current game state (moves, status, clocks)
- **Move submission** — sends a UCI move to the active game
- **Resignation** — submits a resign request

The Lichess token is stored in NVS (namespace `"lichess"`, key `"token"`). The web UI's Lichess settings page allows entering or clearing the token. API responses to the web UI return only a masked version of the token (first 4 characters + asterisks).

`ChessLichess` polls in `update()` every `POLL_INTERVAL_MS` (500ms). Game state sync happens in `syncBoardWithLichess()`, which compares the server's move list against `lastKnownMoves` and applies any new remote moves. `lastSentMove` prevents the player's own move from being processed as a remote move on the next poll.

## Storage

### LittleFS

LittleFS partitions the ESP32's flash for file storage. Used for:

**Web assets** — gzip-compressed HTML, CSS, JS, images, and sounds placed in `data/` during the build pipeline. The web server serves them with `Content-Encoding: gzip` headers via `serveStatic`. Files matching `*.nogz.*` skip compression (used for binary assets like MP3s).

**Game history** — binary files under `/games/`:

| File | Purpose |
|------|---------|
| `/games/live.bin` | Current in-progress game (header + moves) |
| `/games/live_fen.bin` | FEN snapshots for the current game |
| `/games/<id>.bin` | Completed game (header + moves) |
| `/games/<id>_fen.bin` | FEN snapshots for completed game |

Storage limits: max 50 games, 80% of LittleFS capacity. `enforceStorageLimits()` deletes oldest games (lowest ID) when limits are reached.

### NVS (Non-Volatile Storage)

NVS stores settings that survive firmware updates and power cycles. Organized by namespace:

| Namespace | Keys | Purpose |
|-----------|------|---------|
| `ledSettings` | `brightness`, `dimMult` | LED brightness (0–255) and dark square dimming (20–100%) |
| `boardCal` | `ver`, `rowPins`, `srPins`, `row`, `col`, `led`, `swap` | Calibration version, pin config verification, logical mapping arrays, axis swap flag |
| `wifiNets` | `count`, `ssid0`–`ssid2`, `pass0`–`pass2` | Up to 3 saved WiFi networks |
| `lichess` | `token` | Lichess API token |
| `ota` | `passHash`, `salt` | OTA password (salted SHA-256 hash) |

All NVS access uses Arduino's `Preferences` library. `SystemUtils::ensureNvsInitialized()` must be called before any NVS operation — it initializes the NVS partition if needed.

### NTP Time Sync

`configTime(0, 0, "pool.ntp.org", "time.nist.gov")` is called once in `setup()`. The call is non-blocking — NTP resolves in the background over WiFi. `LittleFSStorage::getTimestamp()` returns the current Unix epoch, or 0 if NTP hasn't synced yet. Timestamps are stored in game headers for the web UI's game history display.

## Security

### WiFi Access Point Lifecycle

The AP follows strict lifecycle rules to ensure the board is always accessible for configuration:

1. **Boot**: AP starts immediately (SSID `LibreChess`, password `chess123`, IP `192.168.4.1`)
2. **STA connection established**: a 10-second stabilization timer starts
3. **Timer expires with stable STA**: AP shuts down (callback `apStabilizationCallback` calls `disableAP()`)
4. **STA disconnect**: AP re-enables immediately via `enableAP()` in the WiFi event handler
5. **No saved networks**: AP remains permanently active

The stabilization window prevents the AP from flapping on/off with unstable WiFi connections. The timer is a FreeRTOS software timer (`apStabilizationTimer`), canceled if the STA disconnects before it fires.

### OTA Password Protection

Firmware uploads via the web UI can be protected with an optional password:

- Password is stored as a **salted SHA-256 hash** in NVS namespace `"ota"` (never plaintext)
- Salt: 16 random bytes generated via `esp_random()`, stored as hex string
- Hashing: `mbedtls_sha256` (bundled with ESP-IDF, no external crypto dependencies)
- The web UI sends the password in an `X-OTA-Password` HTTP header
- `verifyOtaPassword()` re-hashes the provided password with the stored salt and compares
- Firmware validation: the upload handler checks for the ESP32 magic byte (`0xE9`) at offset 0 before accepting
- Management: `POST /ota/password` to set/change/remove, `GET /ota/status` to query if a password is set

The web UI separates password management (Security section on settings page) from firmware upload (OTA Update section).

### TLS for External Connections

Outbound HTTPS connections to Lichess and Stockfish use `WiFiClientSecure` with `setInsecure()`. TLS encryption is enabled, but certificate pinning is not performed. This trades certificate validation for reliability across different ESP32 SDK versions and certificate store limitations.

### Factory Reset

Adding `-DFACTORY_RESET` to `build_flags` in `platformio.ini` triggers a full NVS erase on next boot:
1. `nvs_flash_erase()` — wipes the entire NVS partition
2. `nvs_flash_init()` — reinitializes the partition
3. All settings are cleared: WiFi credentials, Lichess token, OTA password, calibration data, LED brightness

This is a compile-time flag — not exposed via the web UI. Remove the flag after flashing to resume normal operation. Requires physical USB access.

### Input Validation

API handlers validate all parameters at system boundaries. Internal functions trust data that has already passed validation. Sensitive data is never exposed in API responses — WiFi passwords, Lichess tokens (only masked), and OTA password hashes are excluded from all JSON responses.

## Frontend Architecture

### API Layer

The frontend uses a two-file pattern for server communication:

- **`api.js`** — low-level utilities: `getApi(url)`, `postApi(url, body)`, `deleteApi(url)` fetch wrappers with error handling, and `pollHealth(timeoutMs)` for post-OTA reboot polling (polls `GET /health` until the board responds after a reboot).
- **`provider.js`** — domain-specific `Api` object with a named method for every backend endpoint. Examples: `Api.getNetworks()`, `Api.resign()`, `Api.selectGame(mode, config)`, `Api.getBoardUpdate()`, `Api.getOtaStatus()`, `Api.setOtaPassword(pass)`, `Api.getGames()`, `Api.deleteGame(id)`.

All HTML pages include both scripts via `<script>` tags in `<head>`. All server communication goes through `Api.*` methods — no page contains raw `fetch()` calls. When adding a new API endpoint, add a corresponding method to `provider.js`.

### Pages

| Page | File | Purpose |
|------|------|---------|
| Settings | `index.html` | WiFi management, Lichess token, LED settings, calibration trigger, OTA upload, security |
| Board | `board.html` | Live board view, evaluation bar, move history, board editor, game history browser, review mode, resign button |
| Game Selection | `game.html` | Game mode cards with bot configuration panel. Redirects to board page after selection. |

### Web Asset Pipeline

Source files live in `src/web/`. Three build scripts (defined in `platformio.ini`) process them:

1. `minify.py` — minifies HTML/CSS/JS using `html-minifier-terser`, `clean-css-cli`, `terser`. Skips gracefully if npm tools aren't installed.
2. `prepare_littlefs.py` — gzip-compresses output into `data/` with `.gz` extensions. Files with `.nogz.` in their name are copied uncompressed. Deletes intermediate minified files afterward.
3. `upload_fs.py` — on `pio run -t upload`, hashes `data/` contents and compares with `.littlefs_hash`. Only re-uploads the filesystem image when assets have changed.

The `data/` directory is committed to git so users without npm tools can still build. `.littlefs_hash` is git-ignored. Always edit source files in `src/web/`, never in `data/`.

## Performance Notes

| Technique | Impact |
|-----------|--------|
| I2S DMA for LEDs | CPU-free LED signal generation, avoids WiFi interrupt conflicts |
| Shift register column scanning | 64 sensors via 11 GPIOs with efficient sequential shifting |
| Zobrist hashing (PROGMEM) | O(1) position hashing, ~6.2KB flash not RAM |
| Binary move encoding | 2 bytes/move vs 5 for UCI strings, more games per flash |
| Gzip web assets | Smaller flash footprint and faster browser transfers |
| `constexpr` menu arrays | Menu data in flash, zero RAM cost |
| Fixed-size arrays/stacks | No heap fragmentation from dynamic containers |
| FEN snapshots in game files | Bounded replay time regardless of game length |
| FreeRTOS animation task | Non-blocking LED rendering during sensor/network operations |
| Conditional FS upload (hash) | Skip redundant filesystem uploads on firmware-only changes |

## Utilities

**`ChessUtils`** (`utils.h/cpp`) — namespace with helper functions:
- `boardToFEN(board, turn, state)` / `fenToBoard(fen, board, turn, state)` — FEN ↔ board array conversion with full state restoration (castling rights, en passant, clocks) via `PositionState*`
- `toUCIMove()` / `parseUCIMove()` — UCI move string encoding and parsing
- `getPieceColor(piece)` — returns `'w'` or `'b'`
- `isWhitePiece()`, `isBlackPiece()` — piece color checks
- `isEnPassantMove()`, `getEnPassantCapturedPawnRow()`, `isCastlingMove()` — special move detection
- `evaluatePosition(board)` — material balance in pawns
- `castlingRightsToString()` / `castlingRightsFromString()` — FEN castling field
- `colorName()` — `'w'`→`"White"`, `'b'`→`"Black"`

**`grid_scan_test.cpp`** — standalone hardware debugging utility at the repo root (not compiled in normal builds). Tests shift register column scanning and row GPIO reads. Useful for verifying sensor wiring before running the full firmware.
