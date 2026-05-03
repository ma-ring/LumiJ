#include "M5Dial.h"
#include "const.h"

// Use constants from const.h


HardwareSerial Stamp1Serial(STAMP1_UART_PORT);
HardwareSerial Stamp2Serial(STAMP2_UART_PORT);


// System state
int selectedId = 0;
LedParam ledParams[LED_COUNT];

// UI state
int currentBeatValue = DEFAULT_BEAT;
WaveType currentWaveValue = DEFAULT_WAVE;

// BPM mode state
bool bpmMode = false;
int currentBPM = 120;
unsigned long lastTapTime = 0;
int tapCount = 0;
unsigned long tapTimes[3] = {0};
unsigned long lastBPMCalcTime = 0;  // 最後のBPM計算時間

// Communication optimization - track last sent values
int lastSentBPM = -1;
int lastSentBeat = -1;
WaveType lastSentWave = SQUARE;

// Communication error handling
unsigned long lastCommunicationTime = 0;
bool communicationError = false;

// Wave type names
const char* waveNames[] = {"SQUARE", "TRIANGLE", "SINE", "SAW"};
const int numWaveTypes = 4;

// Valid beat values
const int validBeats[] = {1, 2, 4, 8, 16};
const int numValidBeats = 5;

// Display update control
unsigned long lastDisplayUpdate = 0;

// Debug mode flag for direct mode switching
bool debugMode = true;

// Layout 
int displayWidthCenter = DISPLAY_WIDTH_CENTER;
int displayHeightOffset = DISPLAY_HEIGHT_OFFSET;

// Encoder state for proper handling
long lastEncoderValue = 0;

// Touch screen state
bool wasTouched = false;
unsigned long touchReleaseTime = 0;

void setup() {
  // Use old API like working code
  auto config = M5.config();
  M5Dial.begin(config, true, false);
  Serial.begin(BAUD_RATE);
  
  // Initialize UART after M5.begin()
  Stamp1Serial.begin(BAUD_RATE, SERIAL_8N1, DIAL_STAMP1_RX_PIN, DIAL_STAMP1_TX_PIN);
  Stamp2Serial.begin(BAUD_RATE, SERIAL_8N1, DIAL_STAMP2_RX_PIN, DIAL_STAMP2_TX_PIN);
  
  // Initialize LED parameters
  for (int i = 0; i < LED_COUNT; i++) {
    ledParams[i].beat = DEFAULT_BEAT;
    ledParams[i].wave = DEFAULT_WAVE;
  }
  
  displayWidthCenter = M5Dial.Lcd.width() / 2;
  
  // Initialize display with old API
  M5Dial.Lcd.fillScreen(BLACK);
  M5Dial.Lcd.setTextSize(2);
  M5Dial.Lcd.setTextColor(WHITE);
  M5Dial.Lcd.setTextDatum(middle_center);
  M5Dial.Lcd.drawString("LumiJ Controller", displayWidthCenter, TITLE_Y + displayHeightOffset);
  M5Dial.Lcd.setTextSize(1);
  M5Dial.Lcd.drawString("Initializing...", displayWidthCenter, 120 + displayHeightOffset);
  
  // Initialize encoder state
  lastEncoderValue = M5Dial.Encoder.read();
  
  Serial.println("Dial: UI Controller Initialized (Old API with Touch)");
  Serial.println("Debug mode: " + String(debugMode ? "ON" : "OFF"));
}

void loop() {
  // Use old API update
  M5Dial.update();
  
  // Handle encoder with proper state tracking
  long currentEncoderValue = M5Dial.Encoder.read();
  if (currentEncoderValue != lastEncoderValue) {
    Serial.printf("Encoder changed: %ld -> %ld (diff: %ld)\n", 
                  lastEncoderValue, currentEncoderValue, 
                  currentEncoderValue - lastEncoderValue);
    handleEncoderMove(currentEncoderValue);
    lastEncoderValue = currentEncoderValue;
  }
  
  // Handle touch screen zones
  handleTouchZones();
  
  // Receive commands from Stamp1
  receiveCommands();
  
  // BPMタイムアウトチェック
  if (bpmMode && (millis() - lastBPMCalcTime > BPM_TIMEOUT)) {
    Serial.println("BPM timeout - resetting tap count");
    tapCount = 0;
    lastTapTime = 0;
  }
  
  // 通信状態チェック
  checkCommunicationStatus();
  
  // Update display periodically
  unsigned long currentTime = millis();
  if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }
  
  delay(LOOP_DELAY);
}

