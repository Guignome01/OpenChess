# Components

Complete parts list for building a LibreChess smart chessboard.

## Core Electronics

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| ESP32 Dev Board | Any ESP32-WROOM-32 variant | 1 | Main controller — runs firmware, WiFi, and web server |
| WS2812B LED Strip | 30 LEDs/m, 3 meters | 1 | Per-square lighting for move feedback and animations |
| Hall Effect Sensors | A3144 TO-92 package | 64 | One per square — detects magnets in chess pieces |
| Shift Register | SN74HC595N | 1 | Drives the 8-column sensor matrix with 3 GPIO pins |
| Logic Level Converter | SN74AHCT125N (recommended) or BSS138 | 1 | Shifts 3.3V ESP32 signals to 5V for LEDs, shift register, and transistors |

## Transistors & Resistors

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| NPN Transistors | 2N2222 | 8 | Powers one column of 8 sensors each (alternatively: 2N2907 PNP) |
| Pull-down Resistors | 10kΩ | 8 | Pull-down on sensor row GPIO inputs |
| Base Resistors | 1kΩ | 8 | Current limiting for transistor bases |

Either NPN (2N2222) or PNP (2N2907) transistors can be used depending on the circuit design. The firmware supports both configurations through calibration.

## Mechanical Components

| Component | Specification | Quantity | Purpose |
|-----------|--------------|----------|---------|
| Iron Discs | 12×1mm | 64 | Placed under each square to spread the magnetic field — required for reliable sensor detection and to prevent pieces from sticking to each other |
| Neodymium Magnets | 8×4mm | 32 | Embedded in chess piece bases (one per piece). Alternatively: 8×2mm × 64 (two stacked per piece, as single 8×2mm magnets are too weak) |
| USB-C Female Port | Panel mount | 1 | Power injection point for the LED strip at different positions |
| Perfboard | 5×7 cm | 1 | Mounting surface for the ESP32, shift register, transistors, and resistors |
| Solid Core Wire | 26 AWG, insulated | ~10 meters | Wiring connections between components (sensor rows/columns, power, signals) |
| Electrical Tape | Insulating | 1 roll | Insulating exposed connections and securing wires |
| Solder Wire | 60/40 or lead-free | 1 spool | Soldering all wire and component connections |

## Notes

- **ESP32 choice** — any standard ESP32-WROOM-32 development board works. The firmware uses GPIO pins that are available on all common variants. Avoid boards with limited pin breakouts.
- **LED strip length** — 30 LEDs/m × 3 meters gives 90 LEDs total, of which 64 are used (one per square). Extra LEDs at the strip ends are left unconnected.
- **Magnet sizing** — 8×4mm magnets are strongly recommended over 8×2mm. The 8×2mm variant requires stacking two magnets per piece to generate sufficient field strength for the A3144 sensors.
- **Iron discs** — these are not optional. Without them, the A3144 sensors have a very narrow detection cone and adjacent magnets attract each other through the board.
- **Power supply** — 64 WS2812B LEDs at full brightness can draw up to ~3.8A at 5V. A USB power supply rated for at least 2A is recommended, with power injection via the USB-C port to prevent voltage drop along the strip.