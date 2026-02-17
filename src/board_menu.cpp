#include "board_menu.h"
#include <string.h> // memset

// Two-phase debounce: square must be empty for DEBOUNCE_CYCLES, then
// occupied for DEBOUNCE_CYCLES, to count as a valid selection.
static constexpr uint8_t DEBOUNCE_CYCLES = (DEBOUNCE_MS / SENSOR_READ_DELAY_MS) + 2;

// Back button LED color
static constexpr LedRGB BACK_BUTTON_COLOR = LedColors::White;

// ---------------------------
// BoardMenu
// ---------------------------

BoardMenu::BoardMenu(BoardDriver* bd)
    : bd_(bd),
      items_(nullptr),
      itemCount_(0),
      flipped_(false),
      hasBack_(false),
      backRow_(0),
      backCol_(0) {
  memset(states_, 0, sizeof(states_));
}

void BoardMenu::setItems(const MenuItem* items, uint8_t count) {
  items_ = items;
  itemCount_ = (count > MAX_ITEMS) ? MAX_ITEMS : count;
}

void BoardMenu::setBackButton(int8_t row, int8_t col) {
  hasBack_ = true;
  backRow_ = row;
  backCol_ = col;
}

void BoardMenu::setFlipped(bool flipped) {
  flipped_ = flipped;
}

void BoardMenu::show() {
  BoardDriver::LedGuard guard(bd_);
  bd_->clearAllLEDs(false);
  for (uint8_t i = 0; i < itemCount_; ++i) {
    bd_->setSquareLED(transformRow(items_[i].row), transformCol(items_[i].col), items_[i].color);
  }
  if (hasBack_) {
    bd_->setSquareLED(transformRow(backRow_), transformCol(backCol_), BACK_BUTTON_COLOR);
  }
  bd_->showLEDs();
}

void BoardMenu::hide() {
  BoardDriver::LedGuard guard(bd_);
  bd_->clearAllLEDs(false);
  bd_->showLEDs();
}

void BoardMenu::reset() {
  memset(states_, 0, sizeof(states_));
}

int8_t BoardMenu::transformRow(int8_t row) const {
  return flipped_ ? (7 - row) : row;
}

int8_t BoardMenu::transformCol(int8_t col) const {
  // Vertical flip only — col unchanged.
  // Change to (7 - col) for full 180° rotation if needed after visual testing.
  return col;
}

bool BoardMenu::updateDebounce(SelectorState& state, bool occupied) {
  if (!occupied) {
    // Phase 1: counting consecutive empty readings
    if (state.emptyCount < DEBOUNCE_CYCLES)
      state.emptyCount++;
    state.occupiedCount = 0;
    if (state.emptyCount >= DEBOUNCE_CYCLES)
      state.readyForSelection = true;
  } else {
    // Phase 2: counting consecutive occupied readings (only after phase 1 passed)
    state.emptyCount = 0;
    if (state.readyForSelection) {
      if (state.occupiedCount < DEBOUNCE_CYCLES)
        state.occupiedCount++;
    } else {
      state.occupiedCount = 0;
    }
  }
  return state.readyForSelection && state.occupiedCount >= DEBOUNCE_CYCLES;
}

int BoardMenu::poll() {
  // Check menu items
  for (uint8_t i = 0; i < itemCount_; ++i) {
    int8_t r = transformRow(items_[i].row);
    int8_t c = transformCol(items_[i].col);
    bool occupied = bd_->getSensorState(r, c);
    if (updateDebounce(states_[i], occupied)) {
      // Selection confirmed — blink feedback in the item's color
      bd_->blinkSquare(r, c, items_[i].color, 1);
      return items_[i].id;
    }
  }

  // Check back button
  if (hasBack_) {
    int8_t r = transformRow(backRow_);
    int8_t c = transformCol(backCol_);
    bool occupied = bd_->getSensorState(r, c);
    if (updateDebounce(states_[itemCount_], occupied)) {
      bd_->blinkSquare(r, c, BACK_BUTTON_COLOR, 1);
      return RESULT_BACK;
    }
  }

  return RESULT_NONE;
}

int BoardMenu::waitForSelection() {
  reset();
  show();
  while (true) {
    bd_->readSensors();
    int result = poll();
    if (result != RESULT_NONE)
      return result;
    delay(SENSOR_READ_DELAY_MS);
  }
}

// ---------------------------
// boardConfirm
// ---------------------------

bool boardConfirm(BoardDriver* bd, bool flipped) {
  static constexpr MenuItem confirmItems[] = {
      {4, 3, LedColors::Green, 1}, // Yes — d4
      {4, 4, LedColors::Red, 0},   // No  — e4
  };

  BoardMenu menu(bd);
  menu.setItems(confirmItems, 2);
  menu.setFlipped(flipped);
  return menu.waitForSelection() == 1;
}