void handleTouchZones() {
  // Check touch screen state (M5Dial.update()はloop()で呼んでいるので不要)
  bool currentlyTouched = (M5Dial.Touch.getCount() > 0);
  
  if (currentlyTouched && !wasTouched) {
    // タッチ開始
    wasTouched = true;
    touchReleaseTime = millis();
    
    // タッチ位置を取得
    auto touch = M5Dial.Touch.getDetail();
    int touchY = touch.y;
    
    Serial.printf("Touch started at Y: %d\n", touchY);
    
    // タッチ位置に応じた処理（モード別）
    if (touchY < MODE_BUTTON_ZONE_Y) {
      // 上部：モード切替（デバッグモードでは直接切替）
      if (debugMode) {
        // デバッグモード：直接モード切替
        bpmMode = !bpmMode;
        Serial.printf("Direct mode switch: %s\n", bpmMode ? "BPM MODE" : "EDIT MODE");
        updateDisplay();
      } else {
        // 通常モード：従来通り
        Serial.println("Mode button zone touched");
        parseKeyButton("KEY_BUTTON:PRESSED");
      }
    } else {
      // 中部・下部：モードに応じた処理
      if (bpmMode) {
        // BPMモード：中部・下部どちらもBPM設定
        Serial.println("BPM zone touched (middle or bottom)");
        handleBPMTap();
      } else {
        // Editモード：中部・下部どちらもwave設定
        Serial.println("Wave zone touched (middle or bottom)");
        handleTouchButton();
      }
    }
  } else if (!currentlyTouched && wasTouched) {
    // タッチ終了
    wasTouched = false;
    unsigned long touchDuration = millis() - touchReleaseTime;
    
    // 短いタッチは無視（誤操作防止）
    if (touchDuration < MIN_TOUCH_DURATION) {
      Serial.println("Touch too short - ignored");
      return;
    }
    
    Serial.printf("Touch released after %lu ms\n", touchDuration);
  }
}

void handleEncoderMove(long encoderValue) {
  if (bpmMode) {
    // BPMモード：BPMの微調整
    if (encoderValue > lastEncoderValue) {
      // 時計回り - BPMを+1
      if (currentBPM < MAX_BPM) {
        currentBPM++;
        Serial.printf("BPM adjusted: %d (+1)\n", currentBPM);
        sendBPMToStamp2(currentBPM);  // 最適化された送信関数
        updateDisplay();
      } else {
        Serial.printf("BPM already at maximum (%d)\n", MAX_BPM);
      }
    } else if (encoderValue < lastEncoderValue) {
      // 反時計回り - BPMを-1
      if (currentBPM > MIN_BPM) {
        currentBPM--;
        Serial.printf("BPM adjusted: %d (-1)\n", currentBPM);
        sendBPMToStamp2(currentBPM);  // 最適化された送信関数
        updateDisplay();
      } else {
        Serial.printf("BPM already at minimum (%d)\n", MIN_BPM);
      }
    }
  } else {
    // Editモード：beat値の変更
    if (encoderValue > lastEncoderValue) {
      // 時計回り - beatを増加
      // 16から1へのラップアラウンド対応
      if (currentBeatValue == 16) {
        currentBeatValue = 1;  // 16の次は1に戻る
      } else {
        for (int i = 0; i < numValidBeats - 1; i++) {
          if (validBeats[i] == currentBeatValue) {
            currentBeatValue = validBeats[i + 1];
            break;
          }
        }
      }
    } else if (encoderValue < lastEncoderValue) {
      // 反時計回り - beatを減少
      // 1から16へのラップアラウンド対応
      if (currentBeatValue == 1) {
        currentBeatValue = 16;  // 1の前は16に戻る
      } else {
        for (int i = 1; i < numValidBeats; i++) {
          if (validBeats[i] == currentBeatValue) {
            currentBeatValue = validBeats[i - 1];
            break;
          }
        }
      }
    }
    
    // Update local parameter
    ledParams[selectedId].beat = currentBeatValue;
    
    // Send to Stamp2
    sendBeatToStamp2(selectedId, currentBeatValue);
    
    Serial.printf("Beat changed: ID=%d, beat=%d\n", selectedId, currentBeatValue);
    updateDisplay();
  }
}

