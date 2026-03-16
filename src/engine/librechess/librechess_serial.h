#ifndef LIBRECHESS_SERIAL_H
#define LIBRECHESS_SERIAL_H

#include <Arduino.h>

#include "uci.h"

// ---------------------------------------------------------------------------
// SerialUCIStream — UCIStream over the hardware UART (Serial).
//
// Allows external UCI GUIs (Arena, CuteChess, etc.) to drive the engine
// over the ESP32's USB-UART bridge.  Activated via build flag or command
// detection in main.cpp.
//
// readLine()  blocks until a full line is available on Serial.
// writeLine() writes the line + newline to Serial.
// ---------------------------------------------------------------------------

class SerialUCIStream : public LibreChess::uci::UCIStream {
 public:
  std::string readLine() override {
    // Block until a complete line arrives
    while (!Serial.available()) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    String line = Serial.readStringUntil('\n');
    line.trim();  // Remove \r and trailing whitespace
    return std::string(line.c_str());
  }

  void writeLine(const std::string& line) override {
    Serial.println(line.c_str());
  }
};

#endif  // LIBRECHESS_SERIAL_H
