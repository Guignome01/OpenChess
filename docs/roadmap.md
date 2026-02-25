# Roadmap

Planned features and areas of improvement for LibreChess. Items are roughly grouped by category but not in a strict priority order.

## Gameplay

### Promotion Piece Selection
Currently, pawn promotion always results in a queen. A future update will allow players to select the promotion piece directly on the physical board, likely using a menu overlay on the promotion rank.

### Draw Offers
There is currently no mechanism to offer or accept a draw. A physical gesture (similar to the resign gesture) and a corresponding web UI button are planned.

### Insufficient Material Detection
Automatically detect and declare a draw when neither side has enough material to force checkmate (e.g., king vs king, king and bishop vs king).

### Impossible Mating Positions
Detect positions where checkmate is theoretically impossible due to board geometry — such as kings isolated on opposite sides with no pieces able to cross to deliver mate.

### Piece Color Assignment
Assign a visual color identity to each piece type for easier board setup. When pieces are missing from the starting position, the board could show the piece's assigned color rather than just white/blue by side, making it faster to identify which specific piece is missing.

### Chess Opening Practice
A guided mode for studying chess openings. The board could walk through known opening lines, showing the expected moves and branching into variations.

### Tactical Puzzles
A puzzle mode that presents tactical positions from a puzzle database. The player solves them on the physical board with LED feedback for correct and incorrect moves.

## Engine & Connectivity

### Offline Bot Play
Run a lightweight chess engine directly on the ESP32, eliminating the need for a WiFi connection to play against an AI opponent. This would make the board fully self-contained.

### Auto OTA Updates
Automatically check for new firmware versions and offer to install them, reducing the manual step of downloading and uploading `.bin` files through the web interface.

## Web Interface

### Single-Page Application
Convert the current multi-page web interface into a single-page application for a smoother user experience, faster navigation, and reduced overhead from serving multiple HTML files.

## Hardware & Calibration

### Improved Calibration Process
The current calibration relies on serial monitor output, requiring a computer connection. A future version should use an entirely LED-guided process so calibration can be completed without any external display or terminal.

### Comprehensive Assembly Guide
The current assembly documentation covers the general construction approach but lacks detailed step-by-step instructions. A future update will provide a more thorough guide with clearer guidance for each assembly stage.

## Development & Quality

### Unit Tests
Add test coverage for the chess engine, move history system, and utility functions. This would catch regressions early and make refactoring safer.

### CI/CD Pipeline
Set up automated builds, tests, and release packaging. Pull requests would be validated automatically before merging.

### Future Refactors
Ongoing identification and resolution of technical debt, such as improving the game mode initialization flow, reducing coupling in the main loop, and standardizing error handling patterns.
