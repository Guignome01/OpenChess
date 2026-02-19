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

int BoardMenu::trySelect(SelectorState& state, int8_t row, int8_t col, LedRGB color, int id) {
  int8_t r = transformRow(row);
  int8_t c = transformCol(col);
  bool occupied = bd_->getSensorState(r, c);
  if (updateDebounce(state, occupied)) {
    bd_->blinkSquare(r, c, color, 1);
    bd_->waitForAnimationQueueDrain();
    // Wait for piece removal so the next menu starts with a clean square
    while (bd_->getSensorState(r, c)) {
      bd_->readSensors();
      delay(SENSOR_READ_DELAY_MS);
    }
    return id;
  }
  return RESULT_NONE;
}

int BoardMenu::poll() {
  for (uint8_t i = 0; i < itemCount_; ++i) {
    int result = trySelect(states_[i], items_[i].row, items_[i].col, items_[i].color, items_[i].id);
    if (result != RESULT_NONE)
      return result;
  }
  if (hasBack_) {
    int result = trySelect(states_[itemCount_], backRow_, backCol_, BACK_BUTTON_COLOR, RESULT_BACK);
    if (result != RESULT_NONE)
      return result;
  }
  return RESULT_NONE;
}

int BoardMenu::waitForSelection() {
  reset();
  show();
  while (true) {
    bd_->readSensors();
    int result = poll();
    if (result != RESULT_NONE) {
      hide();
      return result;
    }
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
