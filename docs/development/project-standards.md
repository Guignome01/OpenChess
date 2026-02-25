# Project Standards

Conventions, architecture principles, and technical patterns used throughout LibreChess. This document is intended as a foundation for understanding the project — both the rules that govern the codebase and the reasoning behind them.

## Code Style

Formatting is enforced via `.clang-format` at the project root (Google style base, no column limit). Run clang-format before committing to keep the codebase consistent.

## Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes | PascalCase | `BoardDriver`, `ChessEngine`, `WiFiManagerESP32` |
| Methods & variables | camelCase | `readSensors()`, `currentTurn`, `isGameOver` |
| Constants & macros | UPPER_SNAKE_CASE | `LED_COUNT`, `SENSOR_READ_DELAY_MS`, `DEBOUNCE_MS` |
| File names | snake_case | `board_driver.cpp`, `chess_engine.h`, `move_history.cpp` |
| Enum values | UPPER_SNAKE_CASE | `RESULT_CHECKMATE`, `AP_ONLY` |

## Architecture Principles

### Separation of Concerns

Each class owns a single responsibility and never crosses into another's domain:

- `BoardDriver` handles all hardware interaction (LEDs, sensors, calibration). No chess logic.
- `ChessEngine` implements chess rules and move generation. No hardware access, no network calls.
- `WiFiManagerESP32` manages WiFi, the web server, and API endpoints. Doesn't touch the board hardware directly.
- `MoveHistory` owns game persistence. Doesn't know about sensors or LEDs.

When adding new functionality, determine which component owns that concern and extend it there. If the functionality crosses boundaries, coordinate through the main loop or injection — don't let one component reach into another's internals.

### Dependency Injection

Components are connected through pointer injection at construction time, not through global state or singletons:

```
BoardDriver boardDriver;
ChessEngine chessEngine;
WiFiManagerESP32 wifiManager(&boardDriver, &moveHistory);
ChessBot botGame(&boardDriver, &chessEngine, &moveHistory, botConfig);
```

This keeps dependencies explicit and testable. A game mode receives exactly the services it needs — nothing more.

### DRY and Reuse

Before writing new code, check whether an existing function or pattern covers the need. Shared logic is extracted into helpers (`ChessUtils`), base classes (`ChessGame`), or utility functions. If the same pattern appears in more than one place, refactor it into a single reusable implementation.

New features should build on existing infrastructure. For example, `ChessLichess` extends `ChessBot` (which extends `ChessGame`) rather than reimplementing bot-move guidance from scratch.

## ESP32 Patterns

The ESP32's constrained environment (limited heap, 520KB SRAM, 4MB flash split between firmware and filesystem) shapes several technical decisions:

### Memory Management

- **Prefer `constexpr` and PROGMEM** for data that doesn't change at runtime. Zobrist hash tables (~6.2KB) live in flash via PROGMEM. Menu item arrays are `constexpr` file-scoped statics.
- **Minimize heap allocations** — use fixed-size arrays, stack allocation, and file-scoped statics over dynamic allocation. Menu navigator uses `std::array<BoardMenu*, 4>` (fixed) instead of `std::vector`.
- **Strong types** — prefer `enum class` over raw integers to catch errors at compile time.

### Concurrency

The firmware uses two execution contexts:

- **Main loop** — runs on the Arduino core's main task. Handles sensor polling, game logic, menu navigation, WiFi updates.
- **Animation task** — a dedicated FreeRTOS task that dequeues `AnimationJob` items from a FreeRTOS queue and executes them. This keeps LED animations non-blocking relative to the main loop.

The LED strip is a shared resource between these two contexts, guarded by a FreeRTOS mutex via the `LedGuard` RAII wrapper.

### I2S DMA

NeoPixelBus drives the WS2812B strip using the ESP32's I2S peripheral with DMA. This means LED data is written to the strip without CPU involvement during the transfer, keeping the main loop responsive even during complex animations.

## LED Patterns

### LedGuard

Multi-step LED updates (clearing the strip, setting multiple squares, then showing) must be wrapped in a `LedGuard` scope to hold the LED mutex:

```cpp
{
    BoardDriver::LedGuard guard(boardDriver);
    boardDriver.clearAllLEDs();
    boardDriver.setSquareLED(row, col, LedColors::Cyan);
    boardDriver.showLEDs();
}
```

