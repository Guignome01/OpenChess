---
description: "Use when writing, modifying, or debugging unit tests. Covers test architecture, file mirroring convention, test helpers, and per-file test group descriptions."
applyTo: "test/**, lib/core/**"
---

# Unit Testing Guide

## Architecture

Tests run natively on the host (no ESP32) using PlatformIO Unity framework with `[env:native]`.

The `lib/core/` library has zero Arduino dependencies — all chess logic compiles natively. Tests include `lib/core/` headers directly.

## Running Tests

| Action | Command |
|--------|---------|
| Run all tests | `pio test -e native` |
| Run the suite | `pio test -e native -f test_core` || Run perft suite | `pio test -e native -f test_perft` |
## File Structure

Fast unit tests live in `test/test_core/` (one suite, one binary). The heavyweight perft suite lives separately in `test/test_perft/`.

```
test/
├── test_helpers.h                       Shared utilities (setupInitialBoard, clearBoard, placePiece, etc.)
├── test_perft/
│   └── test_perft.cpp                   Perft move-tree enumeration with detailed counters (captures, EP, castles, promotions, checks, checkmates)
└── test_core/
    ├── test_core.cpp                    Main entry: setUp/tearDown, shared globals, register calls
    ├── test_attacks.cpp                 attacks: leaper tables, slider rays, x-ray attacks, geometry rays (between, line), computeAll
    ├── test_bitboard.cpp                LibreChess: square mapping, bit ops, square-color masks, BitboardSet mutations
    ├── test_evaluation.cpp              eval: material scoring, pawn structure (passed/isolated/doubled/backward), tapered evaluation
    ├── test_fen.cpp                     FEN round-trip, boardToFEN/fenToBoard, validateFEN
    ├── test_game.cpp                    Game: lifecycle, draws, observer, history, undo/redo, getHistory
    ├── test_history.cpp                 History: move log with undo/redo, branch-on-undo
    ├── test_history_persistence.cpp     Recording: persistence, header flush, replay, branch-truncation, encode/decode
    ├── test_iterator.cpp                Board iteration: forEachSquare, forEachPiece, somePiece, findPiece
    ├── test_movegen.cpp                 Move generation per piece type, captures, bulk generation, move flags, legal move queries
    ├── test_notation.cpp                Coordinate/SAN/LAN output and parsing, roundtrip verification
    ├── test_piece.cpp                   piece: type extraction, construction, predicates, FEN chars, material values, Zobrist index, color helpers
    ├── test_position.cpp                Position: moves, special moves, draws, FEN, reverseMove, king cache, MoveList, HashHistory
    ├── test_rules.cpp                   rules: check/checkmate/stalemate detection, pin-aware generation, castling, en passant, promotion, isDraw, isGameOver
    ├── test_utils.cpp                   utils: 50-move rule, castling rights strings, coordinate helpers, board transforms, special-move analysis
    └── test_zobrist.cpp                 Zobrist hashing: key determinism, computeHash, position sensitivity
```

## File Mirroring Convention

Each `lib/core/src/` source file has a corresponding `test/test_core/test_*.cpp`:

| Source | Test |
|--------|------|
| `position.cpp` | `test_position.cpp` |
| `game.cpp` | `test_game.cpp` |
| `history.cpp` | `test_history.cpp` + `test_history_persistence.cpp` |
| `movegen.cpp` + `rules.cpp` | `test_movegen.cpp` + `test_rules.cpp` |
| `piece.h` | `test_piece.cpp` |
| `utils.cpp` | `test_utils.cpp` |
| `evaluation.cpp` | `test_evaluation.cpp` |
| `fen.cpp` | `test_fen.cpp` |
| `notation.cpp` | `test_notation.cpp` |
| `bitboard.h` | `test_bitboard.cpp` |
| `attacks.h/cpp` | `test_attacks.cpp` |
| `iterator.h` | `test_iterator.cpp` |
| `zobrist.h/cpp` | `test_zobrist.cpp` |

Place tests in the file that mirrors the tested functionality. When creating a new `lib/core/src/` file, create a matching test file and register its test functions in `test_core.cpp`.

## Test Helpers (`test_helpers.h`)

