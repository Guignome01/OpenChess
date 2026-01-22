#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include <Adafruit_NeoPixel.h>

// ---------------------------
// Hardware Configuration
// ---------------------------

// ---------------------------
// WS2812B LED Data IN GPIO Pin
// The strip doesn't need to have a specific layout, calibration will map it correctly
// ---------------------------
#define LED_PIN 32
#define NUM_ROWS 8
#define NUM_COLS 8
#define LED_COUNT (NUM_ROWS * NUM_COLS)
#define BRIGHTNESS 255 // LED brightness: 0-255 (0=off, 255=max). Current: 255 (100% max brightness)

// ---------------------------
// Shift Register (74HC595) Pins
// ---------------------------
// Pin 10 (SRCLR') 5V = don't clear the register
// Pin 13 (OE') GND = always enabled
// Pin 11 (SRCLK) GPIO = Shift Register Clock
#define SR_CLK_PIN 14
// Pin 12 (RCLK) GPIO = Latch Clock
#define SR_LATCH_PIN 26
// Pin 14 (SER) GPIO = Serial data input
#define SR_SER_DATA_PIN 33

// ---------------------------
// Row and column pins don't need to be in any particular order, calibration will map them correctly
// ---------------------------

// ---------------------------
// Row Input Pins (Safe pins for ESP32: 4, 13, 14, [16-17], 18, 19, 21, 22, 23, 25, 26, 27, 32, 33)ù
// ---------------------------
#define ROW_PIN_0 4
#define ROW_PIN_1 16
#define ROW_PIN_2 17
#define ROW_PIN_3 18
#define ROW_PIN_4 19
#define ROW_PIN_5 21
#define ROW_PIN_6 22
#define ROW_PIN_7 23

// ---------------------------
// Sensor Polling Delay and Debounce
// ---------------------------
#define SENSOR_READ_DELAY_MS 40
#define DEBOUNCE_MS 125

// ---------------------------
// Board Driver Class
// Logical board coordinates: row 0 = rank 8, column 0 = file a
// ---------------------------
class BoardDriver {
 private:
  Adafruit_NeoPixel strip;
  bool sensorState[NUM_ROWS][NUM_COLS];
  bool sensorPrev[NUM_ROWS][NUM_COLS];
  bool sensorRaw[NUM_ROWS][NUM_COLS];
  unsigned long sensorDebounceTime[NUM_ROWS][NUM_COLS];

  enum Axis {
    RowsAxis = 0,
    ColsAxis = 1,
    UnknownAxis = 2,
  };
  // Calibration data
  uint8_t swapAxes;
  uint8_t toLogicalRow[NUM_ROWS];
  uint8_t toLogicalCol[NUM_COLS];
  uint8_t ledIndexMap[NUM_ROWS][NUM_COLS];
  bool calibrationLoaded;

  bool loadCalibration();
  void saveCalibration();
  void runCalibration();
  void readRawSensors(bool rawState[NUM_ROWS][NUM_COLS]);
  bool waitForBoardEmpty();
  bool waitForSingleRawPress(int& rawRow, int& rawCol, unsigned long stableMs = 200);
  void showCalibrationError();
  bool calibrateAxis(Axis axis, uint8_t* axisPinsOrder, size_t NUM_PINS, bool firstAxisSwapped);
  String getAxisString(Axis axis) const { return (axis == RowsAxis) ? "Rows" : ((axis == ColsAxis) ? "Columns" : "Unknown"); };

  void loadShiftRegister(byte data);
  void disableAllCols();
  void enableCol(int col);
  int getPixelIndex(int row, int col);

 public:
  BoardDriver();
  void begin();
  void readSensors();
  bool getSensorState(int row, int col);
  bool getSensorPrev(int row, int col);
  void updateSensorPrev();

  // LED Control
  void clearAllLEDs();
  void setSquareLED(int row, int col, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
  void showLEDs();

  // Animation Functions
  void fireworkAnimation();
  void captureAnimation();
  void promotionAnimation(int col);
  void blinkSquare(int row, int col, uint8_t r, uint8_t g, uint8_t b, int times = 3, bool clearAfter = true);
  void showConnectingAnimation();
  void flashBoardAnimation(uint8_t r, uint8_t g, uint8_t b, int times = 3);

  // Setup Functions
  void updateSetupDisplay();
};

#endif // BOARD_DRIVER_H
