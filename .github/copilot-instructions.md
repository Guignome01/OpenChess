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
| Factory reset | — | Add `-DFACTORY_RESET` to `build_flags` in `platformio.ini`, then flash |

## Patterns & Conventions

### LED Mutex
LED strip access is guarded by a FreeRTOS mutex via `BoardDriver::LedGuard` (RAII). For multi-step LED updates, use a scoped `{ BoardDriver::LedGuard guard(boardDriver); ... }` block. Single animation calls (e.g., `blinkSquare`, `captureAnimation`) are queued and acquire the mutex automatically.

### Async Animations
Animations run on a dedicated FreeRTOS task via a queue (`AnimationJob`). Long-running animations (`waiting`, `thinking`) return an `std::atomic<bool>*` stop flag — use `stopAndWaitForAnimation(flag)` to cancel, block until finished, and free the flag (single-owner lifecycle). Use `waitForAnimationQueueDrain()` as a barrier before writing LEDs directly, to ensure all queued animations have completed.

### Sensor Debouncing
Sensors are polled every `SENSOR_READ_DELAY_MS` (40ms) with `DEBOUNCE_MS` (125ms) debounce. Always call `boardDriver.readSensors()` before reading state.

### Menu System
The board doubles as a primitive 8×8 pixel GUI via `BoardMenu` and `MenuNavigator` (defined in `board_menu.h/cpp` and `menu_navigator.h/cpp`). Menu configuration (item layouts, IDs, instances) lives in `menu_config.h/cpp`.

- **`MenuItem`** — `{row, col, color, id}` struct. Positions authored in white-side orientation (row 7 = rank 1). Use `constexpr` file-scoped arrays so data lives in flash.
- **`menu_config.h/cpp`** — centralizes `MenuId` namespace (distinct id ranges per level), `constexpr MenuItem[]` layout arrays, `extern` menu/navigator instances, and `initMenus(BoardDriver*)` for two-phase initialization. Call `initMenus()` once in `setup()`.
- **Two-phase debounce** — every menu square must be seen empty for `DEBOUNCE_CYCLES` (5) consecutive polls, then occupied for the same count. This prevents false triggers from pieces already on the board. Implemented in `BoardMenu::updateDebounce()`.
- **Selection feedback** — on confirmed selection, the square blinks once in its own color via `blinkSquare()` before returning the id.
- **Back button** — set via `setBackButton(row, col)`, lit in `LedColors::White`. Omit for root menus. The navigator auto-pops on back and re-shows the parent.
- **Orientation** — `setFlipped(true)` mirrors row coordinates (`row' = 7 - row`) so menus face a black-side player. Default is white-side.
- **ID routing** — use distinct id ranges per menu level (0–9 root, 10–19 sub-menu, 20–29 sub-sub, etc.) so the `loop()` handler can identify which menu produced a result without callbacks or virtual dispatch.
- **`boardConfirm()`** — standalone yes/no dialog (green/red squares). Blocking. Returns `bool`.
- **No heap allocation** — menus are file-scoped statics, item arrays are `constexpr`, navigator uses a fixed `std::array<BoardMenu*, 4>` stack.

### Color Semantics (LedColors namespace)
Colors have fixed meanings in `led_colors.h`: `White` = valid move, `Red` = capture/error, `Green` = confirmation, `Yellow` = check/promotion, `Cyan` = piece origin, `Purple` = en passant, `Blue` = bot thinking, `Orange` = resign gesture. `scaleColor(color, factor)` is an inline constexpr helper for brightness scaling. Use these consistently.

### Resign System
Players can resign by performing a physical king gesture or via the web UI resign button.

- **Physical gesture** — on the current player's turn, lift your king and hold it off its square for 3 seconds. The origin square transitions from valid-move highlights to dim orange (25%). Return the king (50%), then perform 2 quick lift-and-return cycles within 1 second each — orange brightens to 75% then 100%. A `boardConfirm()` dialog follows; on confirm the opponent wins with a firework animation.
- **Web resign** — the web UI shows a ⚑ button during live play. Click triggers a JS `confirm()`, then `POST /resign`. `WiFiManagerESP32` sets a `hasPendingResign` flag, relayed to the active game via `main.cpp`. `processResign()` picks it up on the next update cycle and calls `handleResign()`.
- **Inline gesture** — the entire resign gesture runs inline inside `tryPlayerMove()`. After the 3s hold and initial return, `continueResignGesture(row, col, color)` is called as a blocking loop for the 2 remaining quick lifts. No state machine — all resign state is local variables within these functions. Timeouts silently cancel (no error feedback).
- **Virtual hook** — `handleResign(char resignColor)` is `virtual` so `ChessLichess` can override it to call `LichessAPI::resignGame()` and manage the thinking animation lifecycle.
- **LED helpers** — `showResignProgress(row, col, level, clearFirst)` and `clearResignFeedback(row, col)` encapsulate the LedGuard + setSquareLED + showLEDs pattern for resign feedback. `showIllegalMoveFeedback(row, col)` wraps the red-blink for illegal moves.
- **Turn restriction** — resign is only processed on the current player's turn (all modes), matching real chess conventions.

### Frontend JS Architecture
Web pages share API logic through two scripts in `src/web/scripts/`:
- **`api.js`** — low-level utilities: typed wrappers `getApi()`, `postApi()`, `deleteApi()` for fetch calls, and `pollHealth(timeoutMs)` for post-OTA reboot polling.
- **`provider.js`** — domain-specific `Api` object with named methods for every endpoint (e.g., `Api.getNetworks()`, `Api.resign()`, `Api.selectGame()`, `Api.getOtaStatus()`, `Api.setOtaPassword()`). Centralizes URL construction and response parsing. All HTML pages call `Api.*` methods — no page contains raw `fetch()` calls to API endpoints.