Shared utilities available to all test files:
- `setupInitialBoard(bb, mailbox)` — sets up standard starting position in `BitboardSet` + `Piece mailbox[64]`
- `clearBoard(bb, mailbox)` — empties the bitboard set and mailbox
- `placePiece(bb, mailbox, row, col, piece)` — places a piece at specific coordinates in both representations

## Testing Principles

- **Tests guard correctness** — never modify a test to make it pass. If a test fails, fix the production code.
- **Tests must stay in sync** — when changing chess logic in `lib/core/`, update or add tests in the same change. New public APIs, new structs, renamed parameters, moved functions, and new internal state (caches, derived fields) all need test coverage. This includes:
  - Struct behavior tests for new data types (e.g. `MoveList`, `HashHistory`)
  - State maintenance tests for derived/cached fields (e.g. king cache across `makeMove`/`reverseMove`/`loadFEN`)
  - Updating existing tests when signatures or patterns change
- **Test group descriptions must stay current** — when adding new test sections, update the Test Group Details and file structure listing in this file.
- **Always test changes** — every logic change must be validated by running the test suite before committing.

## Test Group Details

### Movegen (`test_movegen.cpp`)
Move generation for each piece type (pawn, knight, bishop, rook, queen, king). Blocked paths, captures, initial pawn double-push, edge of board. Starting position move counts. Legal move queries (`hasAnyLegalMove`). Bulk move generation: `Move`/`MoveList` struct behavior, `generateAllMoves` (initial position count, under check evasions, double check king-only, stalemate zero moves, consistency with per-piece `getPossibleMoves`), `generateCaptures` (capture-only filtering, EP included, no quiet moves), move flag correctness (capture, EP, castling, promotion with all 4 piece types), promotion index round-trip.

### Rules (`test_rules.cpp`)
Check detection from every piece type. Checkmate positions. Stalemate positions. Move legality when king is in check — blocking, capturing attacker, king escape. Pin-aware generation: pinned piece cannot leave pin ray; pinned piece can move along pin ray; diagonal pin; double check (only king can move); single-check slider blocking (checkMask filtering); knight check no blocking; two-friendly shielding not pinned; EP horizontal pin. Idempotency (repeated calls produce identical results). King position finder. `isValidMove`, `isDraw`, `isGameOver`, threefold repetition. Castling: rights preservation, blocking pieces, through-check prevention, both colors, queenside/kingside. En passant: standard capture, edge cases. Promotion: all piece types.

### Position (`test_position.cpp`)
New game state. Basic moves and captures. En passant execution. Castling execution (both sides). Promotion with piece selection. Check and checkmate detection. Stalemate. 50-move draw. Insufficient material (K vs K, K+B vs K, K+N vs K, K+B vs K+B same-color). Threefold repetition via Zobrist. FEN loading and validation. Compact encode/decode. Board API queries. `reverseMove()` for normal/capture/en passant/castling/promotion. `applyMoveEntry()` replay. King cache (`kingRow`/`kingCol`) correctness after `newGame()`, `loadFEN()`, `makeMove()` (king/non-king), castling, and `reverseMove()`. `MoveList` struct (initial state, add/access, clear, capacity, integration with `getPossibleMoves`). `HashHistory` struct (initial state, add/read, MAX_SIZE constant).

### Game (`test_game.cpp`)
Lifecycle: `endGame()` with various results, `isGameOver()` state, move rejection after game-over, checkmate/stalemate/insufficient/fifty-move auto-set game-over, undo clears game-over, `newGame()`/`loadFEN()` reset game-over. Draw detection. Observer notification and batching. History integration. Undo/redo cursor. `getHistory()` in all three formats (coordinate, SAN, LAN).

### History (`test_history.cpp`)
Move log add/undo/redo. Cursor positioning. Branch-on-undo: adding a move at a branch point wipes future moves.

### History Persistence (`test_history_persistence.cpp`)
Persistence lifecycle with MockGameStorage. Header flush timing. Game replay from storage. Branch-truncation of storage. Game recording integration: startNewGame, endGame, resume, auto-finish. Compact 2-byte encode/decode stability. On-disk format compatibility.

