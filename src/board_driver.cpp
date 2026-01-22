#include "board_driver.h"
#include "chess_utils.h"
#include "led_colors.h"
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

static constexpr int rowPins[NUM_ROWS] = {ROW_PIN_0, ROW_PIN_1, ROW_PIN_2, ROW_PIN_3, ROW_PIN_4, ROW_PIN_5, ROW_PIN_6, ROW_PIN_7};
// ---------------------------
// LED Strip Col/Row to Pixel index mapping (default)
// ---------------------------
static constexpr int DefaultRowColToLEDindexMap[NUM_ROWS][NUM_COLS] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {15, 14, 13, 12, 11, 10, 9, 8},
    {16, 17, 18, 19, 20, 21, 22, 23},
    {31, 30, 29, 28, 27, 26, 25, 24},
    {32, 33, 34, 35, 36, 37, 38, 39},
    {47, 46, 45, 44, 43, 42, 41, 40},
    {48, 49, 50, 51, 52, 53, 54, 55},
    {63, 62, 61, 60, 59, 58, 57, 56},
};

BoardDriver::BoardDriver() : strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800) {
  calibrationLoaded = false;
  swapAxes = 0;
  for (int i = 0; i < NUM_ROWS; i++)
    toLogicalRow[i] = i;
  for (int i = 0; i < NUM_COLS; i++)
    toLogicalCol[i] = i;
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++)
      ledIndexMap[row][col] = DefaultRowColToLEDindexMap[row][col];
}

void BoardDriver::begin() {
  // Initialize NeoPixel strip
  strip.begin();
  strip.show(); // turn off all pixels
  strip.setBrightness(BRIGHTNESS);
  // Shift register pins as outputs
  pinMode(SR_SER_DATA_PIN, OUTPUT);
  pinMode(SR_CLK_PIN, OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  disableAllCols();
  // Row pins as inputs
  for (int c = 0; c < NUM_ROWS; c++)
    pinMode(rowPins[c], INPUT);
  // Initialize sensor arrays
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++) {
      sensorState[row][col] = false;
      sensorPrev[row][col] = false;
      sensorRaw[row][col] = false;
      sensorDebounceTime[row][col] = 0;
    }

  // Load calibration or run first-time calibration
  if (!loadCalibration()) {
    runCalibration();
    saveCalibration();
  }
}

bool BoardDriver::loadCalibration() {
  if (!ChessUtils::ensureNvsInitialized()) {
    Serial.println("NVS init failed - calibration not loaded");
    return false;
  }
  Preferences prefs;
  prefs.begin("boardCal", true);
  uint8_t ver = prefs.getUChar("ver", 0);
  if (ver != 1) {
    prefs.end();
    return false;
  }

  // Verify pin configuration matches
  size_t rowPinsLen = prefs.getBytesLength("rowPins");
  if (rowPinsLen != NUM_ROWS) {
    prefs.end();
    return false;
  }
  uint8_t savedRowPins[NUM_ROWS];
  prefs.getBytes("rowPins", savedRowPins, sizeof(savedRowPins));
  for (int i = 0; i < NUM_ROWS; i++)
    if (savedRowPins[i] != (uint8_t)rowPins[i]) {
      prefs.end();
      return false;
    }
  uint8_t savedSRPins[3];
  prefs.getBytes("srPins", savedSRPins, sizeof(savedSRPins));
  if (savedSRPins[0] != (uint8_t)SR_CLK_PIN || savedSRPins[1] != (uint8_t)SR_LATCH_PIN || savedSRPins[2] != (uint8_t)SR_SER_DATA_PIN) {
    prefs.end();
    return false;
  }

  size_t rowLen = prefs.getBytesLength("row");
  size_t colLen = prefs.getBytesLength("col");
  size_t ledLen = prefs.getBytesLength("led");
  if (rowLen != NUM_ROWS || colLen != NUM_COLS || ledLen != LED_COUNT) {
    prefs.end();
    return false;
  }
  swapAxes = prefs.getUChar("swap", 0);
  prefs.getBytes("row", toLogicalRow, NUM_ROWS);
  prefs.getBytes("col", toLogicalCol, NUM_COLS);
  uint8_t ledFlat[LED_COUNT];
  prefs.getBytes("led", ledFlat, LED_COUNT);
  int idx = 0;
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++)
      ledIndexMap[row][col] = ledFlat[idx++];
  prefs.end();
  calibrationLoaded = true;
  Serial.println("Board calibration loaded from NVS");
  return true;
}

