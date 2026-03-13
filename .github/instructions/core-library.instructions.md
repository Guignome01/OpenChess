---
applyTo: "lib/core/**, src/game_mode/**, src/engine/**"
description: "Core chess library classes — ChessGame, ChessBoard, ChessHistory, ChessRules, ChessPiece, ChessUtils, ChessFEN, ChessNotation, ChessHash, types, interfaces. Use for any work on lib/core/, game modes, or engine providers including bug fixes, feature additions, refactoring, or test writing."
---

# Core Chess Library (`lib/core/`)

Pure C++ chess library with no hardware dependencies. Natively compilable for unit tests. Uses `std::string` (not Arduino `String`); firmware bridges with `.c_str()` / `std::string()`.

## Component Roles

| Class/Namespace | Role | State |
|-----------------|------|-------|
| `ChessGame` | Central orchestrator, sole owner of game lifecycle | Composes ChessBoard + ChessHistory |
| `ChessBoard` | Position container, move execution, game-end detection | Board array, turn, PositionState, king cache, HashHistory |
| `ChessHistory` | In-memory move log + persistent recording | Cursor-based undo/redo, binary storage |
| `ChessPiece` | Type-safe piece representation: type/color extraction, construction, predicates, color-derived constants, FEN char conversion | Stateless namespace (header-only, all constexpr) |
| `ChessRules` | Move generation, check/checkmate/stalemate detection | Stateless (all static) |
| `ChessUtils` | Board-level helpers: coordinate helpers, castling/EP analysis, material evaluation, `gameResultName()` | Stateless namespace |
| `ChessIterator` | Board iteration helpers: `forEachSquare`, `forEachPiece`, `somePiece`, `findPiece` | Stateless namespace (header-only) |
| `ChessHash` | Zobrist key generation (constexpr xorshift64), piece-index mapping, full-board hash computation | Stateless namespace (header-only) |
| `ChessFEN` | FEN parse/serialize/validate | Stateless namespace |
| `ChessNotation` | Coordinate/SAN/LAN conversion | Stateless namespace |

### Interfaces (DI)

| Interface | Purpose | Concrete impl |
|-----------|---------|---------------|
| `IGameStorage` | Persistence: live game files, FEN tables, finalize/discard | `LittleFSStorage` |
| `IGameObserver` | Board-state notification: `onBoardStateChanged(fen, evaluation)` | `WiFiManagerESP32` |
| `ILogger` | Diagnostic output: `info()`, `error()`, formatted helpers | `SerialLogger` |
| `Log` | Null-safe logger proxy (value type wrapping `ILogger*`) | Defined in `logger.h` |

All nullable — core classes use `Log` proxy members instead of raw `ILogger*` pointers, eliminating manual null guards.

## Fundamental Concepts

Six independent concepts compose `ChessBoard`'s game state. Understanding these prevents confusion about what each field represents and why they're grouped the way they are.

| # | Concept | Storage | Role |
|---|---------|---------|------|
| 1 | Piece layout | `Piece squares_[8][8]` | The 64 squares |
| 2 | Turn | `Color currentTurn_` | Whose move |
| 3 | Move-gen context | `PositionState.castlingRights`, `.epRow`, `.epCol` | Input to ChessRules for move generation and Zobrist hashing |
| 4 | Game clocks | `PositionState.halfmoveClock`, `.fullmoveClock` | 50-move rule counter, FEN move numbering |
| 5 | King cache | `int kingSquare_[2][2]` | Derived from squares_, maintained incrementally for O(1) check detection |
| 6 | Hash history | `HashHistory hashHistory_` | Zobrist hashes of past positions for threefold repetition detection |

Concepts 3+4 are bundled in `PositionState` because `MoveEntry` stores the full state for undo — splitting would create friction at that boundary for minimal gain. Concept 5 is derived (redundant with squares_) but essential for performance. Concept 6 resets on irreversible moves (pawn push, capture).

## Data Flow: How a Move Works

Understanding this flow prevents bugs where steps get skipped or reordered:

