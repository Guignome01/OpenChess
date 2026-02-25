# Additional Topics

## AI-Assisted Development

This project provides context files for AI coding assistants:

| File | Purpose |
|------|---------|
| `.github/copilot-instructions.md` | GitHub Copilot instructions |
| `.claude/CLAUDE.md` | Claude Code instructions |

Both files are identical (sync directive at the top of each). They contain the full project architecture, conventions, patterns, and engineering principles. When modifying either file, always apply the same changes to the other.

### Documentation Sync Rule

When a code change affects architecture, public APIs, endpoints, configuration, build pipeline, file structure, or engineering conventions, update the relevant documentation in the same change — never defer to a follow-up:

- New or changed API endpoints → update [api.md](api.md)
- New or removed source files → update [project-structure.md](project-structure.md)
- Build or dependency changes → update [installation.md](installation.md)
- New LED behaviors, menu changes, or physical interaction changes → update the relevant file in `docs/guides/`

## CLI Quick Reference

PlatformIO CLI is not on `PATH` by default. Use the full path:

| Platform | Path |
|----------|------|
| Windows | `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` |
| Linux | `~/.platformio/penv/bin/pio` |

Common commands:

| Action | Command |
|--------|---------|
| Build | `pio run` |
| Upload firmware | `pio run -t upload` |
| Serial monitor | `pio device monitor` |
| Clean build | `pio run -t clean` |

Serial monitor runs at **115200 baud** (configured in `platformio.ini`).

## Filesystem and Storage

### LittleFS

LittleFS stores web assets and game history on the ESP32's flash partition.

**Web assets** — HTML, CSS, JS, images, and sounds are gzip-compressed during the build pipeline and placed in the `data/` directory. The web server serves them with automatic `Content-Encoding: gzip` headers. Files matching `*.nogz.*` skip compression (used for binary files like MP3s).

**Game history** — completed games are stored as binary files under `/games/` on LittleFS:

| File | Purpose |
|------|---------|
| `/games/live.bin` | Current in-progress game (move data) |
| `/games/live_fen.bin` | FEN snapshots for the current game |
| `/games/<id>.bin` | Completed game file |
| `/games/<id>_fen.bin` | FEN snapshots for completed game |

Storage limits:
- Maximum **50** saved games
- Maximum **80%** of LittleFS partition capacity
- Oldest games are deleted when limits are reached

**Binary game format**: Each game file contains a 16-byte packed `GameHeader` followed by 2-byte UCI-encoded moves. The header includes game mode, result, winner, move count, and NTP Unix timestamp. FEN snapshot files store periodic board positions for efficient replay.

**Crash recovery**: The `live.bin` and `live_fen.bin` files persist during gameplay. On boot, if a live game exists, the firmware offers to resume it using `MoveHistory::replayIntoGame()`.

### NVS (Non-Volatile Storage)

NVS stores settings that survive firmware updates. Multiple namespaces partition the data:

| Namespace | Keys | Purpose |
|-----------|------|---------|
| `chess` | `brightness`, `dimMultiplier` | LED settings |
| `chess` | `calDone`, `calData` | Calibration state and mapping |
| `wifiNets` | `count`, `ssid0`–`ssid2`, `pass0`–`pass2` | Saved WiFi networks |
| `wifiCreds` | `ssid`, `password` | Legacy WiFi (auto-migrated) |
| `lichess` | `token` | Lichess API token |
| `ota` | `passHash`, `salt` | OTA password (salted SHA-256 hash) |

All NVS access goes through Arduino's `Preferences` library. `ChessUtils::ensureNvsInitialized()` must be called before first use.

## Security

### WiFi Access Point Lifecycle

The ESP32 runs a WiFi access point named `LibreChess_XXXXXX` (last 6 of MAC address). The AP follows strict lifecycle rules:

1. **Boot**: AP starts immediately (SSID visible, default IP `192.168.4.1`)
2. **STA connection**: after a stable STA connection (~10 second stabilization window), the AP shuts down
3. **STA disconnect**: AP re-enables immediately for recovery access
4. **No saved networks**: AP remains permanently active

This design ensures the board is always accessible for configuration even if WiFi credentials change.

### Known-Networks Registry

Up to 3 WiFi networks can be saved. On boot, the firmware tries each in order until one connects. If all fail, it cycles through them with exponential backoff. The web UI at `http://librechess.local` manages the list.

Legacy single-network NVS data (from earlier firmware versions) is auto-migrated to the new multi-network format on first boot.

### OTA Password Protection

Firmware uploads can be protected with an optional password:

- The password is stored as a **salted SHA-256 hash** in NVS (never plaintext)
- A random 16-byte salt is generated using `esp_random()` when setting the password
- Hashing uses `mbedtls_sha256` (bundled with ESP-IDF, no external dependencies)
- The web UI sends the password in an `X-OTA-Password` header
- Firmware validation checks the ESP32 magic byte (`0xE9`) at offset 0 before accepting an upload

### TLS for External Connections

Outbound HTTPS connections to Lichess and Stockfish APIs use `WiFiClientSecure` with `setInsecure()` — TLS encryption is enabled but certificate pinning is not performed. This trades certificate validation for reliability across different ESP32 SDK versions and certificate store limitations.

### Factory Reset

Adding `-DFACTORY_RESET` to `build_flags` in `platformio.ini` triggers a full NVS erase on next boot (`nvs_flash_erase()` + `nvs_flash_init()`). This clears all saved settings: WiFi credentials, Lichess token, OTA password, calibration data, and LED brightness. Remove the flag after flashing.

## Performance and Optimization

### Zobrist Hashing

Threefold repetition detection uses Zobrist hashing — a technique that assigns random 64-bit values to each piece-square combination. Board positions are hashed incrementally (XOR operations on move/unmove) rather than recomputed from scratch. The pre-computed hash tables are stored in PROGMEM (~6.2 KB flash) via `zobrist_keys.h`.

### Sensor Scanning

The 8×8 sensor grid (64 hall-effect sensors) is read using only 11 GPIO pins through a shift register column-scanning approach. One column is activated at a time via the shift register, and 8 row GPIOs are read simultaneously. This multiplexing keeps the GPIO count low while maintaining fast scan rates (40 ms cycle with 125 ms debounce).

### Binary Move Encoding

Game moves are stored as 2-byte packed values rather than UCI strings, reducing storage overhead from ~5 bytes per move to 2 bytes. This allows more games to fit in the limited LittleFS space.

### Web Asset Compression

All web assets are gzip-compressed during the build pipeline and stored compressed on LittleFS. The web server serves them with `Content-Encoding: gzip` headers, reducing both flash storage usage and transfer time to the browser.

### I2S DMA for LEDs

The WS2812B LED strip is driven through NeoPixelBus using the ESP32's I2S peripheral with DMA (Direct Memory Access). This offloads the timing-critical LED signal generation to hardware, freeing the CPU during LED updates and avoiding timing conflicts with WiFi interrupts.

### FreeRTOS Animation Task

LED animations run on a dedicated FreeRTOS task with its own queue. This decouples animation rendering from the main game loop, preventing visual stuttering during sensor reads or network operations. Long-running animations (waiting, thinking) use atomic stop flags for clean cancellation.

### Memory-Conscious Design

- Menu item arrays use `constexpr` — data lives in flash, not RAM
- Fixed-size arrays and stacks instead of heap-allocated containers
- File-scoped statics for singleton-like objects (menus, navigator)
- `std::array` with compile-time bounds instead of `std::vector`
- `enum class` for type-safe constants without runtime overhead