void BoardDriver::saveCalibration() {
  if (!ChessUtils::ensureNvsInitialized()) {
    Serial.println("NVS init failed - calibration not saved");
    return;
  }
  Preferences prefs;
  prefs.begin("boardCal", false);
  prefs.putUChar("ver", 1);
  uint8_t rowPinsU8[NUM_ROWS];
  for (int i = 0; i < NUM_ROWS; i++)
    rowPinsU8[i] = (uint8_t)rowPins[i];
  prefs.putBytes("rowPins", rowPinsU8, sizeof(rowPinsU8));
  uint8_t srPins[3] = {(uint8_t)SR_CLK_PIN, (uint8_t)SR_LATCH_PIN, (uint8_t)SR_SER_DATA_PIN};
  prefs.putBytes("srPins", srPins, sizeof(srPins));
  prefs.putUChar("swap", swapAxes);
  prefs.putBytes("row", toLogicalRow, NUM_ROWS);
  prefs.putBytes("col", toLogicalCol, NUM_COLS);
  uint8_t ledFlat[LED_COUNT];
  int idx = 0;
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++)
      ledFlat[idx++] = ledIndexMap[row][col];
  prefs.putBytes("led", ledFlat, LED_COUNT);
  prefs.end();
  calibrationLoaded = true;
  Serial.println("Board calibration saved to NVS");
}

void BoardDriver::readRawSensors(bool rawState[NUM_ROWS][NUM_COLS]) {
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++)
      rawState[row][col] = false;

  for (int col = 0; col < NUM_COLS; col++) {
    enableCol(col);
    for (int row = 0; row < NUM_ROWS; row++)
      rawState[row][col] = (digitalRead(rowPins[row]) == LOW);
  }
  disableAllCols();
}

bool BoardDriver::waitForBoardEmpty() {
  bool rawState[NUM_ROWS][NUM_COLS];
  while (true) {
    readRawSensors(rawState);
    bool any = false;
    for (int row = 0; row < NUM_ROWS; row++) {
      for (int col = 0; col < NUM_COLS; col++)
        if (rawState[row][col]) {
          any = true;
          break;
        }
      if (any)
        break;
    }
    if (!any)
      return true;
    delay(SENSOR_READ_DELAY_MS);
  }
}

bool BoardDriver::waitForSingleRawPress(int& rawRow, int& rawCol, unsigned long stableMs) {
  bool rawState[NUM_ROWS][NUM_COLS];
  int lastRow = -1;
  int lastCol = -1;
  unsigned long stableStart = 0;

  while (true) {
    readRawSensors(rawState);
    int count = 0;
    int foundRow = -1;
    int foundCol = -1;
    for (int row = 0; row < NUM_ROWS; row++)
      for (int col = 0; col < NUM_COLS; col++)
        if (rawState[row][col]) {
          count++;
          foundRow = row;
          foundCol = col;
        }
    if (count == 1) {
      if (foundRow == lastRow && foundCol == lastCol) {
        if (stableStart == 0)
          stableStart = millis();
        if (millis() - stableStart >= stableMs) {
          rawRow = foundRow;
          rawCol = foundCol;
          return true;
        }
      } else {
        lastRow = foundRow;
        lastCol = foundCol;
        stableStart = 0;
      }
    } else {
      lastRow = -1;
      lastCol = -1;
      stableStart = 0;
    }
    delay(SENSOR_READ_DELAY_MS);
  }
}

void BoardDriver::showCalibrationError() {
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, strip.Color(LedColors::Red.r, LedColors::Red.g, LedColors::Red.b));
  strip.show();
  waitForBoardEmpty();
  clearAllLEDs();
}

