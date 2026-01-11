#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include <Adafruit_NeoPixel.h>

// ---------------------------
// Hardware Configuration
// ---------------------------

// if ESP32 use pin GPIO 32, otherwise 17
#if defined(ESP32)
    #define LED_PIN 32
#else
    #define LED_PIN     17       // Pin for NeoPixels
#endif

#define NUM_ROWS    8
#define NUM_COLS    8
#define LED_COUNT   (NUM_ROWS * NUM_COLS)
#define BRIGHTNESS  255  // LED brightness: 0-255 (0=off, 255=max). Current: 255 (100% max brightness)

// Shift Register (74HC594) Pins
#define SR_SER_DATA_PIN     2   // Serial data input (74HC594 pin 14)
#define SR_CLK_PIN   3   // Shift register clock (pin 11)
#define SR_LATCH_PIN    4   // Latch clock (pin 12)

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

// Column Input Pins (D6..D13)
#define COL_PINS {6, 7, 8, 9, 10, 11, 12, 13}

// ---------------------------
// Board Driver Class
// ---------------------------
class BoardDriver {
private:
    Adafruit_NeoPixel strip;
    int colPins[NUM_COLS];
    byte rowPatterns[8];
    bool sensorState[8][8];
    bool sensorPrev[8][8];
    
    void loadShiftRegister(byte data);
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
    void setSquareLED(int row, int col, uint32_t color);
    void setSquareLED(int row, int col, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
    void showLEDs();
    
    // Animation Functions
    void fireworkAnimation();
    void captureAnimation();
    void promotionAnimation(int col);
    void blinkSquare(int row, int col, int times = 3);
    void highlightSquare(int row, int col, uint32_t color);
    
    // Setup Functions
    bool checkInitialBoard(const char initialBoard[8][8]);
    void updateSetupDisplay(const char initialBoard[8][8]);
    void printBoardState(const char initialBoard[8][8]);
};

#endif // BOARD_DRIVER_H
