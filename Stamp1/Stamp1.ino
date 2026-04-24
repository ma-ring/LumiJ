#include <Arduino.h>
#include <HardwareSerial.h>
#include "const.h"

// UART instance
HardwareSerial SerialPort(STAMP1_UART_PORT);

// Key matrix state
KeyMatrixState keyMatrixState;

void setup() {
  Serial.begin(BAUD_RATE);
  SerialPort.begin(BAUD_RATE, SERIAL_8N1, STAMP1_DIAL_RX_PIN, STAMP1_DIAL_TX_PIN);
  
  // Initialize pins
  for (int i = 0; i < KEY_MATRIX_ROWS; i++) {
    pinMode(STAMP1_ROW_PINS[i], OUTPUT);
    digitalWrite(STAMP1_ROW_PINS[i], HIGH);
  }
  
  for (int j = 0; j < KEY_MATRIX_COLS; j++) {
    pinMode(STAMP1_COL_PINS[j], INPUT_PULLUP);
  }
  
  // Initialize KeyButton pin
  pinMode(KEY_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("Stamp1: 4x4 Key Matrix Initialized");
}

void loop() {
  // Scan key matrix
  scanKeys();
  
  // Check KeyButton
  checkKeyButton();
  
  delay(DEBOUNCE_DELAY_MS);
}

void scanKeys() {
  for (int row = 0; row < KEY_MATRIX_ROWS; row++) {
    // Set current row to LOW
    digitalWrite(STAMP1_ROW_PINS[row], LOW);
    delayMicroseconds(SCAN_DELAY_US);
    
    for (int col = 0; col < KEY_MATRIX_COLS; col++) {
      // Read column pin state
      bool currentState = (digitalRead(STAMP1_COL_PINS[col]) == LOW);
      
      // Check if key was pressed
      if (currentState && !keyMatrixState.lastKeyState[row][col]) {
        // Key pressed moment
        int keyId = row * KEY_MATRIX_COLS + col;
        sendKeyDown(keyId);
        Serial.printf("Key pressed: %d (row:%d, col:%d)\n", keyId, row, col);
      }
      
      keyMatrixState.lastKeyState[row][col] = currentState;
    }
    
    // Return row to HIGH
    digitalWrite(STAMP1_ROW_PINS[row], HIGH);
  }
}

void checkKeyButton() {
  bool currentKeyButtonState = (digitalRead(KEY_BUTTON_PIN) == LOW);
  
  // Check if KeyButton was pressed
  if (currentKeyButtonState && !keyMatrixState.lastKeyButtonState) {
    // KeyButton pressed moment
    sendKeyButtonPress();
    Serial.println("KeyButton pressed");
  }
  
  keyMatrixState.lastKeyButtonState = currentKeyButtonState;
}

void sendKeyButtonPress() {
  // Send KeyButton press notification to Dial
  SerialPort.printf(MSG_KEY_BUTTON_FORMAT);
}

void sendKeyDown(int id) {
  // Send key press notification to Dial
  SerialPort.printf(MSG_KEY_DOWN_FORMAT, id);
}