#ifndef CONST_H
#define CONST_H

// === Wave types ===
enum WaveType {
  SQUARE,
  TRIANGLE,
  SINE,
  SAW
};

// === Default values ===
#define DEFAULT_BEAT 4
#define DEFAULT_WAVE SQUARE

// === Communication settings ===
#define BAUD_RATE 115200
#define COMMAND_BUFFER_SIZE 64
#define COMMUNICATION_TIMEOUT 5000

// === UART port settings ===
#define DIAL_STAMP1_UART_PORT 1
#define DIAL_STAMP2_UART_PORT 2
#define STAMP1_UART_PORT 1
#define STAMP2_UART_PORT 2

// === Pin configurations ===
// Dial -> Stamp1
#define DIAL_STAMP1_TX_PIN 15
#define DIAL_STAMP1_RX_PIN 13
#define STAMP1_DIAL_TX_PIN 2
#define STAMP1_DIAL_RX_PIN 1

// Dial -> Stamp2
#define DIAL_STAMP2_TX_PIN 1
#define DIAL_STAMP2_RX_PIN 2

// Stamp1 Key Matrix
#define KEY_MATRIX_ROWS 4
#define KEY_MATRIX_COLS 4
#define KEY_BUTTON_PIN 11

// Stamp1 key matrix pins
const int STAMP1_ROW_PINS[KEY_MATRIX_ROWS] = {3, 4, 5, 6};
const int STAMP1_COL_PINS[KEY_MATRIX_COLS] = {7, 8, 9, 10};

// === System settings ===
#define LED_COUNT 16
#define MIN_BPM 40
#define MAX_BPM 240
#define BPM_CALCULATION_INTERVAL 60000

// === Timing constants ===
#define DEBOUNCE_DELAY_MS 10
#define SCAN_DELAY_US 10
#define DISPLAY_UPDATE_INTERVAL 200
#define BPM_MIN_TAP_INTERVAL 300
#define BPM_TIMEOUT 5000
#define LOOP_DELAY 50
#define MIN_TOUCH_DURATION 100

// === Display constants ===
#define DISPLAY_WIDTH_CENTER 160
#define DISPLAY_HEIGHT_OFFSET 40
#define TITLE_Y 15
#define MODE_Y 30
#define DEBUG_Y 5
#define COMM_ERROR_Y 20

// === Touch zones ===
#define MODE_BUTTON_ZONE_Y 50
#define WAVE_BUTTON_ZONE_Y 100
#define BPM_BUTTON_ZONE_Y 150
#define TOUCH_THRESHOLD 30

// === Command protocol ===
#define CMD_KEY_DOWN "KEY_DOWN:"
#define CMD_KEY_BUTTON "KEY_BUTTON:"
#define CMD_SET_BEAT "SET_BEAT:"
#define CMD_SET_WAVE "SET_WAVE:"
#define CMD_SET_BPM "SET_BPM:"
#define CMD_SEPARATOR ","

// === Protocol message formats ===
#define MSG_KEY_DOWN_FORMAT CMD_KEY_DOWN "%d"
#define MSG_KEY_BUTTON_FORMAT CMD_KEY_BUTTON "PRESSED"
#define MSG_SET_BEAT_FORMAT CMD_SET_BEAT "%d%s%d"
#define MSG_SET_WAVE_FORMAT CMD_SET_WAVE "%d%s%s"
#define MSG_SET_BPM_FORMAT CMD_SET_BPM "%d"

// === Valid beat values ===
const int VALID_BEATS[] = {1, 2, 4, 8, 16};
const int NUM_VALID_BEATS = 5;

// === Wave type names ===
const char* WAVE_NAMES[] = {"SQUARE", "TRIANGLE", "SINE", "SAW"};
const int NUM_WAVE_TYPES = 4;

// === Benchmark settings ===
#define BENCHMARK_ENABLED 1
#define BENCHMARK_INTERVAL_MS 30000  // 30秒ごとにレポート
#define MAX_BENCHMARK_SAMPLES 100

// === Data structures ===

// Key matrix state structure
struct KeyMatrixState {
  bool keyState[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
  bool lastKeyState[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
  bool lastKeyButtonState;
  
  KeyMatrixState() {
    lastKeyButtonState = false;
    for (int row = 0; row < KEY_MATRIX_ROWS; row++) {
      for (int col = 0; col < KEY_MATRIX_COLS; col++) {
        keyState[row][col] = false;
        lastKeyState[row][col] = false;
      }
    }
  }
};

// LED parameter structure
struct LedParam {
  int beat;
  WaveType wave;
  
  LedParam() : beat(DEFAULT_BEAT), wave(DEFAULT_WAVE) {}
  LedParam(int b, WaveType w) : beat(b), wave(w) {}
};

// === Benchmark structure ===
struct FunctionBenchmark {
  const char* name;
  unsigned long totalTime;
  unsigned long maxTime;
  unsigned long minTime;
  unsigned long callCount;
  
  FunctionBenchmark(const char* funcName) : 
    name(funcName), totalTime(0), maxTime(0), minTime(ULONG_MAX), callCount(0) {}
  
  void addSample(unsigned long duration) {
    totalTime += duration;
    if (duration > maxTime) maxTime = duration;
    if (duration < minTime) minTime = duration;
    callCount++;
  }
  
  void reset() {
    totalTime = 0;
    maxTime = 0;
    minTime = ULONG_MAX;
    callCount = 0;
  }
  
  void report() {
    if (callCount > 0) {
      Serial.printf("=== %s BENCHMARK ===\n", name);
      Serial.printf("Calls: %lu\n", callCount);
      Serial.printf("Average: %lu μs\n", totalTime / callCount);
      Serial.printf("Min: %lu μs\n", minTime);
      Serial.printf("Max: %lu μs\n", maxTime);
      Serial.printf("Total: %lu μs\n", totalTime);
      Serial.println("========================");
    }
  }
};

#endif // CONST_H