void receiveCommands() {
  // Stamp1からの受信処理
  if (Stamp1Serial.available()) {
    // 固定バッファで受信（メモリ効率化）
    char buffer[COMMAND_BUFFER_SIZE];
    int len = Stamp1Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    
    String command(buffer);  // デバッグ用に一時的にString使用
    command.trim();
    
    Serial.printf("Received from Stamp1: %s\n", command.c_str());
    
    if (command.startsWith("KEY_DOWN:")) {
      parseKeyDown(command);
    } else if (command.startsWith("KEY_BUTTON:")) {
      parseKeyButton(command);
    }
  }
  
  // シリアルモニターからのシミュレーションコマンド
  if (Serial.available()) {
    char simBuffer[SIMULATION_BUFFER_SIZE];
    int len = Serial.readBytesUntil('\n', simBuffer, sizeof(simBuffer) - 1);
    simBuffer[len] = '\0';
    
    String simCommand(simBuffer);
    simCommand.trim();
    
    Serial.printf("SIMULATION: %s\n", simCommand.c_str());
    
    if (simCommand.startsWith("SIM_KEY:")) {
      int id = simCommand.substring(8).toInt();
      if (id >= 0 && id < LED_COUNT) {
        Serial.printf("Simulating KEY_DOWN:%d\n", id);
        parseKeyDown("KEY_DOWN:" + String(id));
      }
    } else if (simCommand == "SIM_KEYBUTTON") {
      Serial.println("Simulating KEY_BUTTON:PRESSED");
      parseKeyButton("KEY_BUTTON:PRESSED");
    } else if (simCommand.startsWith("SIM_TAP:")) {
      int interval = simCommand.substring(8).toInt();
      Serial.printf("Simulating tap with %d ms interval\n", interval);
      if (bpmMode) {
        lastTapTime = millis() - interval;
        handleBPMTap();
      } else {
        Serial.println("Not in BPM mode - use SIM_KEYBUTTON first");
      }
    } else if (simCommand == "STATUS") {
      printSystemStatus();
    } else if (simCommand == "HELP") {
      printSimulationHelp();
    } else if (simCommand == "DEBUG") {
      debugMode = !debugMode;
      Serial.printf("Debug mode %s\n", debugMode ? "enabled" : "disabled");
      updateDisplay();
    }
  }
}

void parseKeyDown(String command) {
  int id = command.substring(9).toInt();
  
  if (id >= 0 && id < LED_COUNT) {
    selectedId = id;
    currentBeatValue = ledParams[id].beat;
    currentWaveValue = ledParams[id].wave;
    
    Serial.printf("Key selected: ID=%d\n", id);
    updateDisplay();
    
    sendBeatToStamp2(id, currentBeatValue);
    sendWaveToStamp2(id, currentWaveValue);
  }
}

void parseKeyButton(String command) {
  bpmMode = !bpmMode;
  
  if (bpmMode) {
    Serial.println("=== BPM SELECTION MODE ACTIVATED ===");
    Serial.println("Touch screen zones for BPM setting");
    tapCount = 0;
    lastTapTime = 0;
  } else {
    Serial.println("=== BPM SELECTION MODE DEACTIVATED ===");
    Serial.println("Returning to normal edit mode");
  }
  
  updateDisplay();
}

void handleTouchButton() {
  currentWaveValue = (WaveType)((currentWaveValue + 1) % numWaveTypes);
  
  ledParams[selectedId].wave = currentWaveValue;
  
  sendWaveToStamp2(selectedId, currentWaveValue);
  
  Serial.printf("Wave changed: ID=%d, wave=%s\n", selectedId, waveNames[currentWaveValue]);
  updateDisplay();
}

void sendBeatToStamp2(int id, int beat) {
  // 値が変更された場合のみ送信
  if (beat != lastSentBeat || id != selectedId) {
    communicationError = false;
    lastCommunicationTime = millis();
    
    Stamp2Serial.printf("SET_BEAT:%d,%d\n", id, beat);
    Serial.printf("Sent to Stamp2: SET_BEAT:%d,%d\n", id, beat);
    lastSentBeat = beat;
  }
}

void sendWaveToStamp2(int id, WaveType wave) {
  // 値が変更された場合のみ送信
  if (wave != lastSentWave || id != selectedId) {
    communicationError = false;
    lastCommunicationTime = millis();
    
    Stamp2Serial.printf("SET_WAVE:%d,%s\n", id, waveNames[wave]);
    Serial.printf("Sent to Stamp2: SET_WAVE:%d,%s\n", id, waveNames[wave]);
    lastSentWave = wave;
  }
}

