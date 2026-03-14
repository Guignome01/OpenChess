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
    ├── test_chess_rules_moves.cpp        Move generation per piece type, captures, initial position
    ├── test_chess_rules_check.cpp        Check detection, checkmate, stalemate, move legality under check
    ├── test_chess_rules_special.cpp      Castling, en passant, promotion, helper functions
    ├── test_chess_piece.cpp              ChessPiece: bit extraction, construction, predicates, FEN chars, Zobrist index
    ├── test_chess_utils.cpp              Evaluation, piece color helpers, 50-move rule, castling rights strings
    ├── test_chess_fen.cpp                FEN round-trip, boardToFEN/fenToBoard, validateFEN
    ├── test_chess_notation.cpp           Coordinate/SAN/LAN output and parsing, roundtrip verification
    ├── test_chess_iterator.cpp           Board iteration: forEachSquare, forEachPiece, somePiece, findPiece
    ├── test_chess_hash.cpp               Zobrist hashing: key determinism, computeHash, position sensitivity
    ├── test_chess_board.cpp              ChessBoard: moves, special moves, draws, FEN, reverseMove, king cache, MoveList, HashHistory
    ├── test_chess_game.cpp               ChessGame: lifecycle, draws, observer, history, undo/redo, getHistory
    ├── test_chess_history.cpp            ChessHistory: move log with undo/redo, branch-on-undo
    ├── test_chess_history_recording.cpp  Recording: persistence, header flush, replay, branch-truncation, encode/decode
    └── test_chess_bitboard.cpp           ChessBitboard + ChessAttacks: square mapping, bit ops, square-color masks, leaper tables, slider rays, BitboardSet mutations
