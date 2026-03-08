#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include "led_colors.h"

namespace SystemUtils {

/// Map a player color character ('w'/'b') to an LED color.
LedRGB colorLed(char color);

/// Initialize NVS for ESP32 (required before Preferences.begin).
bool ensureNvsInitialized();

} // namespace SystemUtils

#endif // SYSTEM_UTILS_H