### Utils (`test_utils.cpp`)
50-move rule counter. Castling rights string formatting and parsing (`castlingCharToBit`, `hasCastlingRight`). Coordinate helpers (`squareName`, `fileChar`, `rankChar`, `fileIndex`, `rankIndex`, `isValidSquare`). Special-move analysis (`checkEnPassant`, `checkCastling`, `updateCastlingRights`). `applyBoardTransform`. `boardToText`. `positionState`. `gameResultName`. `isValidPromotionChar`.

### Evaluation (`test_evaluation.cpp`)
Material evaluation scoring. Pawn structure evaluation (symmetry, passed pawn bonus, doubled/isolated penalties). Tapered evaluation (opening symmetry, endgame king centralization, phase-dependent king PST blend). Pawn-structure analysis functions: `isPassed`, `isIsolated`, `isDoubled`, `isBackward`.

### FEN (`test_fen.cpp`)
Round-trip: board → FEN → board. `boardToFEN()` output correctness. `fenToBoard()` parsing. `validateFEN()`: valid positions, invalid rank structure, bad piece chars, wrong turn field, invalid castling, bad en passant, bad clocks.

### Notation (`test_notation.cpp`)
Coordinate notation output and parsing. SAN output (disambiguation, captures, castling, promotion). LAN output. Auto-format detection from input strings. Round-trip verification: format → parse → same move.

### Piece (`test_piece.cpp`)
piece namespace: bit extraction (`pieceType()` for all 12 pieces + NONE, `pieceColor()`), construction (`makePiece()` round-trips), predicates (`isEmpty()`, `isWhite()`, `isBlack()`, `isColor()`), color flip (`~Color`, `~Piece` operator overloads), FEN char round-trip (`charToPiece()`/`pieceToChar()` for all valid chars, invalid chars, NONE), PieceType char conversion, material values (`pieceValue()`, `pieceTypeValue()` for all piece types), Zobrist index (`pieceZobristIndex()` for all pieces + NONE), color helpers (`pawnDirection()`, `homeRow()`, `promotionRow()`, `colorName()`, `charToColor()`/`colorToChar()`, `getPieceColor`, `opponentColor`), zero-initialization safety, `isPromotion()` edge cases.

### Iterator (`test_iterator.cpp`)
`forEachSquare` visits all 64 squares. `forEachPiece` skips empty. `somePiece` early-exit. `findPiece` locates specific pieces with max limit.

### Zobrist (`test_zobrist.cpp`)
Zobrist key determinism. `pieceZobristIndex` mapping. `computeHash` stability. Hash changes with turn flip, castling rights, en passant. Hash equality for identical positions.

### Perft (`test_perft/test_perft.cpp`)
Exhaustive move-tree enumeration for 6 positions from the Chess Programming Wiki. Positions 1–4 verify detailed leaf-level counters: nodes, captures, en passant, castles, promotions, checks, and checkmates. Positions 5–6 verify node counts only (no wiki reference counters). Catches compensating bugs that node-only perft misses (e.g. a missed capture offset by a phantom quiet move).

### Bitboard (`test_bitboard.cpp`)
Square mapping roundtrip (`squareOf(rowOf(sq), colOf(sq)) == sq` for all 64). LERF anchor values (`squareOf(0,0) == SQ_A8`, `squareOf(7,0) == SQ_A1`). Bit manipulation (`popcount`, `lsb`, `popLsb`). Square-color masks (a1 dark, b1 light, popcount 32 each, no overlap). `BitboardSet` mutations (`setPiece`/`removePiece`/`movePiece` consistency, aggregate bitboard correctness).

### Attacks (`test_attacks.cpp`)
Leaper attack tables (knight on e4, king on a1, pawn attacks per color). Slider attack functions (rook/bishop/queen on empty board and with blockers). Bulk slider correctness (all 64 squares × 5 occupancy patterns cross-checked against reference ray implementation for both rook and bishop). X-ray attack functions (`xrayRook`, `xrayBishop`). `between` geometry (file/rank/diagonal/anti-diagonal/adjacent/non-colinear). `line` geometry (rank/file/diagonal/non-colinear/endpoints). `computeAll` validation (initial knight attacks, pawn bulk attacks, color unions, kings-only board).
