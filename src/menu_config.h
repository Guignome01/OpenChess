#ifndef MENU_CONFIG_H
#define MENU_CONFIG_H

#include "board_menu.h"
#include "menu_navigator.h"

// ---------------------------
// Menu Item IDs
// ---------------------------
// Distinct ranges per menu level for easy routing in handleMenuResult().
namespace MenuId {
  // Game selection (root)
  constexpr int8_t CHESS_MOVES = 0;
  constexpr int8_t BOT         = 1;
  constexpr int8_t LICHESS     = 2;
  constexpr int8_t SENSOR_TEST = 3;

  // Bot difficulty
  constexpr int8_t EASY   = 10;
  constexpr int8_t MEDIUM = 11;
  constexpr int8_t HARD   = 12;
  constexpr int8_t EXPERT = 13;

  // Bot color
  constexpr int8_t PLAY_WHITE  = 20;
  constexpr int8_t PLAY_BLACK  = 21;
  constexpr int8_t PLAY_RANDOM = 22;
}

// ---------------------------
// Menu Item Layouts
// ---------------------------
// Coordinates are in white-side orientation (row 7 = rank 1).
// All arrays are constexpr — stored in flash, zero RAM cost.

static constexpr MenuItem gameMenuItems[] = {
    {3, 3, LedColors::Blue,   MenuId::CHESS_MOVES}, // Chess Moves (Human vs Human)
    {3, 4, LedColors::Green,  MenuId::BOT},          // Chess Bot (Human vs AI)
    {4, 3, LedColors::Yellow, MenuId::LICHESS},       // Lichess (Online play)
    {4, 4, LedColors::Red,    MenuId::SENSOR_TEST},   // Sensor Test
};

static constexpr MenuItem botDifficultyItems[] = {
    {3, 2, LedColors::Green,  MenuId::EASY},
    {3, 3, LedColors::Yellow, MenuId::MEDIUM},
    {3, 4, LedColors::Red,    MenuId::HARD},
    {3, 5, LedColors::Purple, MenuId::EXPERT},
};

static constexpr MenuItem botColorItems[] = {
    {3, 3, LedColors::White,  MenuId::PLAY_WHITE},
    {3, 4, {40, 40, 40},      MenuId::PLAY_BLACK},  // Dim white = black side
    {3, 5, LedColors::Yellow, MenuId::PLAY_RANDOM},
};

// ---------------------------
// Menu Instances & Navigator
// ---------------------------
// Declared extern here, defined in menu_config.cpp.
// Menus are file-scoped statics with no heap allocation.

extern BoardMenu gameMenu;
extern BoardMenu botDifficultyMenu;
extern BoardMenu botColorMenu;
extern MenuNavigator navigator;

/// Initialize all menus (set items, back buttons). Call once in setup().
void initMenus(BoardDriver* bd);

#endif // MENU_CONFIG_H
