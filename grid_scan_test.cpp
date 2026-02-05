#include <Arduino.h>

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
// Row Input Pins
// ---------------------------
#define ROW_PIN_0 4
#define ROW_PIN_1 16
#define ROW_PIN_2 17
#define ROW_PIN_3 18
#define ROW_PIN_4 19
#define ROW_PIN_5 21
#define ROW_PIN_6 22
#define ROW_PIN_7 23

#define SR_COLUMN_CHANGE_DELAY_MS 3000

// Array of row pins for easy iteration
const int rowPins[] = {ROW_PIN_0, ROW_PIN_1, ROW_PIN_2, ROW_PIN_3, ROW_PIN_4, ROW_PIN_5, ROW_PIN_6, ROW_PIN_7};
const int numRowPins = 8;

// Shift register output pin names (QA-QH correspond to each bit position)
const char* shiftRegisterOutPins[] = {"QA", "QB", "QC", "QD", "QE", "QF", "QG", "QH"};

// Array of bytes to send to the shift register, each byte corresponds to one column being active (assuming active HIGH)
const uint8_t shiftRegisterBitPatterns[] = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000};
const int numShiftRegisterPatterns = sizeof(shiftRegisterBitPatterns) / sizeof(shiftRegisterBitPatterns[0]);

void loadShiftRegister(byte data, int bits = 8) {
  // Make sure latch is low before shifting data
  digitalWrite(SR_LATCH_PIN, LOW);
  // Shift bits MSB first
  for (int i = bits - 1; i >= 0; i--) {
    digitalWrite(SR_SER_DATA_PIN, !!(data & (1 << i)));
    delayMicroseconds(10);
    digitalWrite(SR_CLK_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SR_CLK_PIN, LOW);
    delayMicroseconds(10);
  }
  // Latch the data to output pins
  digitalWrite(SR_LATCH_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SR_LATCH_PIN, LOW);
}

void readRowPins(int rowValues[8]) {
  for (int i = 0; i < numRowPins; i++) {
    rowValues[i] = digitalRead(rowPins[i]);
  }
}

void printRowPinsGPIOLegend() {
  Serial.print("  GPIO Pins: ");
  for (int i = 0; i < numRowPins; i++) {
    int pinNum = rowPins[i];
    Serial.print(pinNum);
    if (pinNum < 10)
      Serial.print(" ");
    if (i < numRowPins - 1) {
      Serial.print(" ");
    }
  }
  Serial.println("");
}

void printRowPins(const int rowValues[8]) {
  for (int i = 0; i < numRowPins; i++) {
    Serial.print(" ");
    Serial.print(rowValues[i]);
    if (i < numRowPins - 1) {
      Serial.print(" ");
    }
  }
  Serial.println("");
}

void printByteWithLeadingZeros(uint8_t byte) {
  Serial.print("0b");
  for (int i = 7; i >= 0; i--) {
    Serial.print((byte >> i) & 1);
  }
}

bool arrayEqual(const int values1[8], const int values2[8]) {
  for (int i = 0; i < 8; i++) {
    if (values1[i] != values2[i]) {
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Grid scan tester");
  Serial.println("----------------------------");
  // Initialize shift register pins as outputs
  pinMode(SR_SER_DATA_PIN, OUTPUT);
  pinMode(SR_CLK_PIN, OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  // Initialize all shift register pins to LOW
  digitalWrite(SR_SER_DATA_PIN, LOW);
  digitalWrite(SR_CLK_PIN, LOW);
  digitalWrite(SR_LATCH_PIN, LOW);
  // Initialize row pins as inputs
  for (int i = 0; i < numRowPins; i++)
    pinMode(rowPins[i], INPUT);
  Serial.println("Setup complete!");
}

void loop() {
  // Iterate through the data array and send each byte
  for (int i = 0; i < numShiftRegisterPatterns; i++) {
    Serial.print("Sending byte: ");
    printByteWithLeadingZeros(shiftRegisterBitPatterns[i]);
    Serial.print(" (Shift Register Pin: ");
    Serial.print(shiftRegisterOutPins[i]);
    Serial.println(")");
    printRowPinsGPIOLegend();

    // Read row pins 4 times and check for consistency
    int rowReadings[4][8];
    for (int repeat = 0; repeat < 4; repeat++) {
      loadShiftRegister(shiftRegisterBitPatterns[i]);
      delayMicroseconds(100);
      readRowPins(rowReadings[repeat]);
      loadShiftRegister(0); // disable all shift register outputs, to mimic scanning behavior
      Serial.print("  Read #");
      Serial.print(repeat + 1);
      Serial.print(":  ");
      printRowPins(rowReadings[repeat]);
      delay(30);
    }
    // Check for consistency between all 4 reads
    bool consistent = true;
    for (int j = 1; j < 4; j++) {
      if (!arrayEqual(rowReadings[0], rowReadings[j])) {
        consistent = false;
        break;
      }
    }
    if (!consistent)
      Serial.println("⚠️ WARNING: Inconsistent readings detected!");
    else
      Serial.println("✓ All readings consistent");
    Serial.println();

    loadShiftRegister(shiftRegisterBitPatterns[i]);
    delay(SR_COLUMN_CHANGE_DELAY_MS); // allow time to take multimeter measurements on shift register outputs
  }
}