bool BoardDriver::calibrateAxis(Axis axis, uint8_t* axisPinsOrder, size_t NUM_PINS, bool firstAxisSwapped) {
  if ((NUM_ROWS != NUM_COLS) || (NUM_ROWS != NUM_PINS)) {
    Serial.println("Non-square boards not supported for calibration");
    return false;
  }
  Axis detectedAxis = UnknownAxis;
  int firstRow = -1;
  int firstCol = -1;
  uint8_t counts[NUM_PINS] = {0};
  for (int i = 0; i < NUM_PINS; i++)
    axisPinsOrder[i] = -1;

  // If this is column calibration and we already have row mapping, find expected raw row/col for rank 1
  int expectedRawPin = -1;
  bool useRow = true; // Whether to check row or col during column calibration
  if (axis == ColsAxis)
    for (int i = 0; i < NUM_ROWS; i++)
      if (toLogicalRow[i] == 7) {
        expectedRawPin = i;
        // If first axis swapped, toLogicalRow is indexed by raw cols, so we check col
        useRow = !firstAxisSwapped;
        break;
      }

  for (int i = 0; i < NUM_PINS; i++) {
    char square[3];
    if (axis == RowsAxis) {
      square[0] = 'a';
      square[1] = (char)('8' - i);
    } else {
      square[0] = (char)('a' + i);
      square[1] = '1';
    }
    square[2] = '\0';

    Serial.printf("Place a piece on %s (%s calibration)\n", square, getAxisString(axis).c_str());
    int row = 0;
    int col = 0;
    waitForSingleRawPress(row, col);

    // Verify pin consistency for column calibration
    if (axis == ColsAxis && expectedRawPin != -1) {
      int actualPin = useRow ? row : col;
      if (actualPin != expectedRawPin) {
        Serial.printf("Error: Expected piece on %s pin %d but detected on %d. Place piece on %s.\n", useRow ? "row" : "col", expectedRawPin, actualPin, square);
        showCalibrationError();
        i--;
        continue;
      }
    }

    if (i == 0) {
      firstRow = row;
      firstCol = col;
      Serial.println("Remove the piece");
      waitForBoardEmpty();
      continue;
    }

    if (detectedAxis == UnknownAxis && i == 1) {
      if (row == firstRow && col != firstCol) {
        detectedAxis = ColsAxis;
        axisPinsOrder[firstCol] = i - 1;
        counts[firstCol]++;
        Serial.printf("%s calibration using cols %s\n", getAxisString(axis).c_str(), axis != detectedAxis ? "(axis swap)" : "(no axis swap)");
      } else if (col == firstCol && row != firstRow) {
        detectedAxis = RowsAxis;
        axisPinsOrder[firstRow] = i - 1;
        counts[firstRow]++;
        Serial.printf("%s calibration using rows %s\n", getAxisString(axis).c_str(), axis != detectedAxis ? "(axis swap)" : "(no axis swap)");
      } else {
        Serial.printf("Ambiguous %s calibration (first two squares not aligned). Retry.\n", getAxisString(axis).c_str());
        showCalibrationError();
        i = -1;
        continue;
      }
    }

    if (detectedAxis == UnknownAxis) {
      // Will never happen due to above logic, but just in case
      Serial.printf("Ambiguous %s calibration (no orientation detected). Retry.\n", getAxisString(axis).c_str());
      showCalibrationError();
      i = -1;
      continue;
    }

    int pin = (detectedAxis == RowsAxis) ? row : col;
    if (counts[pin] > 0) {
      Serial.printf("Duplicate %s detected with pin %d. Retry %s.\n", getAxisString(axis).c_str(), pin, square);
      showCalibrationError();
      i--;
      continue;
    }

    axisPinsOrder[pin] = i;
    counts[pin]++;

    Serial.println("Remove the piece");
    waitForBoardEmpty();
  }

  return axis != detectedAxis;
}

