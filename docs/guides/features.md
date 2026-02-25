# Board Features

Detailed descriptions of the physical board's interactive features.

## Resign

Players can resign from any game mode using a physical gesture on the board or through the web UI.

### Physical Resign Gesture

The resign gesture is performed on your own turn by manipulating your king:

1. **Hold** — lift your king and keep it off the board for 3 seconds. The king's origin square transitions from the normal move highlight to **orange at 25% brightness**.
2. **First return** — place the king back on its square within 1 second. The square shows **orange at 50%** and all other LEDs clear.
3. **Second lift** — lift the king again and return it within 1 second. Orange increases to **75%**.
4. **Third lift** — lift and return once more within 1 second. Orange reaches **100%**.
5. **Confirmation** — a [confirm dialog](menus.md#confirm-dialog) appears (green = yes, red = no).
6. **Result** — if confirmed, a firework animation plays in the opponent's color and the game ends as a resignation.

If any step times out (piece not returned within the 1-second window), the gesture is silently canceled — no error feedback, just a return to normal play. You can retry from step 1.

The gesture is intentionally multi-step to prevent accidental resignations. The progressive orange brightness gives visual feedback on how far along the gesture you are.

### Web Resign

During an active game, the board page in the web UI shows a ⚑ (flag) resign button. Clicking it triggers a browser confirmation dialog, then a board-level confirm dialog (green/red squares). If confirmed, the game ends the same way as the physical gesture.

### Lichess Resign

In Lichess mode, resigning through either method also submits a resignation to the Lichess server, ending the online game.

## Game History

Every completed game is automatically saved to the ESP32's flash storage (LittleFS) for later review.

### What's Saved

Each game record includes:
- Game mode (Human vs Human or Bot)
- Result (checkmate, stalemate, draw by 50-move rule, draw by threefold repetition, or resignation)
- Winner color
- Bot configuration (player color, difficulty level) for bot games
- Full move list in a compact binary format (2 bytes per move)
- Periodic FEN snapshots for efficient position reconstruction
- Timestamp (from NTP if available)

### Storage Limits

- Maximum 50 saved games
- Storage capped at 80% of LittleFS capacity
- When limits are reached, the oldest games are available for deletion through the web UI

### Reviewing Games

The web UI's board page includes a game history panel:
- Browse all completed games as cards showing date, mode, result, and move count
- Select a game to enter review mode with a full PGN-style move list
- Click any move to jump to that position on the board
- Use navigation buttons (first, previous, next, last) or keyboard arrows to step through
- Games can be individually or bulk-deleted

## Game Resume

If power is lost during a game, the board automatically recovers the game state on the next boot.

### How It Works

During active play, the board continuously persists the game state to flash storage:
- A live game file (`/games/live.bin`) stores the game header and move list
- A companion FEN file (`/games/live_fen.bin`) stores periodic position snapshots

On boot, if these files exist with valid data:
1. The board identifies the game mode from the saved header
2. A square blinks twice in the mode's color (blue for Human vs Human, green for Bot)
3. A confirm dialog appears — green to resume, red to discard
4. If resumed, the engine replays all saved moves to reconstruct the exact game state, then continues play from where it left off

Game resume is supported for Human vs Human and Bot modes. Lichess games are managed by the server and resume through the normal Lichess polling mechanism.

## Dark Square Dimming

LEDs on dark squares are automatically dimmed relative to light squares. The default setting is 70%, meaning dark square LEDs operate at 70% of the brightness of light squares. This compensates for the increased perceived brightness on a dark background, creating a visually uniform appearance.

The dimming percentage is adjustable from 20% to 100% through the web UI's board settings section. The setting is saved to NVS and persists across reboots.
