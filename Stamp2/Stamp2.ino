#include <Arduino.h>
#include <HardwareSerial.h>
#include <math.h>
#include "const.h"

// UART instance for receiving from Dial
HardwareSerial DialSerial(STAMP2_UART_PORT);

// LED parameters
LedParam ledParams[LED_COUNT];

// Timing variables
unsigned long lastUpdateTime = 0;
unsigned long beatInterval = 500; // Default beat interval
int currentBeat = 0;
int globalBPM = 120;

// Use shift register pin definitions from const.h

void setup() {
  Serial.begin(BAUD_RATE);
  DialSerial.begin(BAUD_RATE, SERIAL_8N1, STAMP2_DIAL_RX_PIN, STAMP2_DIAL_TX_PIN);
  
  // Initialize shift register pins
  pinMode(STAMP2_DIO_PIN, OUTPUT);
  pinMode(STAMP2_RCLK_PIN, OUTPUT);
  pinMode(STAMP2_SCLK_PIN, OUTPUT);
  
  // Set initial state
  digitalWrite(STAMP2_DIO_PIN, LOW);
  digitalWrite(STAMP2_SCLK_PIN, LOW);
  digitalWrite(STAMP2_RCLK_PIN, LOW);
  
  // Initialize LED parameters
  for (int i = 0; i < LED_COUNT; i++) {
    ledParams[i] = LedParam();
  }
  
  // Turn off all LEDs initially
  updateLEDs(0);
  
  Serial.println("Stamp2: LED Controller Initialized");
  Serial.println("Waiting for commands from Dial...");
}

void loop() {
  // Receive commands from Dial
  receiveCommands();
  
  // Update LED lighting
  updateLighting();
  
  delay(LOOP_DELAY);
}

void receiveCommands() {
  if (DialSerial.available()) {
    char buffer[COMMAND_BUFFER_SIZE];
    int len = DialSerial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    
    String command(buffer);
    command.trim();
    
    Serial.printf("Received from Dial: %s\n", command.c_str());
    
    if (command.startsWith(CMD_SET_BEAT)) {
      parseSetBeat(command);
    } else if (command.startsWith(CMD_SET_WAVE)) {
      parseSetWave(command);
    } else if (command.startsWith(CMD_SET_BPM)) {
      parseSetBPM(command);
    }
  }
}

void parseSetBeat(String command) {
  // Format: SET_BEAT:id,beat
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
  // Format: SET_WAVE:id,wave
  int separatorPos = command.indexOf(CMD_SEPARATOR);
  if (separatorPos == -1) return;
  
  int id = command.substring(strlen(CMD_SET_WAVE), separatorPos).toInt();
  String waveName = command.substring(separatorPos + 1);
  
  if (id >= 0 && id < LED_COUNT) {
    // Convert wave name to enum
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
  // Format: SET_BPM:bpm
  int bpm = command.substring(strlen(CMD_SET_BPM)).toInt();
  
  if (bpm >= MIN_BPM && bpm <= MAX_BPM) {
    globalBPM = bpm;
    // Calculate beat interval from BPM (60000ms / BPM)
    beatInterval = 60000 / bpm;
    Serial.printf("Set BPM: %d, beat interval: %lu ms\n", globalBPM, beatInterval);
  }
}

void updateLighting() {
  unsigned long currentTime = millis();
  
  // Calculate minimum beat value for timing
  int minBeat = 16;
  for (int i = 0; i < LED_COUNT; i++) {
    if (ledParams[i].beat < minBeat) {
      minBeat = ledParams[i].beat;
    }
  }
  
  // Update timing based on minimum beat
  if (currentTime - lastUpdateTime >= beatInterval) {
    lastUpdateTime = currentTime;
    currentBeat = (currentBeat + 1) % minBeat;
    
    // Calculate LED states
    uint16_t ledStates = 0;
    
    for (int i = 0; i < LED_COUNT; i++) {
      if (shouldLightUp(i, currentBeat)) {
        ledStates |= (1 << i);
      }
    }
    
    updateLEDs(ledStates);
  }
}

bool shouldLightUp(int id, int currentBeat) {
  LedParam param = ledParams[id];
  
  // Calculate position within beat cycle
  int beatPosition = currentBeat % param.beat;
  float phase = (float)beatPosition / param.beat;
  
  switch (param.wave) {
    case SQUARE:
      return beatPosition < (param.beat / 2);
    
    case TRIANGLE:
      return phase < 0.5 ? (phase * 2) > 0.5 : (2 - phase * 2) > 0.5;
    
    case SINE:
      return sin(phase * 2 * PI) > 0;
    
    case SAW:
      return phase < 0.5;
    
    default:
      return false;
  }
}

void updateLEDs(uint16_t states) {
  digitalWrite(STAMP2_RCLK_PIN, LOW);
  
  // Shift out 16 bits (MSB first)
  // K-595D12W uses two 74HC595 in series for 16-bit support
  for (int i = LED_COUNT - 1; i >= 0; i--) {
    digitalWrite(STAMP2_SCLK_PIN, LOW);
    digitalWrite(STAMP2_DIO_PIN, (states >> i) & 1);
    digitalWrite(STAMP2_SCLK_PIN, HIGH);
    delayMicroseconds(STAMP2_SHIFT_DELAY_US);
  }
  
  // Latch the output
  digitalWrite(STAMP2_RCLK_PIN, HIGH);
  delayMicroseconds(STAMP2_SHIFT_DELAY_US);
  digitalWrite(STAMP2_RCLK_PIN, LOW);
}