void BoardDriver::runCalibration() {
  Serial.println("============== Board calibration required ==============");
  Serial.println("- Board needs to be empty to begin the calibration");
  Serial.println("- Follow the prompts to place a single piece");
  Serial.println("========================================================");

  clearAllLEDs();
  waitForBoardEmpty();

  // Calibration animation - light up each pixel sequentially
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(LedColors::White.r, LedColors::White.g, LedColors::White.b));
    strip.show();
    delay(50);
  }
  delay(500);
  clearAllLEDs();

  bool swapAxes1 = calibrateAxis(Axis::RowsAxis, toLogicalRow, NUM_ROWS, false);
  bool swapAxes2 = calibrateAxis(Axis::ColsAxis, toLogicalCol, NUM_COLS, swapAxes1);
  if (swapAxes1 != swapAxes2) {
    Serial.println("Inconsistent axis orientation detected during calibration. Restarting calibration.");
    showCalibrationError();
    runCalibration();
    return;
  }
  swapAxes = swapAxes1 ? 1 : 0;

  // LED mapping calibration
  Serial.println("LED mapping calibration:");
  Serial.println("A single LED will light. Place a piece on that square.");

  bool logicalUsed[NUM_ROWS][NUM_COLS] = {false};

  auto displayCalibrationLEDs = [&](int currentPixel) {
    for (int i = 0; i < LED_COUNT; i++)
      strip.setPixelColor(i, 0);
    for (int r = 0; r < NUM_ROWS; r++)
      for (int c = 0; c < NUM_COLS; c++)
        if (logicalUsed[r][c])
          strip.setPixelColor(ledIndexMap[r][c], strip.Color(LedColors::Green.r, LedColors::Green.g, LedColors::Green.b));
    if (currentPixel < LED_COUNT)
      strip.setPixelColor(currentPixel, strip.Color(LedColors::White.r, LedColors::White.g, LedColors::White.b));
    strip.show();
  };

  for (int pixelIndex = 0; pixelIndex < LED_COUNT; pixelIndex++) {
    int row = 0;
    int col = 0;
    displayCalibrationLEDs(pixelIndex);
    waitForSingleRawPress(row, col);

    uint8_t logicalRow = toLogicalRow[swapAxes ? col : row];
    uint8_t logicalCol = toLogicalCol[swapAxes ? row : col];
    if (logicalUsed[logicalRow][logicalCol]) {
      Serial.printf("Duplicate square %c%c detected. Retry LED %d.\n", (char)('a' + logicalCol), (char)('8' - logicalRow), pixelIndex);
      showCalibrationError();
      pixelIndex--;
      continue;
    }
    logicalUsed[logicalRow][logicalCol] = true;
    ledIndexMap[logicalRow][logicalCol] = pixelIndex;
    Serial.printf("  LED %d -> %c%c\n", pixelIndex, (char)('a' + logicalCol), (char)('8' - logicalRow));

    displayCalibrationLEDs(pixelIndex + 1);

    Serial.println("Remove the piece");
    waitForBoardEmpty();
  }

  clearAllLEDs();
  Serial.println("Calibration complete");
}

void BoardDriver::loadShiftRegister(byte data) {
  digitalWrite(SR_LATCH_PIN, LOW);
  for (int i = 7; i >= 0; i--) {
    digitalWrite(SR_SER_DATA_PIN, !!(data & (1 << i)));
    digitalWrite(SR_CLK_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SR_CLK_PIN, LOW);
    delayMicroseconds(10);
  }
  digitalWrite(SR_LATCH_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SR_LATCH_PIN, LOW);
}

void BoardDriver::disableAllCols() {
  loadShiftRegister(0);
}

void BoardDriver::enableCol(int col) {
  loadShiftRegister(((byte)((1 << (col)) & 0xFF)));
  delayMicroseconds(100); // Allow time for the column to stabilize, otherwise random readings occur
}

