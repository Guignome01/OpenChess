#include "board_driver.h"
#include <math.h>

// ---------------------------
// BoardDriver Implementation
// ---------------------------

// ---------------------------
// Row Input Pins (Safe pins for ESP32: 4, 13, 14, [16-17], 18, 19, 21, 22, 23, 25, 26, 27, 32, 33)
// ---------------------------
static constexpr int rowPins[NUM_ROWS] = {23, 22, 21, 19, 18, 17, 16, 4};
// ---------------------------
// LED Strip Col/Row to Pixel index mapping
// ---------------------------
static constexpr int RowColToLEDindexMap[NUM_ROWS][NUM_COLS] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {15, 14, 13, 12, 11, 10, 9, 8},
    {16, 17, 18, 19, 20, 21, 22, 23},
    {31, 30, 29, 28, 27, 26, 25, 24},
    {32, 33, 34, 35, 36, 37, 38, 39},
    {47, 46, 45, 44, 43, 42, 41, 40},
    {48, 49, 50, 51, 52, 53, 54, 55},
    {63, 62, 61, 60, 59, 58, 57, 56},
};

BoardDriver::BoardDriver() : strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800)
{
}

void BoardDriver::begin()
{
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
    {
        for (int col = 0; col < NUM_COLS; col++)
        {
            sensorState[row][col] = false;
            sensorPrev[row][col] = false;
        }
    }
}

void BoardDriver::loadShiftRegister(byte data)
{
    digitalWrite(SR_LATCH_PIN, LOW);
    for (int i = 7; i >= 0; i--)
    {
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

void BoardDriver::disableAllCols()
{
    loadShiftRegister(0);
}

void BoardDriver::enableCol(int col)
{
    loadShiftRegister(((byte)((1 << (col)) & 0xFF)));
    delayMicroseconds(100); // Allow time for the column to stabilize, otherwise random readings occur
}

void BoardDriver::readSensors()
{
    for (int col = 0; col < NUM_COLS; col++)
    {
        enableCol(col);
        for (int row = 0; row < NUM_ROWS; row++)
            sensorState[row][col] = digitalRead(rowPins[row]) == LOW;
    }
    disableAllCols();
}

bool BoardDriver::getSensorState(int row, int col)
{
    return sensorState[row][col];
}

bool BoardDriver::getSensorPrev(int row, int col)
{
    return sensorPrev[row][col];
}

void BoardDriver::updateSensorPrev()
{
    for (int row = 0; row < NUM_ROWS; row++)
    {
        for (int col = 0; col < NUM_COLS; col++)
            sensorPrev[row][col] = sensorState[row][col];
    }
}

int BoardDriver::getPixelIndex(int row, int col)
{
    return RowColToLEDindexMap[row][col];
}

void BoardDriver::clearAllLEDs()
{
    for (int i = 0; i < LED_COUNT; i++)
        strip.setPixelColor(i, 0);
    strip.show();
}

void BoardDriver::setSquareLED(int row, int col, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    uint32_t color;
    float multiplier = 1.0f;
    int pixelIndex = getPixelIndex(row, col);
    if (pixelIndex % 2 == 0)
        multiplier = 0.7f; // Dim dark squares to 70% brightness because they appear brighter due to the contrast
    (w == 255 && ((r == 0 && g == 0 && b == 0) || (r == 255 && g == 255 && b == 255))) ? color = strip.Color(255 * multiplier, 255 * multiplier, 255 * multiplier) : color = strip.Color(r * multiplier, g * multiplier, b * multiplier);
    strip.setPixelColor(pixelIndex, color);
}

void BoardDriver::showLEDs()
{
    strip.show();
}

void BoardDriver::blinkSquare(int row, int col, int times)
{
    int pixelIndex = getPixelIndex(row, col);
    for (int i = 0; i < times; i++)
    {
        strip.setPixelColor(pixelIndex, strip.Color(0, 0, 0, 255));
        strip.show();
        delay(200);
        strip.setPixelColor(pixelIndex, 0);
        strip.show();
        delay(200);
    }
}

void BoardDriver::fireworkAnimation()
{
    float centerX = 3.5;
    float centerY = 3.5;

    // Expansion phase:
    for (float radius = 0; radius < 6; radius += 0.5)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                float dx = col - centerX;
                float dy = row - centerY;
                float dist = sqrt(dx * dx + dy * dy);
                int pixelIndex = getPixelIndex(row, col);
                if (fabs(dist - radius) < 0.5)
                    strip.setPixelColor(pixelIndex, strip.Color(0, 0, 0, 255));
                else
                    strip.setPixelColor(pixelIndex, 0);
            }
        }
        strip.show();
        delay(100);
    }

    // Contraction phase:
    for (float radius = 6; radius > 0; radius -= 0.5)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                float dx = col - centerX;
                float dy = row - centerY;
                float dist = sqrt(dx * dx + dy * dy);
                int pixelIndex = getPixelIndex(row, col);
                if (fabs(dist - radius) < 0.5)
                    strip.setPixelColor(pixelIndex, strip.Color(0, 0, 0, 255));
                else
                    strip.setPixelColor(pixelIndex, 0);
            }
        }
        strip.show();
        delay(100);
    }

    // Second expansion phase:
    for (float radius = 0; radius < 6; radius += 0.5)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                float dx = col - centerX;
                float dy = row - centerY;
                float dist = sqrt(dx * dx + dy * dy);
                int pixelIndex = getPixelIndex(row, col);
                if (fabs(dist - radius) < 0.5)
                    strip.setPixelColor(pixelIndex, strip.Color(0, 0, 0, 255));
                else
                    strip.setPixelColor(pixelIndex, 0);
            }
        }
        strip.show();
        delay(100);
    }

    // Clear all LEDs
    clearAllLEDs();
}

