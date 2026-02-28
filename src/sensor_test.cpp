#include "sensor_test.h"
#include <Arduino.h>

SensorTest::SensorTest(BoardDriver* bd) : boardDriver(bd), complete(false) {
  memset(visited, false, sizeof(visited));
}

void SensorTest::begin() {
  Serial.println("Sensor Test: Visit all squares with a piece to complete the test!");
  complete = false;
  memset(visited, false, sizeof(visited));
  boardDriver->clearAllLEDs();
}

void SensorTest::update() {
  if (complete) return;

  boardDriver->readSensors();
  boardDriver->clearAllLEDs(false);

  int visitedCount = 0;
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (boardDriver->getSensorState(row, col))
        visited[row][col] = true;
      if (visited[row][col]) {
        boardDriver->setSquareLED(row, col, LedColors::White);
        visitedCount++;
      }
    }
  }
  boardDriver->showLEDs();
  if (visitedCount == 64) {
    complete = true;
    Serial.println("Sensor Test complete! All squares verified");
    boardDriver->fireworkAnimation();
  }
}