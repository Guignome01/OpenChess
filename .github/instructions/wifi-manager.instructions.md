---
applyTo: "src/wifi_manager_esp32.*"
description: "WiFiManagerESP32 internals — WiFi state machine, AP lifecycle, reconnection backoff, NVS persistence, mDNS, IGameObserver caching, flag-based relay pattern, OTA security. Use when modifying WiFi connectivity, web server setup, or state relay logic."
---

# WiFiManagerESP32

Manages WiFi connectivity, the async web server, and all HTTP API endpoints. Implements `IGameObserver` for board state push from `ChessGame`.

## WiFi State Machine

Event-driven `WiFiState` enum managed via `WiFi.onEvent()` callbacks. Uses a singleton `instance` pointer for the static event callback.

```
AP_ONLY → CONNECTING → CONNECTED → RECONNECTING → CONNECTING → ...
```

| State | AP active | STA | Description |
|-------|-----------|-----|-------------|
| `AP_ONLY` | Yes | Off | No saved networks or initial boot |
| `CONNECTING` | Yes | Trying | Attempting STA connection |
| `CONNECTED` | No (after stabilization) | Up | Stable connection |
| `RECONNECTING` | Yes | Lost | Cycling through saved networks |

### AP Lifecycle
- AP starts immediately on boot (`LibreChess`, password `chess123`, IP `192.168.4.1`)
- After stable STA for `AP_STABILIZATION_MS` (10s), a FreeRTOS timer (`apStabilizationCallback`) disables the AP
- If STA drops, AP re-enables immediately — prevents flapping on intermittent WiFi

### Reconnection
- On STA disconnect → `RECONNECTING` state
- Cycles through all saved networks with exponential backoff
- Starts at `RECONNECT_INITIAL_MS` (5s), caps at `RECONNECT_MAX_MS` (60s)
- `reconnectNetworkIndex` tracks which network to try next

### mDNS
- Hostname: `librechess` (`MDNS_HOSTNAME`)
- Started in `begin()`, restarted in `handleWiFiConnected()` to rebind to STA interface
- Enables `http://librechess.local` access

## Known-Networks Registry

Up to `MAX_SAVED_NETWORKS` (3) WiFi networks stored in NVS namespace `"wifiNets"`:
- Keys: `"count"`, `"ssid0"`/`"pass0"` through `"ssid2"`/`"pass2"`
- Loaded on boot, tried in order
- `loadNetworks()` / `saveNetworks()` manage NVS persistence

## Board State Relay (IGameObserver)

`WiFiManagerESP32` implements `IGameObserver`. `ChessGame` calls `onBoardStateChanged(fen, evaluation)` automatically after every board mutation.

**Cached state** (populated in `onBoardStateChanged`, served by `GET /board-update`):
- `currentFen` — current board position
- `boardEvaluation` — position evaluation
- `cachedMoveIndex_`, `cachedMoveCount_` — history cursor
- `cachedCanUndo_`, `cachedCanRedo_` — navigation availability

The web UI polls `GET /board-update` to read this cached state as JSON.

## Flag-Based Relay Pattern

The web server runs on the async task context, but game mutations must happen on the main loop. Several features use a flag-based relay:

| Feature | Web handler sets | Main loop reads | Main loop clears |
|---------|-----------------|-----------------|------------------|
| Game selection | `gameMode` | `getSelectedGameMode()` | `resetGameSelection()` |
| Board edit | `pendingFenEdit` + `hasPendingEdit` | `getPendingBoardEdit(fenOut)` | `clearPendingEdit()` |
| Resign | `hasPendingResign` | `getPendingResign()` | `clearPendingResign()` |
| Navigation | `pendingNavAction_` | `getPendingNavAction()` | `clearPendingNav()` |

**Important**: Never call `ChessGame` mutation methods from a web handler. Always set a flag and let the main loop consume it.

### Navigation Details
- `setGameRef(const ChessGame*)` — provides read-only game reference for nav state queries
- `setNavigationBlocked(bool)` — bot mode blocks navigation during engine's turn
- `POST /nav` returns `409 Conflict` when blocked

## OTA Security

- Password stored as SHA-256 hash + random salt in NVS
- `hashPassword()` uses mbedtls SHA-256
- `generateRandomHex()` uses `esp_random()` for salt generation
- Upload validates ESP32 magic byte (`0xE9`) at offset 0
- `X-OTA-Password` header required when password is set
- `otaHasError` flag tracks errors across multi-chunk uploads

## Web Server Setup

`AsyncWebServer` on port 80. All routes registered in `begin()`.

### Static file serving
- `serveStatic("/sounds/", LittleFS, "/sounds/")` — raw MP3 (no gzip)
- `serveStatic("/pieces/", LittleFS, "/pieces/")` — immutable cache headers
- `serveStatic("/", LittleFS, "/")` — default `index.html`, auto `.gz` detection
- Unmatched paths → `404 Not Found`

### Configuration getters (public API for main loop)
- `getSelectedGameMode()` / `resetGameSelection()` — game mode selection
- `getStockfishSettings()` / `getBotPlayerColor()` — bot configuration
- `getLichessConfig()` / `getLichessToken()` — Lichess configuration
- `getCurrentFen()` / `getEvaluation()` — cached board state
- `getWiFiState()` / `isWiFiConnected()` — WiFi status

For the complete HTTP API endpoint reference, see `.github/instructions/api.instructions.md`.

## Design Decisions

- **Flag-based relay, not direct mutation** — web handlers run on the async server task context, but `ChessGame` mutations must happen on the main loop (not thread-safe). The flag pattern (`hasPendingResign`, `pendingNavAction_`, etc.) bridges this safely. Never call `ChessGame` methods from a web handler, even if it seems to work — it will eventually corrupt state under load.

- **AP stays active during connection attempts** — the AP doesn't shut down until STA is stable for 10s (`AP_STABILIZATION_MS`). This prevents the user from losing access to the config UI if STA connection fails. If STA drops later, AP re-enables immediately.

- **Singleton pattern for event callbacks** — `WiFi.onEvent()` requires a static function, so a static `instance` pointer provides access to the instance. This is the only singleton in the project and exists only because the ESP32 WiFi API requires it.

- **OTA password is hashed, not stored plaintext** — SHA-256 + random salt via `mbedtls`. The salt prevents rainbow table attacks. The password is validated per-upload via the `X-OTA-Password` header, and the ESP32 magic byte (`0xE9`) check prevents uploading non-firmware files.