Single animation calls (`blinkSquare`, `captureAnimation`) are queued to the animation task and acquire the mutex automatically — no guard needed.

### Animation Queue

Animations run asynchronously via a queue. Long-running animations (thinking, waiting) return an `std::atomic<bool>*` stop flag. Use `stopAndWaitForAnimation(flag)` to cancel, block until the animation task finishes, and free the flag.

Use `waitForAnimationQueueDrain()` as a barrier before writing LEDs directly — it blocks until all queued animations have completed, ensuring a clean slate.

### Color Semantics

Colors in the `LedColors` namespace have fixed meanings throughout the project (see the full table in the [board interactions guide](../guides/board-interactions.md)):

- Cyan = piece origin, White = valid move, Red = capture/error, Green = confirmation, Yellow = check/promotion, Purple = en passant, Orange = resign gesture, Blue = thinking/connecting.

Use these consistently. The `scaleColor(color, factor)` helper adjusts brightness without changing hue.

## Sensor Handling

Sensors are polled every `SENSOR_READ_DELAY_MS` (40ms) with `DEBOUNCE_MS` (125ms) debounce. The debounce ensures a piece must be present (or absent) for a sustained period before the state change is registered, preventing false triggers from sliding pieces.

Always call `boardDriver.readSensors()` before reading sensor state — the state arrays are only updated on explicit read calls.

## Menu System

The board menu system (`BoardMenu` + `MenuNavigator`) follows specific patterns:

- **Constexpr item arrays** — menu layouts are `constexpr MenuItem[]` file-scoped statics stored in flash, not heap-allocated.
- **Two-phase debounce** — every square must be empty for `DEBOUNCE_CYCLES` (5) consecutive polls, then occupied for the same count. This prevents pieces already on the board from immediately triggering selections.
- **ID routing** — each menu level uses distinct ID ranges (0–9 root, 10–19 sub-menu, 20–29 sub-sub) so the main loop can identify which menu produced a result by the ID value alone — no callbacks or virtual dispatch needed.

## Frontend Conventions

### API Layer

The frontend uses a two-file pattern for server communication:

- `api.js` — low-level utilities: `getApi()`, `postApi()`, `deleteApi()` fetch wrappers, and `pollHealth()` for OTA reboot detection.
- `provider.js` — domain-specific `Api` object with a named method for every backend endpoint (e.g., `Api.getBoardUpdate()`, `Api.resign()`, `Api.selectGame()`).

All HTML pages call `Api.*` methods. No page contains raw `fetch()` calls to API endpoints. When adding a new endpoint, add a corresponding method to `provider.js` and use it from the page.

### Web Asset Pipeline

- Edit source files in `src/web/` — HTML, CSS, JS, SVG, MP3
- Never edit files in `data/` directly — they are generated by the build scripts
- Files named `*.nogz.*` skip gzip compression (used for binary assets like MP3s that don't benefit from gzip)
- The build pipeline minifies → gzips → places in `data/`. See the [installation guide](installation.md) for details.

## NVS Persistence

Settings are stored in ESP32 NVS (non-volatile storage) via the Arduino `Preferences` library. Always call `ChessUtils::ensureNvsInitialized()` before the first NVS operation.

NVS is organized by namespace:
- `"wifiNets"` — saved WiFi networks (up to 3)
- `"wifiCreds"` — Lichess token (legacy namespace name, retained for backward compatibility)
- `"ota"` — OTA password hash and salt
- `"boardSettings"` — LED brightness and dark square dimming
- `"calibration"` — sensor/LED mapping data

## Security

Security is treated as a first-class concern, not an afterthought:

- **Input validation** at system boundaries — API handlers validate all parameters before processing. Internal functions trust data that has already been validated.
- **No secrets in responses** — WiFi passwords, Lichess tokens, and OTA passwords are never returned in API responses. The Lichess endpoint returns only a masked token for display.
- **Credential hashing** — OTA passwords are stored as salted SHA-256 hashes (using mbedtls, bundled with ESP-IDF). The raw password is never persisted.
- **TLS encryption** — external API connections (Lichess, Stockfish) use `WiFiClientSecure` with TLS encryption.
- **Minimal dependencies** — prefer `mbedtls` (bundled) over external crypto libraries. Prefer built-in FreeRTOS primitives over third-party abstractions.
