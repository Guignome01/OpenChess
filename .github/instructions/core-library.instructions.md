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
| `ChessRules` | Per-piece and bulk move generation, check/checkmate/stalemate detection | Stateless (all static) |
| `ChessEval` | Position evaluation: material count + piece-square tables (PSTs). Returns centipawns (`int`). | Stateless namespace |
| `ChessUtils` | Board-level helpers: coordinate helpers, castling/EP analysis, `gameResultName()` | Stateless namespace |
| `ChessIterator` | Board iteration helpers: `forEachSquare`, `forEachPiece`, `somePiece`, `findPiece` | Stateless namespace (header-only) |
| `ChessBitboard` | Bitboard types, LERF square mapping, bit manipulation (`popcount`, `lsb`, `popLsb`), file/rank masks, square-color masks (`DARK_SQUARES`, `LIGHT_SQUARES`), directional shifts, `BitboardSet` (12 piece + 2 color + occupancy bitboards with `setPiece`/`removePiece`/`movePiece`) | Stateless namespace (header-only) |
| `ChessAttacks` | Precomputed leaper tables (`KNIGHT_ATTACKS[64]`, `KING_ATTACKS[64]`, `PAWN_ATTACKS[2][64]`), classical ray functions (`rookAttacks`, `bishopAttacks`, `queenAttacks`), x-ray attack functions (`xrayRookAttacks`, `xrayBishopAttacks`), `rayBetween(s1, s2)` (strictly between, exclusive), `lineBB(s1, s2)` (full line through both, inclusive, edge-to-edge), `AttackInfo` struct + `computeAttackInfo(bb)` (per-piece-type and per-color attack maps, computed on-demand for evaluation/search) | Stateless namespace (~2.5 KiB tables, initialized once via `initAttacks()`) |
| `ChessHash` | Zobrist key generation (constexpr xorshift64), piece-index mapping, full-board hash computation | Stateless namespace (header-only) |
| `ChessFEN` | FEN parse/serialize/validate | Stateless namespace |
| `ChessNotation` | Coordinate/SAN/LAN conversion | Stateless namespace |

### Interfaces (DI)

| Interface | Purpose | Concrete impl |
|-----------|---------|---------------|
| `IGameStorage` | Persistence: live game files, FEN tables, finalize/discard | `LittleFSStorage` |
| `IGameObserver` | Board-state notification: `onBoardStateChanged(fen, evaluation)` (evaluation in centipawns) | `WiFiManagerESP32` |
| `ILogger` | Diagnostic output: `info()`, `error()`, formatted helpers | `SerialLogger` |
| `Log` | Null-safe logger proxy (value type wrapping `ILogger*`) | Defined in `logger.h` |

All nullable — core classes use `Log` proxy members instead of raw `ILogger*` pointers, eliminating manual null guards.

## Fundamental Concepts

Six independent concepts compose `ChessBoard`'s game state. Understanding these prevents confusion about what each field represents and why they're grouped the way they are.

| # | Concept | Storage | Role |
|---|---------|---------|------|
| 1 | Piece layout | `BitboardSet bb_` + `Piece mailbox_[64]` | Dual representation: 12 piece bitboards + 2 color + occupancy for fast set operations; flat mailbox for O(1) piece identity by square. Both updated in lockstep on every mutation. |
| 2 | Turn | `Color currentTurn_` | Whose move |
| 3 | Move-gen context | `PositionState.castlingRights`, `.epRow`, `.epCol` | Input to ChessRules for move generation and Zobrist hashing |
| 4 | Game clocks | `PositionState.halfmoveClock`, `.fullmoveClock` | 50-move rule counter, FEN move numbering |
| 5 | King cache | `Square kingSquare_[2]` | Single LERF square index per color, derived from `bb_`, maintained incrementally for O(1) check detection. `kingRow(c)` / `kingCol(c)` convert via `rowOf()` / `colOf()`. |
| 6 | Zobrist hash | `uint64_t hash_` | Running Zobrist hash, updated incrementally on every move via XOR deltas (no full recompute). `computeHash()` kept for debug verification. |
| 7 | Hash history | `HashHistory hashHistory_` | Zobrist hashes of past positions for threefold repetition detection |