```

## File Mirroring Convention

Each `lib/core/src/` source file has a corresponding `test/test_core/test_*.cpp`:

| Source | Test |
|--------|------|
| `chess_board.cpp` | `test_chess_board.cpp` |
| `chess_game.cpp` | `test_chess_game.cpp` |
| `chess_history.cpp` | `test_chess_history.cpp` + `test_chess_history_recording.cpp` |
| `chess_rules.cpp` | `test_chess_rules_moves.cpp` + `test_chess_rules_check.cpp` + `test_chess_rules_special.cpp` |
| `chess_piece.h` | `test_chess_piece.cpp` |
| `chess_utils.cpp` | `test_chess_utils.cpp` |
| `chess_fen.cpp` | `test_chess_fen.cpp` |
| `chess_notation.cpp` | `test_chess_notation.cpp` |
| `chess_bitboard.h` + `chess_attacks.h/cpp` | `test_chess_bitboard.cpp` |

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

### Rules — Moves (`test_chess_rules_moves.cpp`)
Move generation for each piece type (pawn, knight, bishop, rook, queen, king). Blocked paths, captures, initial pawn double-push, edge of board. Starting position move counts. Bulk move generation: `Move`/`MoveList` struct behavior, `generateAllMoves` (initial position count, under check evasions, double check king-only, stalemate zero moves, consistency with per-piece `getPossibleMoves`), `generateCaptures` (capture-only filtering, EP included, no quiet moves), move flag correctness (capture, EP, castling, promotion with all 4 piece types), promotion index round-trip.

### Rules — Check (`test_chess_rules_check.cpp`)
Check detection from every piece type. Checkmate positions. Stalemate positions. Move legality when king is in check — blocking, capturing attacker, king escape. Pin-aware generation: pinned piece cannot leave pin ray; pinned piece can move along pin ray; diagonal pin; double check (only king can move); single-check slider blocking (checkMask filtering); knight check no blocking; two-friendly shielding not pinned; EP horizontal pin. Idempotency (repeated calls produce identical results). King position finder.

### Rules — Special (`test_chess_rules_special.cpp`)
Castling: rights preservation, blocking pieces, through-check prevention, both colors, queenside/kingside. En passant: standard capture, edge cases. Promotion: all piece types. Helper functions.

### Board (`test_chess_board.cpp`)
New game state. Basic moves and captures. En passant execution. Castling execution (both sides). Promotion with piece selection. Check and checkmate detection. Stalemate. 50-move draw. Insufficient material (K vs K, K+B vs K, K+N vs K, K+B vs K+B same-color). Threefold repetition via Zobrist. FEN loading and validation. Compact encode/decode. Board API queries. `reverseMove()` for normal/capture/en passant/castling/promotion. `applyMoveEntry()` replay. King cache (`kingRow`/`kingCol`) correctness after `newGame()`, `loadFEN()`, `makeMove()` (king/non-king), castling, and `reverseMove()`. `MoveList` struct (initial state, add/access, clear, capacity, integration with `getPossibleMoves`). `HashHistory` struct (initial state, add/read, MAX_SIZE constant).

### Game (`test_chess_game.cpp`)
Lifecycle: `endGame()` with various results, `isGameOver()` state, move rejection after game-over, checkmate/stalemate/insufficient/fifty-move auto-set game-over, undo clears game-over, `newGame()`/`loadFEN()` reset game-over. Draw detection. Observer notification and batching. History integration. Undo/redo cursor. `getHistory()` in all three formats (coordinate, SAN, LAN).

### History (`test_chess_history.cpp`)
Move log add/undo/redo. Cursor positioning. Branch-on-undo: adding a move at a branch point wipes future moves.

### History Recording (`test_chess_history_recording.cpp`)
Persistence lifecycle with MockGameStorage. Header flush timing. Game replay from storage. Branch-truncation of storage. ChessGame recording integration: startNewGame, endGame, resume, auto-finish. Compact 2-byte encode/decode stability. On-disk format compatibility.

### Utils (`test_chess_utils.cpp`)
Material evaluation scoring. Piece color detection. 50-move rule counter. Legal move queries. Castling rights string formatting and parsing.

### FEN (`test_chess_fen.cpp`)
Round-trip: board → FEN → board. `boardToFEN()` output correctness. `fenToBoard()` parsing. `validateFEN()`: valid positions, invalid rank structure, bad piece chars, wrong turn field, invalid castling, bad en passant, bad clocks.

### Notation (`test_chess_notation.cpp`)
Coordinate notation output and parsing. SAN output (disambiguation, captures, castling, promotion). LAN output. Auto-format detection from input strings. Round-trip verification: format → parse → same move.

### Piece (`test_chess_piece.cpp`)
Bit extraction: `pieceType()` for all 12 pieces + NONE, `pieceColor()`. Construction: `makePiece()` round-trips. Predicates: `isEmpty()`, `isWhite()`, `isBlack()`, `isColor()`. Color flip: `~Color`, `~Piece` operator overloads. FEN char round-trip: `charToPiece()`/`pieceToChar()` for all valid chars, invalid chars, NONE. PieceType char conversion. Material values: `pieceValue()` for all piece types. Zobrist index: `pieceZobristIndex()` for all pieces + NONE. Color helpers: `pawnDirection()`, `homeRow()`, `promotionRow()`. `colorName()`, `charToColor()`/`colorToChar()`. Zero-initialization safety (`Piece::NONE`, `PieceType::NONE`, `Color::WHITE` all == 0).

### Iterator (`test_chess_iterator.cpp`)
`forEachSquare` visits all 64 squares. `forEachPiece` skips empty. `somePiece` early-exit. `findPiece` locates specific pieces with max limit.

### Hash (`test_chess_hash.cpp`)
Zobrist key determinism. `pieceZobristIndex` mapping. `computeHash` stability. Hash changes with turn flip, castling rights, en passant. Hash equality for identical positions.

### Perft (`test_perft/test_perft.cpp`)
Exhaustive move-tree enumeration for 6 positions from the Chess Programming Wiki. Positions 1–4 verify detailed leaf-level counters: nodes, captures, en passant, castles, promotions, checks, and checkmates. Positions 5–6 verify node counts only (no wiki reference counters). Catches compensating bugs that node-only perft misses (e.g. a missed capture offset by a phantom quiet move).

### Bitboard (`test_chess_bitboard.cpp`)
Square mapping roundtrip (`squareOf(rowOf(sq), colOf(sq)) == sq` for all 64). LERF anchor values (`squareOf(0,0) == SQ_A8`, `squareOf(7,0) == SQ_A1`). Bit manipulation (`popcount`, `lsb`, `popLsb`). Square-color masks (a1 dark, b1 light, popcount 32 each, no overlap). Leaper attack tables (knight on e4, king on a1, pawn attacks per color). Slider ray functions (rook/bishop on empty board and with blockers). X-ray attack functions (`xrayRookAttacks`: north through blocker, no-friendly-blocker; `xrayBishopAttacks`: diagonal through blocker). `rayBetween`: same file, same rank, same diagonal, anti-diagonal, adjacent (returns 0), non-colinear (returns 0). `BitboardSet` mutations (`setPiece`/`removePiece`/`movePiece` consistency, aggregate bitboard correctness).
