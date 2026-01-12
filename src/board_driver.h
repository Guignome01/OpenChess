#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include <Adafruit_NeoPixel.h>

// ---------------------------
// Hardware Configuration
// ---------------------------

// // WS2812B LED Data IN GPIO pin (if ESP32 use pin GPIO 32, otherwise 17)
#if defined(ESP32)
#define LED_PIN 32
#else
#define LED_PIN 17
#endif
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
// Board Driver Class
// ---------------------------
class BoardDriver
{
private:
    Adafruit_NeoPixel strip;
    bool sensorState[NUM_ROWS][NUM_COLS];
    bool sensorPrev[NUM_ROWS][NUM_COLS];

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
    void blinkSquare(int row, int col, uint8_t r, uint8_t g, uint8_t b, int times = 3);
    void showConnectingAnimation();

    // Setup Functions
    bool checkInitialBoard(const char initialBoard[8][8]);
    void updateSetupDisplay(const char initialBoard[8][8]);
    void printBoardState(const char initialBoard[8][8]);
};

#endif // BOARD_DRIVER_H
