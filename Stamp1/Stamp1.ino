#include <Arduino.h>
#include <HardwareSerial.h>
#include <math.h>
#include "const.h"

// UART instance
HardwareSerial SerialPort(STAMP1_UART_PORT);

// Key matrix state
KeyMatrixState keyMatrixState;

// LED parameters for TPIC6B595
LedParam ledParams[LED_COUNT];

// Timing variables (beatInterval = 4分音符 ms)
unsigned long beatInterval = 500;
unsigned long lightingStartMs = 0;
int globalBPM = 120;

// LED state for TPIC6B595
uint16_t ledState = 0;
bool lightingActive = false;

void setup() {
  Serial.begin(BAUD_RATE);
  SerialPort.begin(BAUD_RATE, SERIAL_8N1, STAMP1_DIAL_RX_PIN, STAMP1_DIAL_TX_PIN);
  // Initialize LED control
  ledInit();
  // Initialize key matrix pins
  for (int i = 0; i < KEY_MATRIX_ROWS; i++) {
    pinMode(STAMP1_ROW_PINS[i], OUTPUT);
    digitalWrite(STAMP1_ROW_PINS[i], HIGH);
  }
  
  for (int j = 0; j < KEY_MATRIX_COLS; j++) {
    pinMode(STAMP1_COL_PINS[j], INPUT_PULLUP);
  }
  
  // Initialize KeyButton pin
  pinMode(KEY_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize LED parameters
  for (int i = 0; i < LED_COUNT; i++) {
    ledParams[i] = LedParam();
  }
  
  // Initialize LED parameters
  for (int i = 0; i < LED_COUNT; i++) {
    ledParams[i].beat = DEFAULT_BEAT;
    ledParams[i].wave = DEFAULT_WAVE;
  }


  ledAllOn();

  Serial.println("Stamp1: 4x4 Key Matrix + TPIC6B595 LED Initialized");
}

void loop() {
  /*
  // Scan key matrix
  scanKeys();
  
  // Check KeyButton
  checkKeyButton();
  
  // Receive commands from Dial
  receiveCommands();
  
  // Update LED lighting
  updateLighting();
  
  delay(DEBOUNCE_DELAY_MS);
  */
  ledWrite16(0xFFFF);
  delay(1000);

  //ledWrite16(0x0000);
  delay(1000);
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
  SerialPort.printf(MSG_KEY_BUTTON_FORMAT "\n");
}

void sendKeyDown(int id) {
  // Send key press notification to Dial
  SerialPort.printf(MSG_KEY_DOWN_FORMAT "\n", id);
}

// TPIC6B595 LED Control API (aligned with Stamp2DeviceTest / Stamp2)
void ledInit() {
  pinMode(STAMP1_DATA_PIN, OUTPUT);
  pinMode(STAMP1_CLK_PIN, OUTPUT);
  pinMode(STAMP1_LATCH_PIN, OUTPUT);

  digitalWrite(STAMP1_DATA_PIN, LOW);
  digitalWrite(STAMP1_CLK_PIN, LOW);
  digitalWrite(STAMP1_LATCH_PIN, LOW);
}

void ledWrite16(uint16_t value) {
  digitalWrite(STAMP1_LATCH_PIN, LOW);

  shiftOut(STAMP1_DATA_PIN, STAMP1_CLK_PIN, MSBFIRST, highByte(value));
  shiftOut(STAMP1_DATA_PIN, STAMP1_CLK_PIN, MSBFIRST, lowByte(value));

  digitalWrite(STAMP1_LATCH_PIN, HIGH);
}

void ledOn(uint8_t index) {
  if (index >= 16) return;
  ledState |= (1 << index);
  ledWrite16(ledState);
}

void ledOff(uint8_t index) {
  if (index >= 16) return;
  ledState &= ~(1 << index);
  ledWrite16(ledState);
}

void ledAllOff() {
  ledState = 0x0000;
  ledWrite16(ledState);
}

void ledAllOn() {
  ledState = 0xFFFF;
  ledWrite16(ledState);
}


void updateLEDs(uint16_t states) {
  ledState = states;
  ledWrite16(ledState);
}

uint16_t computeLedPattern() {
  uint16_t pattern = 0;
  for (int i = 0; i < LED_COUNT; i++) {
    if (shouldLightUp(i)) {
      pattern |= (1 << i);
    }
  }
  return pattern;
}

void writeLedPattern() {
  uint16_t pattern = computeLedPattern();
  if (pattern != ledState) {
    updateLEDs(pattern);
  }
}

void startLightingFromBpm() {
  lightingActive = true;
  lightingStartMs = millis();
  ledInit();
  writeLedPattern();
  Serial.printf(
    "Lighting started: pattern=0x%04X quarter=%lu ms beat=%d cycle=%lu ms\n",
    ledState, beatInterval, DEFAULT_BEAT, noteCycleMs(DEFAULT_BEAT));
}

unsigned long noteCycleMs(int beatDiv) {
  if (beatDiv < 1) {
    beatDiv = DEFAULT_BEAT;
  }
  return (beatInterval * 4UL) / (unsigned long)beatDiv;
}

float channelPhase(int beatDiv) {
  unsigned long cycle = noteCycleMs(beatDiv);
  if (cycle < 1) {
    return 0.0f;
  }
  unsigned long elapsed = (millis() - lightingStartMs) % cycle;
  return (float)elapsed / (float)cycle;
}

void updateLighting() {
  if (!lightingActive) return;
  writeLedPattern();
}

bool shouldLightUp(int id) {
  LedParam param = ledParams[id];
  int beat = param.beat;
  if (beat < 1) {
    beat = DEFAULT_BEAT;
  }

  float phase = channelPhase(beat);

  switch (param.wave) {
    case WAVE_SQUARE:
    case WAVE_SAW:
      return phase < 0.5f;

    case WAVE_TRIANGLE: {
      float level = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
      return level >= 0.5f;
    }

    case WAVE_SINE:
      return sinf(phase * 2.0f * PI) > 0.0f;

    default:
      return false;
  }
}

void receiveCommands() {
  while (SerialPort.available()) {
    char buffer[COMMAND_BUFFER_SIZE];
    int len = SerialPort.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';

    String command(buffer);
    command.trim();
    if (command.length() == 0) continue;

    Serial.printf("Received from Dial: %s\n", command.c_str());

    if (command.startsWith(CMD_SET_BEAT)) {
      parseSetBeat(command);
    } else if (command.startsWith(CMD_SET_WAVE)) {
      parseSetWave(command);
    } else if (command.startsWith(CMD_SET_BPM)) {
      parseSetBPM(command);
    }

    if (lightingActive) {
      writeLedPattern();
    }
  }
}

void parseSetBeat(String command) {
  int separatorPos = command.indexOf(CMD_SEPARATOR);
  if (separatorPos == -1) return;

  int id = command.substring(strlen(CMD_SET_BEAT), separatorPos).toInt();
  int beat = command.substring(separatorPos + 1).toInt();

  if (id >= 0 && id < LED_COUNT && beat >= 1 && beat <= 16) {
    ledParams[id].beat = beat;
    Serial.printf("Set beat: ID=%d, beat=%d\n", id, beat);
  }
}

void parseSetWave(String command) {
  int separatorPos = command.indexOf(CMD_SEPARATOR);
  if (separatorPos == -1) return;

  int id = command.substring(strlen(CMD_SET_WAVE), separatorPos).toInt();
  String waveName = command.substring(separatorPos + 1);

  if (id >= 0 && id < LED_COUNT) {
    for (int i = 0; i < NUM_WAVE_TYPES; i++) {
      if (waveName == WAVE_NAMES[i]) {
        ledParams[id].wave = (WaveType)i;
        Serial.printf("Set wave: ID=%d, wave=%s\n", id, WAVE_NAMES[i]);
        break;
      }
    }
  }
}

void parseSetBPM(String command) {
  int bpm = command.substring(strlen(CMD_SET_BPM)).toInt();

  if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
    globalBPM = bpm;
    beatInterval = 60000 / bpm;
    Serial.printf("Set BPM: %d, beat interval: %lu ms\n", globalBPM, beatInterval);
    startLightingFromBpm();
  }
}