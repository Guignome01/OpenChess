#include "system_utils.h"

#include <Arduino.h>

extern "C" {
#include "nvs_flash.h"
}

LedRGB SystemUtils::colorLed(char color) {
  return (color == 'w') ? LedColors::White : ((color == 'b') ? LedColors::Blue : LedColors::Off);
}

void SystemUtils::printBoard(const char board[8][8]) {
  Serial.println("====== BOARD ======");
  for (int row = 0; row < 8; row++) {
    String rowStr = String(8 - row) + " ";
    for (int col = 0; col < 8; col++) {
      char piece = board[row][col];
      rowStr += (piece == ' ') ? String(". ") : String(piece) + " ";
    }
    rowStr += " " + String(8 - row);
    Serial.println(rowStr);
  }
  Serial.println("  a b c d e f g h");
  Serial.println("===================");
}

bool SystemUtils::ensureNvsInitialized() {
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    Serial.printf("[NVS] Init failed with error 0x%x, erasing and retrying...\n", err);
    nvs_flash_erase();
    err = nvs_flash_init();
    if (err == ESP_OK) {
      Serial.println("[NVS] Successfully initialized after erase");
    } else {
      Serial.printf("[NVS] Still failed after erase: 0x%x\n", err);
    }
  }
  return err == ESP_OK;
}