Concepts 3+4 are bundled in `PositionState` because `MoveEntry` stores the full state for undo — splitting would create friction at that boundary for minimal gain. Concept 5 is derived (redundant with `bb_`) but essential for performance. Concept 6 is derived but avoids expensive full-board hashing on every move. Concept 7 resets on irreversible moves (pawn push, capture).

## Data Flow: How a Move Works

Understanding this flow prevents bugs where steps get skipped or reordered:

1. **Firmware** calls `ChessGame::makeMove(from, to, promotion)` — the only entry point for moves
2. **ChessGame** delegates to `ChessBoard::makeMove()`, which:
   - Validates via `ChessRules::isValidMove()` (returns invalid `MoveResult` if illegal)
   - Applies the move via `applyMoveToBoard()`: updates `BitboardSet` + `mailbox_` in lockstep (piece movement, captures, castling rook, en passant removal, promotion), incrementally updates Zobrist hash via XOR deltas, updates `kingSquare_[]` cache
   - Updates `PositionState` (castling rights, EP target, halfmove/fullmove clocks)
   - Records position hash in `HashHistory`
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

- **Bitboard + mailbox dual representation** — `ChessBoard` stores both a `BitboardSet` (12 piece bitboards + 2 color aggregates + occupancy) and a flat `Piece mailbox_[64]`. Both are updated in lockstep on every mutation. Bitboards enable fast set operations (attack detection via bitwise AND, material counting via `popcount`). The mailbox provides O(1) piece identity for any square (needed by FEN serialization, SAN disambiguation, etc.). LERF (Little-Endian Rank-File, a1=0, h8=63) is the square mapping, with `squareOf(row, col)` / `rowOf(sq)` / `colOf(sq)` bridging to the project's row/col coordinate system. See [Chess Programming Wiki — Bitboards](https://www.chessprogramming.org/Bitboards).

- **Classical ray loops for sliders** — slider attack generation (rook, bishop, queen) uses directional ray walking: shift one square, check for occupied, stop. Zero memory overhead, readable, and sufficient for a board UI + future puzzle solver. Leaper attacks (knight, king, pawn) use precomputed lookup tables (`KNIGHT_ATTACKS[64]`, `KING_ATTACKS[64]`, `PAWN_ATTACKS[2][64]` — ~2.5 KiB total). Tables are initialized once via `ChessAttacks::initAttacks()`. `computeAttackInfo(bb)` builds per-piece-type attack maps for both colors in one pass (pawns via bulk shift, leapers via table, sliders via rays) — used by evaluation (king safety, mobility) and search (move ordering). Returned as an `AttackInfo` struct: `byPiece[2][7]`, `byColor[2]`, `allAttacks`.

- **Incremental Zobrist hashing** — `applyMoveToBoard()` XORs Zobrist key deltas inline (`hash_ ^= pieceKey[from] ^ pieceKey[to]` plus castling/EP/side-to-move changes). `computeHash()` remains for debug verification but is never called in the hot path.

- **FEN validation is two-step** — `validateFEN()` checks format (strict), `fenToBoard()` parses (lenient). `ChessBoard::loadFEN()` calls validate first, returns `false` on failure without modifying state. This means `fenToBoard()` alone will accept some invalid FEN — always validate first when accepting user input.

- **Check/checkmate suffixes added by caller** — `ChessNotation` output functions omit `+`/`#` suffixes. `ChessGame::getHistory()` appends them by replaying moves on a temp board. This keeps notation logic pure (no need to apply moves to detect check).

- **Fixed-size arrays everywhere** — `MoveEntry[300]`, `HashHistory` (256 entries), `MoveList` (218 entries, stores `Move` structs), no `std::vector`. This is for ESP32: heap fragmentation from repeated vector growth is a real problem with 320KB RAM.

## Key Patterns

