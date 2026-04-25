// Function benchmarking macros
#ifdef BENCHMARK_ENABLED
  #define BENCHMARK_START(name) \
    static FunctionBenchmark bench_##name(#name); \
    unsigned long bench_start_##name = micros();

  #define BENCHMARK_END(name) \
    bench_##name.addSample(micros() - bench_start_##name);

  #define BENCHMARK_REPORT_ALL() \
    bench_loop.report(); \
    bench_handleTouchZones.report(); \
    bench_receiveCommands.report(); \
    bench_updateDisplay.report(); \
    bench_calculateBPM.report();

  #define BENCHMARK_RESET_ALL() \
    bench_loop.reset(); \
    bench_handleTouchZones.reset(); \
    bench_receiveCommands.reset(); \
    bench_updateDisplay.reset(); \
    bench_calculateBPM.reset();
#else
  #define BENCHMARK_START(name)
  #define BENCHMARK_END(name)
  #define BENCHMARK_REPORT_ALL()
  #define BENCHMARK_RESET_ALL()
#endif

// Add to Dial.ino main functions
void loop() {
  BENCHMARK_START(loop);
  
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
  
  BENCHMARK_END(loop);
  
  // 定期的にベンチマークレポート
  static unsigned long lastBenchmarkReport = 0;
  if (millis() - lastBenchmarkReport > BENCHMARK_INTERVAL_MS) {
    Serial.println("\n📊 === PERFORMANCE BENCHMARK REPORT ===");
    BENCHMARK_REPORT_ALL();
    Serial.println("=====================================\n");
    BENCHMARK_RESET_ALL();
    lastBenchmarkReport = millis();
  }
}

void handleTouchZones() {
  BENCHMARK_START(handleTouchZones);
  
  // Check touch screen state
  bool currentlyTouched = (M5Dial.Touch.getCount() > 0);
  
  if (currentlyTouched && !wasTouched) {
    // タッチ開始
    wasTouched = true;
    touchReleaseTime = millis();
    
    // タッチ位置を取得
    auto touch = M5Dial.Touch.getDetail();
    int touchY = touch.y;
    
    Serial.printf("Touch started at Y: %d\n", touchY);
    
    // タッチ位置に応じた処理
    if (touchY < MODE_BUTTON_ZONE_Y) {
      if (debugMode) {
        bpmMode = !bpmMode;
        Serial.printf("Direct mode switch: %s\n", bpmMode ? "BPM MODE" : "EDIT MODE");
        updateDisplay();
      } else {
        Serial.println("Mode button zone touched");
        parseKeyButton("KEY_BUTTON:PRESSED");
      }
    } else {
      if (bpmMode) {
        Serial.println("BPM zone touched (middle or bottom)");
        handleBPMTap();
      } else {
        Serial.println("Wave zone touched (middle or bottom)");
        handleTouchButton();
      }
    }
  } else if (!currentlyTouched && wasTouched) {
    // タッチ終了
    wasTouched = false;
    unsigned long touchDuration = millis() - touchReleaseTime;
    
    if (touchDuration < MIN_TOUCH_DURATION) {
      Serial.println("Touch too short - ignored");
      return;
    }
    
    Serial.printf("Touch released after %lu ms\n", touchDuration);
  }
  
  BENCHMARK_END(handleTouchZones);
}

void receiveCommands() {
  BENCHMARK_START(receiveCommands);
  
  // Stamp1からの受信処理
  if (Stamp1Serial.available()) {
    char buffer[COMMAND_BUFFER_SIZE];
    int len = Stamp1Serial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    
    String command(buffer);
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
    } else if (simCommand == "BENCHMARK") {
      Serial.println("\n📊 === MANUAL BENCHMARK REPORT ===");
      BENCHMARK_REPORT_ALL();
      Serial.println("=================================\n");
    }
  }
  
  BENCHMARK_END(receiveCommands);
}

void updateDisplay() {
  BENCHMARK_START(updateDisplay);
  
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
  
  BENCHMARK_END(updateDisplay);
}

void calculateBPM() {
  BENCHMARK_START(calculateBPM);
  
  if (tapCount < 2) {
    BENCHMARK_END(calculateBPM);
    return;
  }
  
  int validTaps = min(tapCount, 3);
  int bpmCount = 0;
  unsigned long totalBPM = 0;
  
  Serial.println("=== BPM CALCULATION ===");
  
  // 各間隔で個別にBPMを計算
  for (int i = 1; i < validTaps; i++) {
    unsigned long interval;
    if (tapTimes[i] >= tapTimes[i-1]) {
      interval = tapTimes[i] - tapTimes[i-1];
    } else {
      Serial.printf("Invalid interval detected between taps %d and %d - skipping\n", i-1, i);
      continue;
    }
    
    if (interval > MAX_INTERVAL_FOR_VALID_CHECK) {
      Serial.printf("Invalid interval %d: %lu ms - too large, skipping\n", i, interval);
      continue;
    }
    
    int individualBPM = BPM_CALCULATION_INTERVAL / interval;
    
    if (individualBPM < MIN_BPM) individualBPM = MIN_BPM;
    if (individualBPM > MAX_BPM) individualBPM = MAX_BPM;
    
    Serial.printf("Interval %d: %lu ms -> BPM: %d\n", i, interval, individualBPM);
    
    totalBPM += individualBPM;
    bpmCount++;
  }
  
  if (bpmCount > 0) {
    currentBPM = totalBPM / bpmCount;
    
    Serial.printf("Total BPM: %lu\n", totalBPM);
    Serial.printf("BPM count: %d\n", bpmCount);
    Serial.printf("Average BPM: %d\n", currentBPM);
    Serial.printf("Sending to Stamp2: SET_BPM:%d\n", currentBPM);
    
    sendBPMToStamp2(currentBPM);
    
    Serial.println("Continuing BPM measurement - no reset");
  } else {
    Serial.println("No valid intervals found for BPM calculation");
  }
  
  BENCHMARK_END(calculateBPM);
}