void BoardDriver::readSensors() {
  unsigned long currentTime = millis();

  for (int col = 0; col < NUM_COLS; col++) {
    enableCol(col);
    for (int row = 0; row < NUM_ROWS; row++) {
      bool newReading = digitalRead(rowPins[row]) == LOW;
      uint8_t logicalRow = toLogicalRow[swapAxes ? col : row];
      uint8_t logicalCol = toLogicalCol[swapAxes ? row : col];
      // Debounce logic
      if (newReading != sensorState[logicalRow][logicalCol]) {
        if (newReading != sensorRaw[logicalRow][logicalCol]) {
          sensorRaw[logicalRow][logicalCol] = newReading;
          sensorDebounceTime[logicalRow][logicalCol] = currentTime;
        } else if (currentTime - sensorDebounceTime[logicalRow][logicalCol] >= DEBOUNCE_MS) {
          sensorState[logicalRow][logicalCol] = newReading;
        }
      } else {
        sensorRaw[logicalRow][logicalCol] = newReading;
        sensorDebounceTime[logicalRow][logicalCol] = currentTime;
      }
    }
  }
  disableAllCols();
}

bool BoardDriver::getSensorState(int row, int col) {
  return sensorState[row][col];
}

bool BoardDriver::getSensorPrev(int row, int col) {
  return sensorPrev[row][col];
}

void BoardDriver::updateSensorPrev() {
  for (int row = 0; row < NUM_ROWS; row++)
    for (int col = 0; col < NUM_COLS; col++)
      sensorPrev[row][col] = sensorState[row][col];
}

int BoardDriver::getPixelIndex(int row, int col) {
  return ledIndexMap[row][col];
}

void BoardDriver::clearAllLEDs() {
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}

void BoardDriver::setSquareLED(int row, int col, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  uint32_t color;
  float multiplier = 1.0f;
  int pixelIndex = getPixelIndex(row, col);
  if ((row + col) % 2 == 1)
    multiplier = 0.7f; // Dim dark squares to 70% brightness because they appear brighter due to the contrast
  (w > 0 && ((r == 0 && g == 0 && b == 0) || (r == 255 && g == 255 && b == 255))) ? color = strip.Color(255 * multiplier, 255 * multiplier, 255 * multiplier) : color = strip.Color(r * multiplier, g * multiplier, b * multiplier);
  strip.setPixelColor(pixelIndex, color);
}

void BoardDriver::showLEDs() {
  strip.show();
}

void BoardDriver::showConnectingAnimation() {
  // Show each WiFi connection attempt with animated LEDs
  for (int i = 0; i < 8; i++) {
    setSquareLED(3, i, LedColors::Blu.r, LedColors::Blu.g, LedColors::Blu.b);
    setSquareLED(4, i, LedColors::Blu.r, LedColors::Blu.g, LedColors::Blu.b);
    showLEDs();
    delay(100);
  }
  clearAllLEDs();
}

void BoardDriver::blinkSquare(int row, int col, uint8_t r, uint8_t g, uint8_t b, int times, bool clearAfter) {
  for (int i = 0; i < times; i++) {
    setSquareLED(row, col, r, g, b);
    showLEDs();
    delay(200);
    setSquareLED(row, col, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b);
    showLEDs();
    delay(200);
  }
  if (!clearAfter) {
    setSquareLED(row, col, r, g, b);
    showLEDs();
  }
}

