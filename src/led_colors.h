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
static constexpr LedRGB Red{255, 0, 0};       // capture/attack/error/invalid move
static constexpr LedRGB Purple{128, 0, 255};  // en passant, expert mode
static constexpr LedRGB Green{0, 255, 0};     // confirm/move completion
static constexpr LedRGB Gold{255, 215, 0};    // king in check/promotion/special
static constexpr LedRGB Orange{255, 165, 0};  // Chess Moves mode
static constexpr LedRGB Blu{0, 0, 255};       // bot thinking
static constexpr LedRGB Off{0, 0, 0};         // turn off LED
} // namespace LedColors

#endif // LED_COLORS_H
