#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include "led_colors.h"

namespace SystemUtils {

/// Map a player color character ('w'/'b') to an LED color.
LedRGB colorLed(char color);

/// Print the board to Serial for debugging.
void printBoard(const char board[8][8]);

/// Initialize NVS for ESP32 (required before Preferences.begin).
bool ensureNvsInitialized();

} // namespace SystemUtils

#endif // SYSTEM_UTILS_H