void BoardDriver::fireworkAnimation() {
  float centerX = 3.5;
  float centerY = 3.5;

  // Expansion phase:
  for (float radius = 0; radius < 6; radius += 0.5) {
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        float dx = col - centerX;
        float dy = row - centerY;
        float dist = sqrt(dx * dx + dy * dy);
        int pixelIndex = getPixelIndex(row, col);
        if (fabs(dist - radius) < 0.5)
          strip.setPixelColor(pixelIndex, strip.Color(LedColors::White.r, LedColors::White.g, LedColors::White.b));
        else
          strip.setPixelColor(pixelIndex, 0);
      }
    strip.show();
    delay(100);
  }

  // Contraction phase:
  for (float radius = 6; radius > 0; radius -= 0.5) {
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        float dx = col - centerX;
        float dy = row - centerY;
        float dist = sqrt(dx * dx + dy * dy);
        int pixelIndex = getPixelIndex(row, col);
        if (fabs(dist - radius) < 0.5)
          strip.setPixelColor(pixelIndex, strip.Color(LedColors::White.r, LedColors::White.g, LedColors::White.b));
        else
          strip.setPixelColor(pixelIndex, 0);
      }
    strip.show();
    delay(100);
  }

  // Second expansion phase:
  for (float radius = 0; radius < 6; radius += 0.5) {
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        float dx = col - centerX;
        float dy = row - centerY;
        float dist = sqrt(dx * dx + dy * dy);
        int pixelIndex = getPixelIndex(row, col);
        if (fabs(dist - radius) < 0.5)
          strip.setPixelColor(pixelIndex, strip.Color(LedColors::White.r, LedColors::White.g, LedColors::White.b));
        else
          strip.setPixelColor(pixelIndex, 0);
      }
    strip.show();
    delay(100);
  }

  clearAllLEDs();
}

void BoardDriver::captureAnimation() {
  float centerX = 3.5;
  float centerY = 3.5;

  // Pulsing outward animation
  for (int pulse = 0; pulse < 3; pulse++) {
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        float dx = col - centerX;
        float dy = row - centerY;
        float dist = sqrt(dx * dx + dy * dy);

        // Create a pulsing effect around the center
        float pulseWidth = 1.5 + pulse;
        int pixelIndex = getPixelIndex(row, col);

        if (dist >= pulseWidth - 0.5 && dist <= pulseWidth + 0.5) {
          // Alternate between red and orange for capture effect
          uint32_t color = (pulse % 2 == 0) ? strip.Color(LedColors::Red.r, LedColors::Red.g, LedColors::Red.b) : strip.Color(LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b);
          strip.setPixelColor(pixelIndex, color);
        } else {
          strip.setPixelColor(pixelIndex, 0);
        }
      }
    strip.show();
    delay(150);
  }

  clearAllLEDs();
}

void BoardDriver::promotionAnimation(int col) {
  // Column-based waterfall animation
  for (int step = 0; step < 16; step++) {
    for (int row = 0; row < 8; row++) {
      int pixelIndex = getPixelIndex(row, col);

      // Create a golden wave moving up and down the column
      if ((step + row) % 8 < 4)
        strip.setPixelColor(pixelIndex, strip.Color(LedColors::Gold.r, LedColors::Gold.g, LedColors::Gold.b));
      else
        strip.setPixelColor(pixelIndex, 0);
    }
    strip.show();
    delay(100);
  }

  // Clear the animation
  for (int row = 0; row < 8; row++) {
    int pixelIndex = getPixelIndex(row, col);
    strip.setPixelColor(pixelIndex, 0);
  }
  strip.show();
}

void BoardDriver::flashBoardAnimation(uint8_t r, uint8_t g, uint8_t b, int times) {
  for (int i = 0; i < times; i++) {
    clearAllLEDs();
    delay(200);
    // Light up entire board with specified color
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++)
        setSquareLED(row, col, r, g, b);
    showLEDs();
    delay(200);
  }
  clearAllLEDs();
}

void BoardDriver::updateSetupDisplay() {
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      // Check if a piece is detected on this square
      if (sensorState[row][col]) {
        if (row <= 1 || row >= 6)
          // Black or white side - turn off LED (piece is in correct area)
          setSquareLED(row, col, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b);
        else
          // Middle rows - show error (piece shouldn't be here)
          setSquareLED(row, col, LedColors::Red.r, LedColors::Red.g, LedColors::Red.b);
      } else {
        // No piece detected - show where pieces should be placed
        if (row <= 1)
          setSquareLED(row, col, LedColors::Blu.r, LedColors::Blu.g, LedColors::Blu.b); // Black side
        else if (row >= 6)
          setSquareLED(row, col, LedColors::White.r, LedColors::White.g, LedColors::White.b); // White side
        else
          setSquareLED(row, col, LedColors::Off.r, LedColors::Off.g, LedColors::Off.b); // Middle rows - turn off
      }
  strip.show();
}