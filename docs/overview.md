# Project Overview

LibreChess is an open-source smart chessboard powered by an ESP32 microcontroller. It turns a physical chessboard into an interactive chess platform — detecting your moves, enforcing rules, showing legal moves through LEDs, and connecting to chess engines and online platforms. No screen required during play.

## Why a Smart Chessboard?

Playing chess on a screen is convenient, but there's something about moving real pieces on a real board. LibreChess bridges that gap — you get the tactile experience of over-the-board play with the convenience of digital features: automatic rule enforcement so you never miss an illegal move, Stockfish analysis to improve your game, and Lichess integration to play opponents worldwide without leaving your board.

The LED feedback transforms the board into a teaching tool. New players see every legal move at a glance, the board catches mistakes before they happen, and when the game is over, you can review it move-by-move from a browser.

## Key Features

- **Real piece detection** — 64 hall-effect sensors detect magnets embedded in chess pieces, tracking every move on the board
- **LED move guidance** — a per-square LED grid highlights legal moves, captures, special moves, check warnings, and game events in real time
- **Full rule enforcement** — legal move validation, check, checkmate, stalemate, castling, en passant, pawn promotion, 50-move rule, and threefold repetition
- **Multiple game modes** — human vs human, human vs Stockfish AI (8 difficulty levels), live Lichess games, and a sensor diagnostic mode
- **Web companion interface** — a browser-based UI served directly from the board for live board view, game review, settings, and game selection
- **Self-contained** — the ESP32 runs everything: firmware, web server, WiFi management, and game persistence. No external computer needed during play
- **Game history** — completed games are saved to flash storage with crash recovery, reviewable from the web interface
- **Physical menu system** — the board itself serves as an 8×8 pixel display for mode selection, difficulty picking, and confirmation dialogs

## Game Modes

LibreChess supports four modes, selectable from the physical board or the web interface:

- **Human vs Human** — two players on the same board with full rule enforcement and alternating turns
- **Human vs Bot** — play against the Stockfish chess engine at 8 difficulty levels, from beginner to master. The board shows the engine's moves with LED guidance so you can execute them physically. Requires WiFi.
- **Lichess** — play live games from your Lichess account. Moves sync automatically between the physical board and the Lichess servers. Requires WiFi and a Lichess API token.
- **Sensor Test** — a diagnostic mode that lights up squares as sensors detect pieces, useful for verifying hardware after assembly

## Web Interface

The board runs a web server accessible at `http://librechess.local` (or the board's IP address). From a browser you can:

- Watch the game live with an evaluation bar, move history, and move sounds
- Review completed games with PGN-style navigation
- Edit the board position with drag-and-drop and FEN import/export
- Select game modes and configure bot settings
- Manage WiFi networks, Lichess token, LED brightness, and firmware updates

## Getting Started

Ready to build a LibreChess board? Head to the [hardware docs](hardware/) for the parts list, wiring, and assembly instructions. Already have a board? See the [installation guide](development/installation.md) to set up the development environment and flash the firmware. For a deep dive into the system internals, see the [architecture reference](development/architecture.md).