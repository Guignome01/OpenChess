# Assembly

Instructions for assembling a LibreChess smart chessboard. This guide covers the general construction approach. A more comprehensive assembly guide is planned for a future update — see the [roadmap](../roadmap.md).

## Overview

The assembly involves four main stages:
1. Preparing the LED grid
2. Installing the sensor matrix
3. Wiring the electronics
4. Preparing the chess pieces

## 3D-Printed Board

The chessboard requires a 3D-printed enclosure that is **not included in this repository**. The print files must be downloaded from an external source before starting assembly:

**[OpenChess 3D Print Files on MakerWorld](https://makerworld.com/models/1953392-openchess-plastic-pcb)**

Print all required parts before proceeding. The enclosure provides the board frame with slots for sensor and LED placement, and a base to house the electronics.

## LED Strip Routing

The WS2812B LED strip is routed in a serpentine (zigzag) pattern across the 8×8 grid:

```
Row 0: → → → → → → → →
Row 1: ← ← ← ← ← ← ← ←
Row 2: → → → → → → → →
...
Row 7: ← ← ← ← ← ← ← ←
```

- Cut the strip at every 8th LED to create 8 segments
- Arrange segments in alternating directions (right-to-left, left-to-right)
- Solder data connections between the end of one segment and the start of the next
- The first LED in the strip corresponds to a board corner — the exact mapping is handled by calibration

Ensure each LED is centered under its corresponding square. The strip spacing (30 LEDs/m) gives approximately 33mm between LEDs, which works well for standard chess board square sizes.

## Sensor Placement

Each of the 64 squares gets one A3144 hall-effect sensor:

- Orient all sensors consistently (the flat face or marked side should face the same direction)
- The sensor should be positioned at the center of each square, directly above the LED
- Solder column power lines and row signal lines according to the wiring diagram in the [wiring guide](wiring.md)
- The 3D-printed board has slots on each square — use these to seat and align sensors securely

The A3144 is a unipolar switch — it activates when the south pole of a magnet approaches the branded face. Ensure magnets in chess pieces are oriented with the correct polarity.

## Iron Disc Placement

Place one 12×1mm iron disc directly under each square, between the sensor and the board surface:

- The disc spreads the magnetic field from the chess piece magnet, giving the sensor a wider detection area
- Without discs, detection is unreliable — the A3144 has a narrow activation cone
- Discs also prevent adjacent magnets from attracting each other through the board surface
- Secure discs with glue to prevent shifting

## Electronics Assembly

Wire the ESP32, shift register, transistors, and resistors according to the [wiring guide](wiring.md):

1. Connect the ESP32's 3 control signals (SER, SRCLK, RCLK) through the logic level converter (3.3V → 5V), then to the corresponding shift register input pins. The shift register operates at 5V and cannot be driven directly from the ESP32's 3.3V GPIOs.
2. Wire each shift register output to a transistor base through a 1kΩ resistor. Each transistor switches the 5V supply for one column of 8 hall-effect sensors.
3. Connect the 8 hall-effect sensor row signal lines to the ESP32 GPIO inputs, each with a 10kΩ pull-down resistor.
4. Connect the LED strip data line from ESP32 GPIO 32 through a logic level converter channel (3.3V → 5V) to the strip's data input.
5. Install the USB-C port on the board enclosure and wire to the ESP32 (VIN and GND pins). The USB-C port will be used to inject power across the board. 
6. Verify all power and ground connections. The 5V rail must reach the LED strip, the shift register VCC, the logic level converter VCC, and the transistor supply lines. Ground must be common across the ESP32, LED strip, shift register, logic level converter, and all transistors.

## Chess Piece Preparation

Embed one neodymium magnet in the base of each chess piece:

- **8×4mm magnets** (recommended) — one per piece, 32 total
- **8×2mm magnets** (alternative) — stack two per piece, 64 total. Single 8×2mm magnets are too weak for reliable detection.
- Ensure all magnets are oriented with the same polarity facing outward (south pole toward the board for A3144 sensors)
- Secure magnets with adhesive in a recessed hole in the piece base
- Test each piece on the assembled board before finalizing

## Post-Assembly

After assembly:
1. Flash the firmware (see [installation guide](../development/installation.md))
2. Run the calibration process on first boot (see [calibration guide](../guides/calibration.md))
3. Use the sensor test mode to verify all 64 squares detect properly (see [troubleshooting](../troubleshooting.md))