Both scripts are included via `<script>` tags in `<head>` on all pages. When adding new API endpoints, add a corresponding method to the appropriate provider and use it from the page.

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

## WiFi & Security Architecture

### WiFi State Machine
`WiFiManagerESP32` uses an event-driven `WiFiState` enum (`AP_ONLY`, `CONNECTING`, `CONNECTED`, `RECONNECTING`) managed via `WiFi.onEvent()` callbacks. The AP is **only active when there is no STA connection** — it starts on boot, shuts down ~10 seconds after a stable STA connection (stabilization timer prevents flapping), and re-enables immediately on STA disconnect. A non-blocking `update()` method called from `loop()` handles reconnection with exponential backoff across saved networks.

### mDNS
`WiFiManagerESP32` starts mDNS with hostname `openchess` (defined as `MDNS_HOSTNAME` in `wifi_manager_esp32.h`), enabling access via `http://openchess.local`. mDNS initializes in `begin()` after the AP starts, and restarts in `handleWiFiConnected()` to bind to the STA interface. The hostname is included in the `/wifi/networks` JSON response for the web UI.

### Known-Networks Registry
Up to 3 WiFi networks are stored in NVS namespace `"wifiNets"` (keys `"count"`, `"ssid0"`/`"pass0"` through `"ssid2"`/`"pass2"`). On boot, networks are tried in order until one connects. API endpoints (`/wifi/networks`, `/wifi/connect`, `/wifi/scan`) manage the list. Legacy single-network NVS data (`"wifiCreds"`) is auto-migrated on first boot.

### Security Model
The web UI is open to anyone on the local network. Sensitive informations are never exposed outside the server such as passwords and token.

- **OTA Password** — optional password to protect firmware uploads. Stored as a salted SHA-256 hash in NVS namespace `"ota"` (keys `"passHash"`, `"salt"`). When set, OTA uploads must include the password in an `X-OTA-Password` header. Managed via `POST /ota/password` (set/change/remove) and queried via `GET /ota/status`. The web UI separates password management (Security section) from firmware upload (OTA Update section).
- **Factory Reset** — adding `-DFACTORY_RESET` to `build_flags` in `platformio.ini` erases the entire NVS partition on next boot (`nvs_flash_erase()` + `nvs_flash_init()`), clearing all saved settings (WiFi credentials, Lichess token, OTA password, calibration, LED brightness). This is a compile-time flag — not exposed via the web UI. Remove the flag after flashing to resume normal operation.

### TLS & Outbound Security
External HTTPS connections (Lichess, Stockfish) use `WiFiClientSecure` with `setInsecure()` — TLS encryption without certificate pinning.

## Engineering Principles

- **Separation of Concerns** — each class and module owns a single responsibility. Hardware in `BoardDriver`, chess rules in `ChessEngine`, network management in `WiFiManagerESP32`. Never mix concerns across boundaries.
- **Loose Coupling** — components communicate through well-defined interfaces and pointer injection, never through global state or direct member access. New subsystems (network registry, OTA password) follow the same pattern — expose a minimal public API, keep internals private.
- **Dependency Minimization** — avoid adding libraries when the ESP-IDF or Arduino framework already provides the functionality. Prefer `mbedtls` (bundled) over external crypto libraries. Prefer built-in FreeRTOS primitives over third-party abstractions.
- **DRY (Don't Repeat Yourself)** — eliminate duplication across the codebase. Extract shared logic into helpers, base classes, or utility functions. If the same pattern appears in more than one place, refactor it into a single reusable implementation.
- **Reuse Before Creating** — before writing new code, check whether an existing function, helper, or pattern already covers the need. New features should build on top of existing infrastructure rather than duplicating or reinventing it. Adapt callers to use a shared implementation instead of creating parallel solutions for each use case.
- **Design Patterns** — use them when they solve a real problem, not for ceremony. Observer pattern for WiFi events, RAII for resource guards (`LedGuard`). Name patterns explicitly in comments when used so future readers understand the intent.
- **ESP32 Awareness** — adapt code for the ESP32's constrained environment. Prefer `constexpr` and compile-time computation where possible. Minimize heap allocations (fixed arrays, stack allocation, file-scoped statics). Use strong types (`enum class`) over raw integers. Be mindful of task stack sizes, watchdog timers, and ISR-safe operations.
- **Performance & Readability** — optimize for performance and memory usage, but always prioritize readability over cleverness. If a micro-optimization makes the code harder to understand, prefer the readable version unless profiling proves the optimization is necessary.
- **Simplicity Without Compromise** — keep code simple, but never at the cost of correctness or completeness. Prioritize the requirements and logic of a feature first, then look for ways to simplify the implementation. Do not let the pursuit of simplicity cause edge cases or criteria to be overlooked.
- **Security by Default** — consider security implications in every applicable scenario: input validation at system boundaries, never exposing secrets in API responses, hashing credentials, protecting sensitive endpoints, using TLS for external calls. Treat security as a first-class concern, not an afterthought.
- **Code Quality** — validate inputs at system boundaries (API handlers), trust data internally. Prefer compile-time checks over runtime checks where possible.
- **Documentation** — keep `README.md` and `.github/copilot-instructions.md` in sync with architectural changes. When adding a new subsystem, document its purpose, lifecycle, and integration points in those files. Code comments explain *why*, not *what*.
- **Dead Code Cleanup** — after every refactoring or feature change, verify that no dead code is left behind: unused functions, unreachable branches, stale variables, orphaned imports, or commented-out blocks. Audit callers when changing a shared interface to ensure consistency across all call sites.