- **Dirty-flag caching**: `ChessBoard` caches FEN and evaluation, recomputes only when `fenDirty_`/`evalDirty_` are set by mutations.
- **Composition over inheritance**: `ChessGame` composes `ChessBoard` + `ChessHistory`. No inheritance hierarchy.
- **Nullable DI**: Storage, observer, and logger are pointer-injected. All nullable — storage and observer guard with `if (ptr_)`, logger uses `Log` proxy (no manual guards).
- **Compact 2-byte move encoding**: `encodeMove()`/`decodeMove()` — bits 15..10 = from (row*8+col), bits 9..4 = to, bits 3..0 = promo code. Used for binary storage.
- **Color-derived helpers**: `pawnDirection()`, `homeRow()`, `promotionRow()`, `~color` (opponent), `makePiece()` — in `ChessPiece`, use these instead of inline ternaries for color-dependent values.
- **Castling bit mapping**: `castlingCharToBit()` is the single source of truth for K/Q/k/q → bitmask. `hasCastlingRight()` wraps it with color+side semantics. All castling rights logic should use these.
- **MoveEntry factory**: `MoveEntry::build()` encapsulates captured-piece determination and all field assignments. Both `ChessGame::makeMove()` and `ChessHistory::replayInto()` use it.
- **Cohesive data types**: `MoveList` stores `Move[218]` + count — used by both per-piece `getPossibleMoves` (with UI adapter accessors `targetRow(i)`/`targetCol(i)`) and bulk `generateAllMoves`/`generateCaptures`. `Move` struct stores compact from/to/flags (3 bytes: capture, EP, castling, promotion + 2-bit promo piece type). Promotions emit 4 `Move` variants per target square (one per piece type). `ScoredMove` pairs a `Move` with `int16_t score` for future move ordering. `HashHistory` bundles Zobrist keys + count. `BitboardSet` bundles 12 piece + 2 color + 1 occupancy bitboards. All defined in `types.h` / `chess_bitboard.h`.
- **Bitboard serialization via popLsb**: iterating pieces uses `while (bb) { sq = popLsb(bb); ... }` — the standard pattern for extracting set bits from a bitboard one at a time.
- **Attack detection via bitwise AND**: `isSquareUnderAttack` checks `KNIGHT_ATTACKS[sq] & bb.byPiece[knightIdx]` etc. — a single AND per piece type replaces ray-walking loops.
- **Pin-aware move generation**: `ChessRules::getPossibleMoves`, `hasAnyLegalMove(kingSq)`, `generateAllMoves`, and `generateCaptures` compute a `checkMask` and `PinData` (up to 8 pins, one per direction) once per call. Non-king/non-EP moves are filtered via bitwise AND against `pinRayFor(pinData, sq) & checkMask` — no `leavesInCheck` call needed. Only king moves and EP captures still use copy-make (`leavesInCheck`). X-ray helpers (`xrayRookAttacks`, `xrayBishopAttacks`, `rayBetween`) in `ChessAttacks` support pin detection. File-local helpers in `chess_rules.cpp` anonymous namespace: `PinData` struct, `pinRayFor`, `attackersOfSquare`, `computePinData`.
- **Copy-make for legality check**: `leavesInCheck()` copies `BitboardSet` (~120 bytes), applies the move on the copy, and checks the king square. Used for king moves, EP captures, and `isValidMove`. Lightweight because bitboard copy is a flat struct copy.

## Completion Checklist

Every change to `lib/core/` MUST include these steps before the work is considered done. Do not defer any of them to a follow-up.

1. **Tests** — add or update unit tests in `test/test_core/` covering the changed behavior. New public APIs, new structs, renamed parameters, moved functions, and new internal state (like caches) all need test coverage. Register new test functions in `test_core.cpp`.
2. **Scoped instructions** — if the change affects anything described in this file (`core-library.instructions.md`), update it: Component Roles table, Fundamental Concepts table, Data Flow steps, Design Decisions, Key Patterns.
3. **Architecture doc** — if the change affects class responsibilities, public APIs, internal state, or component relationships described in `docs/development/architecture.md`, update the relevant section.
4. **Project structure doc** — if files are added, removed, or their purpose changes, update `docs/development/project-structure.md`.
5. **Testing instructions** — if new test groups are added or existing groups change scope, update `.github/instructions/testing.instructions.md` (file structure listing, file mirroring table, test group descriptions).
6. **Top-level instructions** — if the change introduces a new engineering pattern or convention, update `.github/copilot-instructions.md`.