void sendBPMToStamp2(int bpm) {
  // 値が変更された場合のみ送信
  if (bpm != lastSentBPM) {
    communicationError = false;
    lastCommunicationTime = millis();
    
    Stamp2Serial.printf("SET_BPM:%d\n", bpm);
    Serial.printf("Sent to Stamp2: SET_BPM:%d\n", bpm);
    lastSentBPM = bpm;
  }
}

bool checkCommunicationStatus() {
  // 通信タイムアウトチェック
  if (millis() - lastCommunicationTime > COMMUNICATION_TIMEOUT && lastCommunicationTime > 0) {
    if (!communicationError) {
      communicationError = true;
      Serial.println("⚠️ Communication timeout detected!");
      Serial.printf("Last communication: %lu ms ago\n", millis() - lastCommunicationTime);
    }
    return false;
  }
  
  // エラー状態から復帰した場合
  if (communicationError && lastCommunicationTime > 0) {
    communicationError = false;
    Serial.println("✅ Communication restored");
  }
  
  return true;
}

void handleBPMTap() {
  unsigned long currentTime = millis();
  
  // タップ間隔チェック
  if (tapCount > 0 && (currentTime - lastTapTime < BPM_MIN_TAP_INTERVAL)) {
    Serial.printf("Tap too soon - ignoring (must be >%dms apart)\n", BPM_MIN_TAP_INTERVAL);
    return;
  }
  
  tapTimes[tapCount % 3] = currentTime;
  lastTapTime = currentTime;
  lastBPMCalcTime = currentTime;  // 最後のタップ時間を記録
  tapCount++;
  
  Serial.printf("TAP #%d detected at %lu ms\n", tapCount, currentTime);
  
  // 直近3回のタップでBPM計算（リセットなし）
  if (tapCount >= 3) {
    calculateBPM();
  } else {
    Serial.printf("Need %d more taps to calculate BPM...\n", 3 - tapCount);
  }
  
  updateDisplay();
}

void calculateBPM() {
  if (tapCount < 2) return;
  
  int validTaps = min(tapCount, 3);
  int bpmCount = 0;
  unsigned long totalBPM = 0;
  
  Serial.println("=== BPM CALCULATION ===");
  
  // 各間隔で個別にBPMを計算
  for (int i = 1; i < validTaps; i++) {
    // オーバーフロー防止：正しい間隔計算
    unsigned long interval;
    if (tapTimes[i] >= tapTimes[i-1]) {
      interval = tapTimes[i] - tapTimes[i-1];
    } else {
      // オーバーフロー時はスキップ
      Serial.printf("Invalid interval detected between taps %d and %d - skipping\n", i-1, i);
      continue;
    }
    
    // 異常な間隔を無視
    if (interval > MAX_INTERVAL_FOR_VALID_CHECK) {
      Serial.printf("Invalid interval %d: %lu ms - too large, skipping\n", i, interval);
      continue;
    }
    
    // 個別BPM計算
    int individualBPM = BPM_CALCULATION_INTERVAL / interval;
    
    // BPM値の制限
    if (individualBPM < MIN_BPM) individualBPM = MIN_BPM;
    if (individualBPM > MAX_BPM) individualBPM = MAX_BPM;
    
    Serial.printf("Interval %d: %lu ms -> BPM: %d\n", i, interval, individualBPM);
    
    totalBPM += individualBPM;
    bpmCount++;
  }
  
  // 個別BPMの平均値を計算
  if (bpmCount > 0) {
    currentBPM = totalBPM / bpmCount;
    
    Serial.printf("Total BPM: %lu\n", totalBPM);
    Serial.printf("BPM count: %d\n", bpmCount);
    Serial.printf("Average BPM: %d\n", currentBPM);
    Serial.printf("Sending to Stamp2: SET_BPM:%d\n", currentBPM);
    
    sendBPMToStamp2(currentBPM);  // 最適化された送信関数
    
    // リセットしない - 次のタップに備える
    Serial.println("Continuing BPM measurement - no reset");
  } else {
    Serial.println("No valid intervals found for BPM calculation");
  }
}

void printSystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.printf("Mode: %s\n", bpmMode ? "BPM Selection" : "Normal Edit");
  Serial.printf("Selected ID: %d\n", selectedId);
  Serial.printf("Current Beat: %d\n", currentBeatValue);
  Serial.printf("Current Wave: %s\n", waveNames[currentWaveValue]);
  Serial.printf("Current BPM: %d\n", currentBPM);
  Serial.printf("Tap Count: %d\n", tapCount);
  Serial.printf("Debug Mode: %s\n", debugMode ? "ON" : "OFF");
  
  // 通信状態表示
  Serial.printf("Communication Status: %s\n", communicationError ? "ERROR" : "OK");
  if (lastCommunicationTime > 0) {
    Serial.printf("Last Communication: %lu ms ago\n", millis() - lastCommunicationTime);
  }
  
  Serial.println("====================\n");
}

