#include <Arduino.h>
#include <HardwareSerial.h>
#include "const.h"

// TPIC6B595 LED Control API
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

  uint16_t ledState = 0x0000;   // 全部OFF
  ledState = (1 << index);      // 指定LEDだけON

  ledWrite16(ledState);
}

void ledOff(uint8_t index) {
  if (index >= 16) return;

  ledWrite16(0x0000);
}

void ledAllOff() {
  ledWrite16(0x0000);
}

void ledAllOn() {
  ledWrite16(0xFFFF);
}

void printMenu() {
  Serial.println("\n--- Test modes (send number + Enter) ---");
  Serial.println("  1  All LEDs ON");
  Serial.println("  2  All LEDs OFF");
  Serial.println("  3  Individual LED (0-15)");
  Serial.println("  4  Sequential pattern (0->15)");
  Serial.println("  5  Reverse sequential (15->0)");
  Serial.println("  6  Alternating pattern");
  Serial.println("  7  Binary counter (step 256)");
  Serial.println("  8  All LEDs ON (final check)");
  Serial.println("  9  All LEDs OFF (end state)");
  Serial.println("  a  Run full sequence (1-9)");
  Serial.println("  ?  Show this menu");
  Serial.print("> ");
}

void testAllOn() {
  Serial.println("Test 1: All LEDs ON");
  ledAllOn();
}

void testAllOff() {
  Serial.println("Test 2: All LEDs OFF");
  ledAllOff();
}

void testIndividual() {
  Serial.println("Test 3: Individual LED test (0-15)");
  for (int i = 0; i < 16; i++) {
    Serial.printf("  LED %d ON\n", i);
    ledOn(i);
    delay(1000);
    ledAllOff();
  }
}

void testSequential() {
  Serial.println("Test 4: Sequential pattern");
  for (int i = 0; i < 16; i++) {
    ledWrite16(1 << i);
    delay(200);
  }
}

void testReverseSequential() {
  Serial.println("Test 5: Reverse sequential pattern");
  for (int i = 15; i >= 0; i--) {
    ledWrite16(1 << i);
    delay(200);
  }
}

void testAlternating() {
  Serial.println("Test 6: Alternating pattern");
  for (int cycle = 0; cycle < 8; cycle++) {
    if (cycle % 2 == 0) {
      ledWrite16(0x5555);
    } else {
      ledWrite16(0xAAAA);
    }
    delay(500);
  }
}

void testBinaryCounter() {
  Serial.println("Test 7: Binary counter (0-65535)");
  for (uint16_t value = 0; value < 65536; value += 256) {
    ledWrite16(value);
    delay(50);
  }
}

void testFinalOn() {
  Serial.println("Test 8: All LEDs ON (final check)");
  ledAllOn();
  delay(2000);
}

void testFinalOff() {
  Serial.println("Test 9: All LEDs OFF");
  ledAllOff();
}

bool executeTest(int mode) {
  switch (mode) {
    case 1: testAllOn(); break;
    case 2: testAllOff(); break;
    case 3: testIndividual(); break;
    case 4: testSequential(); break;
    case 5: testReverseSequential(); break;
    case 6: testAlternating(); break;
    case 7: testBinaryCounter(); break;
    case 8: testFinalOn(); break;
    case 9: testFinalOff(); break;
    default: return false;
  }
  return true;
}

void runTest(int mode) {
  if (!executeTest(mode)) {
    Serial.printf("Unknown mode: %d (use 1-9, a, or ?)\n", mode);
    return;
  }
  Serial.println("Done.\n");
  printMenu();
}

void runAllTests() {
  Serial.println("\n=== Running full test sequence ===\n");
  for (int m = 1; m <= 9; m++) {
    executeTest(m);
    if (m == 1) delay(2000);
    else if (m == 2) delay(1000);
  }
  Serial.println("=== Full sequence completed ===\n");
  printMenu();
}

void handleSerialInput() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  char cmd = line.charAt(0);
  if (cmd == '?' || cmd == 'h' || cmd == 'H') {
    printMenu();
    return;
  }
  if (cmd == 'a' || cmd == 'A') {
    runAllTests();
    return;
  }

  int mode = line.toInt();
  if (mode >= 1 && mode <= 9) {
    Serial.printf("\n>>> Mode %d\n", mode);
    runTest(mode);
    return;
  }

  Serial.printf("Invalid command: \"%s\"\n", line.c_str());
  printMenu();
}

void setup() {
  Serial.begin(BAUD_RATE);

  ledInit();
  ledAllOff();

  Serial.println("=== Stamp2 LED Device Test ===");
  Serial.println("Testing TPIC6B595 LED Control");
  Serial.println("Pin Configuration:");
  Serial.printf("  DATA: GPIO %d\n", STAMP2_DATA_PIN);
  Serial.printf("  CLK:  GPIO %d\n", STAMP2_CLK_PIN);
  Serial.printf("  LATCH: GPIO %d\n", STAMP2_LATCH_PIN);
  Serial.println("\nSend a test mode via Serial Monitor (115200).\n");
  printMenu();
  ledWrite16(0xFFFF); // 全部ON
  //ledWrite16(0x0000); // 全部OFF

}

void loop() {
  //handleSerialInput();
  ledWrite16(0xFFFF); // 全部ON
  delay(1000);
  ledWrite16(0x0000); // 全部OFF
  delay(1000);

}