void BoardDriver::captureAnimation()
{
    float centerX = 3.5;
    float centerY = 3.5;

    // Pulsing outward animation
    for (int pulse = 0; pulse < 3; pulse++)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                float dx = col - centerX;
                float dy = row - centerY;
                float dist = sqrt(dx * dx + dy * dy);

                // Create a pulsing effect around the center
                float pulseWidth = 1.5 + pulse;
                int pixelIndex = getPixelIndex(row, col);

                if (dist >= pulseWidth - 0.5 && dist <= pulseWidth + 0.5)
                {
                    // Alternate between red and orange for capture effect
                    uint32_t color = (pulse % 2 == 0)
                                         ? strip.Color(255, 0, 0, 0)    // Red
                                         : strip.Color(255, 165, 0, 0); // Orange
                    strip.setPixelColor(pixelIndex, color);
                }
                else
                {
                    strip.setPixelColor(pixelIndex, 0);
                }
            }
        }
        strip.show();
        delay(150);
    }

    // Clear LEDs
    clearAllLEDs();
}

void BoardDriver::promotionAnimation(int col)
{
    const uint32_t PROMOTION_COLOR = strip.Color(255, 215, 0, 50); // Gold with white

    // Column-based waterfall animation
    for (int step = 0; step < 16; step++)
    {
        for (int row = 0; row < 8; row++)
        {
            int pixelIndex = getPixelIndex(row, col);

            // Create a golden wave moving up and down the column
            if ((step + row) % 8 < 4)
            {
                strip.setPixelColor(pixelIndex, PROMOTION_COLOR);
            }
            else
            {
                strip.setPixelColor(pixelIndex, 0);
            }
        }
        strip.show();
        delay(100);
    }

    // Clear the animation
    for (int row = 0; row < 8; row++)
    {
        int pixelIndex = getPixelIndex(row, col);
        strip.setPixelColor(pixelIndex, 0);
    }
    strip.show();
}

bool BoardDriver::checkInitialBoard(const char initialBoard[8][8])
{
    readSensors();
    bool allPresent = true;
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            if (initialBoard[row][col] != ' ' && !sensorState[row][col])
            {
                allPresent = false;
            }
        }
    }
    return allPresent;
}

void BoardDriver::updateSetupDisplay(const char initialBoard[8][8])
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            // Check if a piece is detected on this square
            if (sensorState[row][col])
            {
                // Determine color based on where the piece is placed
                if (row <= 1)
                    setSquareLED(row, col, 255, 255, 255); // White side (top, rows 0-1) - White LED
                else if (row >= 6)
                    setSquareLED(row, col, 0, 0, 255); // Black side (bottom, rows 6-7) - Blue LED
                else
                    setSquareLED(row, col, 255, 0, 0); // Middle rows (rows 2-5) - Red LED
            }
            else
            {
                // No piece detected - turn off LED
                setSquareLED(row, col, 0, 0, 0);
            }
        }
    }
    strip.show();
}

void BoardDriver::printBoardState(const char initialBoard[8][8])
{
    Serial.println("Current Board:");
    for (int row = 0; row < 8; row++)
    {
        Serial.print("{ ");
        for (int col = 0; col < 8; col++)
        {
            char displayChar = ' ';
            if (initialBoard[row][col] != ' ')
            {
                displayChar = sensorState[row][col] ? initialBoard[row][col] : '-';
            }
            Serial.print("'");
            Serial.print(displayChar);
            Serial.print("'");
            if (col < 7)
                Serial.print(", ");
        }
        Serial.println(" },");
    }
    Serial.println();
}