void printSimulationHelp() {
  Serial.println("\n=== SIMULATION COMMANDS ===");
  Serial.println("SIM_KEY:<id>        - Simulate key press (0-15)");
  Serial.println("SIM_KEYBUTTON      - Simulate KeyButton press");
  Serial.println("SIM_TAP:<interval>  - Simulate tap with interval (ms)");
  Serial.println("STATUS              - Show current system status");
  Serial.println("DEBUG               - Toggle debug mode");
  Serial.println("HELP                - Show this help");
  Serial.println("========================\n");
}

void updateDisplay() {
  // Use old API like working code
  M5Dial.Lcd.fillScreen(BLACK);
  
  // タイトル（中央揃え）
  M5Dial.Lcd.setTextSize(2);
  M5Dial.Lcd.setTextColor(WHITE);
  M5Dial.Lcd.setTextDatum(middle_center);
  M5Dial.Lcd.drawString("LumiJ Controller", displayWidthCenter, TITLE_Y + displayHeightOffset);
  
  // モード表示
  M5Dial.Lcd.setTextSize(1);
  M5Dial.Lcd.setTextColor(bpmMode ? YELLOW : CYAN);
  M5Dial.Lcd.drawString(bpmMode ? "BPM MODE" : "EDIT MODE", displayWidthCenter, MODE_Y + displayHeightOffset);
  
  if (bpmMode) {
    // BPMモード表示
    M5Dial.Lcd.setTextSize(4);
    M5Dial.Lcd.setTextColor(WHITE);
    M5Dial.Lcd.drawString(String(currentBPM), displayWidthCenter, 65 + displayHeightOffset);
    
    if (tapCount > 0) {
      M5Dial.Lcd.setTextSize(1);
      M5Dial.Lcd.setTextColor(CYAN);
      M5Dial.Lcd.drawString("Taps: " + String(tapCount), displayWidthCenter, 95 + displayHeightOffset);
    }
    
    M5Dial.Lcd.setTextSize(1);
    M5Dial.Lcd.setTextColor(GREEN);
    M5Dial.Lcd.drawString("Touch zones:", displayWidthCenter, 105 + displayHeightOffset);
    M5Dial.Lcd.drawString("Top: Mode | Middle/Bottom: BPM", displayWidthCenter, 120 + displayHeightOffset);
  } else {
    // 通常編集モード表示
    M5Dial.Lcd.setTextSize(4);
    M5Dial.Lcd.setTextColor(YELLOW);
    M5Dial.Lcd.drawString("ID: " + String(selectedId), displayWidthCenter, 55 + displayHeightOffset);
    
    M5Dial.Lcd.setTextSize(2);
    M5Dial.Lcd.setTextColor(CYAN);
    M5Dial.Lcd.drawString("Beat: " + String(currentBeatValue), displayWidthCenter, 85 + displayHeightOffset);
    
    M5Dial.Lcd.setTextColor(MAGENTA);
    M5Dial.Lcd.drawString("Wave: " + String(waveNames[currentWaveValue]), displayWidthCenter, 105 + displayHeightOffset);
    
    M5Dial.Lcd.setTextSize(1);
    M5Dial.Lcd.setTextColor(GREEN);
    M5Dial.Lcd.drawString("Touch zones:", displayWidthCenter, 125 + displayHeightOffset);
    M5Dial.Lcd.drawString("Top: Mode | Middle/Bottom: Wave", displayWidthCenter, 140 + displayHeightOffset);
  }
  
  // デバッグモード表示
  if (debugMode) {
    M5Dial.Lcd.setTextSize(1);
    M5Dial.Lcd.setTextColor(RED);
    M5Dial.Lcd.drawString("DEBUG MODE", displayWidthCenter, DEBUG_Y);
  }
  
  // 通信エラー表示
  if (communicationError) {
    M5Dial.Lcd.setTextSize(1);
    M5Dial.Lcd.setTextColor(ORANGE);
    M5Dial.Lcd.drawString("COMM ERROR", displayWidthCenter, COMM_ERROR_Y);
  }
}
