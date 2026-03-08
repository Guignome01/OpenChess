---
applyTo: "lib/core/**, src/game_mode/**, src/engine/**"
description: "Core chess library classes — ChessGame, ChessBoard, ChessHistory, ChessRules, ChessUtils, ChessFEN, ChessNotation, types, interfaces. Use for any work on lib/core/, game modes, or engine providers including bug fixes, feature additions, refactoring, or test writing."
---

# Core Chess Library (`lib/core/`)

Pure C++ chess library with no hardware dependencies. Natively compilable for unit tests. Uses `std::string` (not Arduino `String`); firmware bridges with `.c_str()` / `std::string()`.

## Component Roles

| Class/Namespace | Role | State |
|-----------------|------|-------|
| `ChessGame` | Central orchestrator, sole owner of game lifecycle | Composes ChessBoard + ChessHistory |
| `ChessBoard` | Position container, move execution, game-end detection | Board array, turn, PositionState, Zobrist history |
| `ChessHistory` | In-memory move log + persistent recording | Cursor-based undo/redo, binary storage |
| `ChessRules` | Move generation, check/checkmate/stalemate detection | Stateless (all static) |
| `ChessUtils` | Piece queries, coordinate helpers, castling/EP analysis | Stateless namespace |
| `ChessIterator` | Board iteration helpers: `forEachSquare`, `forEachPiece`, `somePiece`, `findPiece` | Stateless namespace (header-only) |
| `ChessFEN` | FEN parse/serialize/validate | Stateless namespace |
| `ChessNotation` | Coordinate/SAN/LAN conversion | Stateless namespace |

### Interfaces (DI)

| Interface | Purpose | Concrete impl |
|-----------|---------|---------------|
| `IGameStorage` | Persistence: live game files, FEN tables, finalize/discard | `LittleFSStorage` |
| `IGameObserver` | Board-state notification: `onBoardStateChanged(fen, evaluation)` | `WiFiManagerESP32` |
| `ILogger` | Diagnostic output: `info()`, `error()`, formatted helpers | `SerialLogger` |

All nullable — core classes guard with `if (ptr_)`.

## Data Flow: How a Move Works

Understanding this flow prevents bugs where steps get skipped or reordered:

1. **Firmware** calls `ChessGame::makeMove(from, to, promotion)` — the only entry point for moves
2. **ChessGame** delegates to `ChessBoard::makeMove()`, which:
   - Validates via `ChessRules::isValidMove()` (returns invalid `MoveResult` if illegal)
   - Applies the move (piece movement, captures, castling rook, en passant removal, promotion)
   - Updates `PositionState` (castling rights, EP target, halfmove/fullmove clocks)
   - Updates Zobrist hash + position history
   - Runs `detectGameEnd()` → sets `MoveResult.gameResult` if checkmate/stalemate/draw
   - Invalidates FEN/eval caches
3. **ChessGame** records the move in `ChessHistory` (which auto-persists if recording)
4. **ChessGame** checks threefold repetition (Zobrist comparison), auto-ends game if detected
5. **ChessGame** notifies `IGameObserver` with updated FEN + evaluation
6. **ChessGame** auto-saves recording if the game just ended

**Key invariant**: Steps 2–6 are atomic from the caller's perspective. A valid `makeMove()` always completes all steps.

## Design Decisions

These explain *why* the architecture is the way it is — constraints that code alone doesn't communicate:

- **ChessGame is the only firmware entry point** — firmware (`src/`) must never include `ChessBoard`, `ChessHistory`, `ChessRules`, or `ChessIterator` directly. `ChessGame` re-exports iterator helpers (`forEachSquare`, `forEachPiece`, `somePiece`). This guarantees recording, observer notification, and lifecycle tracking can't be bypassed. Native tests may include internal headers.

- **ChessBoard has no lifecycle state** — `gameOver_`, `gameResult_`, `winnerColor_` live in `ChessGame`, not `ChessBoard`. This separation means `ChessBoard` is a replayable position container: you can undo past game-end, replay games into a scratch board, or query positions without lifecycle side effects.

- **ChessRules and ChessNotation are stateless** — all context (board, turn, `PositionState`) is passed as parameters. This makes them safe to call from any context (temp boards for check detection, history replay, test assertions) without hidden state coupling.

- **Undo clears game-over** — `undoMove()` re-opens a finished game. This enables the web UI to navigate back through completed games. It means code must re-check `isGameOver()` after undo rather than assuming a game stays finished.

- **History has two concerns in one class** — `ChessHistory` handles both the in-memory move log and persistent recording. These were unified because they share the move cursor and branch-on-undo must truncate both the log and storage atomically.

- **Branch-on-undo wipes future moves** — when you undo 3 moves and make a new one, all undone moves are permanently deleted from both memory and storage. This is intentional: the binary recording format doesn't support branching.

- **Header flushes every full turn** — `ChessHistory` only writes the `GameHeader` to flash after black's move. This halves flash wear while still allowing mid-game resume. If the device loses power on white's move, resumption loses at most one move.

- **FEN validation is two-step** — `validateFEN()` checks format (strict), `fenToBoard()` parses (lenient). `ChessBoard::loadFEN()` calls validate first, returns `false` on failure without modifying state. This means `fenToBoard()` alone will accept some invalid FEN — always validate first when accepting user input.

- **Check/checkmate suffixes added by caller** — `ChessNotation` output functions omit `+`/`#` suffixes. `ChessGame::getHistory()` appends them by replaying moves on a temp board. This keeps notation logic pure (no need to apply moves to detect check).

- **Fixed-size arrays everywhere** — `MoveEntry[300]`, `positionHistory_[128]`, no `std::vector`. This is for ESP32: heap fragmentation from repeated vector growth is a real problem with 320KB RAM.

## Key Patterns

- **Dirty-flag caching**: `ChessBoard` caches FEN and evaluation, recomputes only when `fenDirty_`/`evalDirty_` are set by mutations.
- **Composition over inheritance**: `ChessGame` composes `ChessBoard` + `ChessHistory`. No inheritance hierarchy.
- **Nullable DI**: Storage, observer, and logger are pointer-injected. All nullable with `if (ptr_)` guards.
- **Compact 2-byte move encoding**: `encodeMove()`/`decodeMove()` — bits 15..10 = from (row*8+col), bits 9..4 = to, bits 3..0 = promo code. Used for binary storage.
