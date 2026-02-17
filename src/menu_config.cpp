#include "menu_config.h"

BoardMenu gameMenu;
BoardMenu botDifficultyMenu;
BoardMenu botColorMenu;
MenuNavigator navigator;

void initMenus(BoardDriver* bd) {
    gameMenu.setBoardDriver(bd);
    botDifficultyMenu.setBoardDriver(bd);
    botColorMenu.setBoardDriver(bd);
    navigator.setBoardDriver(bd);

    gameMenu.setItems(gameMenuItems);
    botDifficultyMenu.setItems(botDifficultyItems);
    botDifficultyMenu.setBackButton(4, 4);
    botColorMenu.setItems(botColorItems);
    botColorMenu.setBackButton(4, 4);
}
