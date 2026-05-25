#include <Arduino.h>
#include <HardwareSerial.h>
#include <math.h>
#include "const.h"

HardwareSerial DialSerial(STAMP2_UART_PORT);

LedParam ledParams[LED_COUNT];

unsigned long beatInterval = 500;
unsigned long lightingStartMs = 0;
int globalBPM = 120;

uint16_t ledState = 0;
bool lightingActive = false;

bool shouldLightUp(int id);
void updateLEDs(uint16_t states);

// beatInterval = 4分音符 (60000/BPM ms)
// beat = 1,2,4,8,16 → 全/二分/四分/八分/十六分音符の周期
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

void setup() {
  Serial.begin(BAUD_RATE);
  DialSerial.begin(BAUD_RATE, SERIAL_8N1, STAMP2_DIAL_RX_PIN, STAMP2_DIAL_TX_PIN);
  ledInit();

  for (int i = 0; i < LED_COUNT; i++) {
    ledParams[i].beat = DEFAULT_BEAT;
    ledParams[i].wave = DEFAULT_WAVE;
  }

  ledAllOff();

  Serial.println("Stamp2: LED Controller Initialized");
  Serial.println("Waiting for commands from Dial...");
}

void loop() {
  receiveCommands();
  updateLighting();
  delay(LOOP_DELAY);
}

void receiveCommands() {
  while (DialSerial.available()) {
    char buffer[COMMAND_BUFFER_SIZE];
    int len = DialSerial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
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

void ledInit() {
  pinMode(STAMP2_DATA_PIN, OUTPUT);
  pinMode(STAMP2_CLK_PIN, OUTPUT);
  pinMode(STAMP2_LATCH_PIN, OUTPUT);

  digitalWrite(STAMP2_DATA_PIN, LOW);
  digitalWrite(STAMP2_CLK_PIN, LOW);
  digitalWrite(STAMP2_LATCH_PIN, LOW);
}

void ledWrite16(uint16_t value) {
  digitalWrite(STAMP2_LATCH_PIN, LOW);

  shiftOut(STAMP2_DATA_PIN, STAMP2_CLK_PIN, MSBFIRST, highByte(value));
  shiftOut(STAMP2_DATA_PIN, STAMP2_CLK_PIN, MSBFIRST, lowByte(value));

  digitalWrite(STAMP2_LATCH_PIN, HIGH);
  delayMicroseconds(STAMP2_SHIFT_DELAY_US);
  digitalWrite(STAMP2_LATCH_PIN, LOW);
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
