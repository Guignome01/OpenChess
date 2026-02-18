#ifndef BOARD_MENU_H
#define BOARD_MENU_H

#include "board_driver.h"
#include "led_colors.h"
#include <stdint.h>

// ---------------------------
// Board Menu System
// ---------------------------
// Reusable menu primitive for the 8x8 LED board.
// Items are placed freely on the grid. Selection uses two-phase
// debounce (empty → occupied) for reliable piece-placement detection.
// Supports orientation flipping so menus face the active player.

/// A single selectable option on the board.
/// Coordinates are authored in white-side orientation (row 7 = rank 1 = white's back rank).
struct MenuItem {
  int8_t row;
  int8_t col;
  LedRGB color;
  int8_t id; // Unique identifier returned on selection
};

/// Reusable board menu with two-phase debounce selection.
/// All state is stack-allocated — no heap usage.
class BoardMenu {
 public:
  static constexpr int RESULT_NONE = -1;
  static constexpr int RESULT_BACK = -2;
  static constexpr int MAX_ITEMS = 16;

  BoardMenu() : bd_(nullptr), items_(nullptr), itemCount_(0),
    flipped_(false), hasBack_(false), backRow_(0), backCol_(0), states_{} {}
  explicit BoardMenu(BoardDriver* bd);

  // Delete copy — menus should not be duplicated
  BoardMenu(const BoardMenu&) = delete;
  BoardMenu& operator=(const BoardMenu&) = delete;

  /// Set or change the BoardDriver pointer (for two-phase init).
  void setBoardDriver(BoardDriver* bd) { bd_ = bd; }

  /// Configure menu options. Items pointer must outlive the menu
  /// (use constexpr file-scoped arrays). Does NOT copy the array.
  void setItems(const MenuItem* items, uint8_t count);

  /// Convenience: configure items with automatic count deduction.
  template <uint8_t N>
  void setItems(const MenuItem (&items)[N]) { setItems(items, N); }

  /// Designate a corner/edge square as a back button (lit with LedColors::White).
  /// Omit for root menus that have no parent.
  void setBackButton(int8_t row, int8_t col);

  /// Set orientation. When true, coordinates are vertically mirrored
  /// (row' = 7 - row) so the menu faces a player on the black side.
  /// Defaults to false (white-side / standard orientation).
  void setFlipped(bool flipped);

  /// Light all menu items and back button on the board.
  /// Acquires LedGuard for the minimum duration needed.
  void show();

  /// Clear all LEDs. Acquires LedGuard briefly.
  void hide();

  /// Reset all debounce counters for a fresh selection cycle.
  void reset();

  /// Non-blocking poll. Call after boardDriver->readSensors().
  /// Returns:
  ///   - MenuItem::id on confirmed selection (with blink feedback)
  ///   - RESULT_BACK if back button selected
  ///   - RESULT_NONE if no selection yet
  int poll();

  /// Blocking convenience: reset() → show() → poll loop → return id.
  /// Includes delay(SENSOR_READ_DELAY_MS) per iteration for watchdog safety.
  /// Does NOT auto-hide — caller controls LED state after selection.
  int waitForSelection();

 private:
  struct SelectorState {
    uint8_t emptyCount;
    uint8_t occupiedCount;
    bool readyForSelection;
  };

  BoardDriver* bd_;
  const MenuItem* items_;
  uint8_t itemCount_;
  bool flipped_;
  bool hasBack_;
  int8_t backRow_;
  int8_t backCol_;

  // +1 slot for the back button
  SelectorState states_[MAX_ITEMS + 1];

  int8_t transformRow(int8_t row) const;
  int8_t transformCol(int8_t col) const;

  /// Apply two-phase debounce to a single square.
  /// Returns true when the occupied-after-empty transition is confirmed.
  bool updateDebounce(SelectorState& state, bool occupied);

  /// Check one square for a confirmed selection. Handles debounce,
  /// blink feedback, and waiting for piece removal.
  /// Returns the given id on selection, or RESULT_NONE.
  int trySelect(SelectorState& state, int8_t row, int8_t col, LedRGB color, int id);
};

/// Blocking yes/no confirmation dialog.
/// Shows two center squares (green = yes, red = no), waits for selection.
/// Returns true for yes, false for no.
bool boardConfirm(BoardDriver* bd, bool flipped = false);

#endif // BOARD_MENU_H
