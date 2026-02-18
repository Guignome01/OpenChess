#ifndef LED_COLORS_H
#define LED_COLORS_H

#include <stdint.h>

struct LedRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

namespace LedColors {
static constexpr LedRGB Cyan{0, 255, 255};    // piece original square
static constexpr LedRGB White{255, 255, 255}; // normal move
static constexpr LedRGB DimWhite{40, 40, 40}; // black side indicator
static constexpr LedRGB Red{255, 0, 0};       // capture/attack/error/invalid move
static constexpr LedRGB Purple{128, 0, 255};  // en passant, expert mode
static constexpr LedRGB Green{0, 255, 0};     // confirm/move completion
static constexpr LedRGB Lime{100, 200, 0};    // easy difficulty
static constexpr LedRGB Yellow{255, 200, 0};  // king in check/promotion
static constexpr LedRGB Orange{255, 80, 0};   // medium difficulty
static constexpr LedRGB Crimson{200, 0, 50};  // hard difficulty
static constexpr LedRGB Blue{0, 0, 255};      // bot thinking
static constexpr LedRGB Off{0, 0, 0};         // turn off LED
} // namespace LedColors

#endif // LED_COLORS_H