1. **Firmware** calls `ChessGame::makeMove(from, to, promotion)` — the only entry point for moves
2. **ChessGame** delegates to `ChessBoard::makeMove()`, which:
   - Validates via `ChessRules::isValidMove()` (returns invalid `MoveResult` if illegal)
   - Applies the move (piece movement, captures, castling rook, en passant removal, promotion)
   - Updates `PositionState` (castling rights, EP target, halfmove/fullmove clocks)
   - Updates Zobrist hash + position history
   - Runs `ChessRules::isGameOver()` → sets `MoveResult.gameResult` if checkmate/stalemate/draw
   - Invalidates FEN/eval caches
3. **ChessGame** logs the move description via `ILogger` (piece, type, from/to square, promotion)
4. **ChessGame** records the move in `ChessHistory` (which auto-persists if recording)
5. **ChessGame** checks threefold repetition (Zobrist comparison), auto-ends game if detected
6. **ChessGame** logs game-end events via `ILogger` (in `endGame()`), or logs check/turn if game continues (in `makeMove()`)
7. **ChessGame** notifies `IGameObserver` with updated FEN + evaluation
8. **ChessGame** auto-saves recording if the game just ended

**Key invariant**: Steps 2–8 are atomic from the caller's perspective. A valid `makeMove()` always completes all steps.

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

- **Fixed-size arrays everywhere** — `MoveEntry[300]`, `HashHistory` (256 entries), `MoveList` (28 entries), no `std::vector`. This is for ESP32: heap fragmentation from repeated vector growth is a real problem with 320KB RAM.

## Key Patterns

- **Dirty-flag caching**: `ChessBoard` caches FEN and evaluation, recomputes only when `fenDirty_`/`evalDirty_` are set by mutations.
- **Composition over inheritance**: `ChessGame` composes `ChessBoard` + `ChessHistory`. No inheritance hierarchy.
- **Nullable DI**: Storage, observer, and logger are pointer-injected. All nullable — storage and observer guard with `if (ptr_)`, logger uses `Log` proxy (no manual guards).
- **Compact 2-byte move encoding**: `encodeMove()`/`decodeMove()` — bits 15..10 = from (row*8+col), bits 9..4 = to, bits 3..0 = promo code. Used for binary storage.
- **Color-derived helpers**: `pawnDirection()`, `homeRow()`, `promotionRow()`, `~color` (opponent), `makePiece()` — in `ChessPiece`, use these instead of inline ternaries for color-dependent values.
- **Castling bit mapping**: `castlingCharToBit()` is the single source of truth for K/Q/k/q → bitmask. `hasCastlingRight()` wraps it with color+side semantics. All castling rights logic should use these.
- **MoveEntry factory**: `MoveEntry::build()` encapsulates captured-piece determination and all field assignments. Both `ChessGame::makeMove()` and `ChessHistory::replayInto()` use it.
- **Cohesive data types**: `MoveList` bundles move array + count (replaces `int& moveCount, int moves[][2]` out-params). `HashHistory` bundles Zobrist keys + count (replaces `uint64_t*, int` pointer-count pairs). Both are defined in `types.h`.

## Completion Checklist

Every change to `lib/core/` MUST include these steps before the work is considered done. Do not defer any of them to a follow-up.

1. **Tests** — add or update unit tests in `test/test_core/` covering the changed behavior. New public APIs, new structs, renamed parameters, moved functions, and new internal state (like caches) all need test coverage. Register new test functions in `test_core.cpp`.
2. **Scoped instructions** — if the change affects anything described in this file (`core-library.instructions.md`), update it: Component Roles table, Fundamental Concepts table, Data Flow steps, Design Decisions, Key Patterns.
3. **Architecture doc** — if the change affects class responsibilities, public APIs, internal state, or component relationships described in `docs/development/architecture.md`, update the relevant section.
4. **Project structure doc** — if files are added, removed, or their purpose changes, update `docs/development/project-structure.md`.
5. **Testing instructions** — if new test groups are added or existing groups change scope, update `.github/instructions/testing.instructions.md` (file structure listing, file mirroring table, test group descriptions).
6. **Top-level instructions** — if the change introduces a new engineering pattern or convention, update `.github/copilot-instructions.md`.
