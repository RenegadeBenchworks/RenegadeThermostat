#include <Arduino.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7796S_kbv.h>
#include <XPT2046_Touchscreen.h>

// Display pins
static constexpr uint8_t TFT_CS_PIN = D10;
static constexpr uint8_t TFT_DC_PIN = D8;
static constexpr uint8_t TFT_RST_PIN = D9;
static constexpr uint32_t TFT_SPI_HZ = 4000000;
static constexpr uint8_t TFT_SCK_PIN = D13;
static constexpr uint8_t TFT_MISO_PIN = D12;
static constexpr uint8_t TFT_MOSI_PIN = D11;

// Touch pins
static constexpr uint8_t TOUCH_CS_PIN = A7;
static constexpr uint8_t TOUCH_IRQ_PIN = A4;

// Sensor pin
static constexpr uint8_t TEMP_SENSOR_PIN = D7;

static_assert(TEMP_SENSOR_PIN != TFT_CS_PIN &&
              TEMP_SENSOR_PIN != TFT_DC_PIN &&
              TEMP_SENSOR_PIN != TFT_RST_PIN,
              "TEMP_SENSOR_PIN must not reuse TFT control pins.");

static_assert(TEMP_SENSOR_PIN != TFT_SCK_PIN &&
              TEMP_SENSOR_PIN != TFT_MISO_PIN &&
              TEMP_SENSOR_PIN != TFT_MOSI_PIN,
              "TEMP_SENSOR_PIN must not reuse TFT SPI bus pins.");

// Relay pins
static constexpr uint8_t HEAT_RELAY_PIN = D4;
static constexpr uint8_t COOL_RELAY_PIN = D5;
static constexpr uint8_t HIGH_FAN_RELAY_PIN = D3;
static constexpr uint8_t LOW_FAN_RELAY_PIN = D2;
static constexpr uint8_t HEAT_FEEDBACK_PIN = A3;
static constexpr uint8_t COOL_FEEDBACK_PIN = A2;

static_assert(HEAT_RELAY_PIN != TFT_CS_PIN && HEAT_RELAY_PIN != TFT_DC_PIN && HEAT_RELAY_PIN != TFT_RST_PIN &&
              COOL_RELAY_PIN != TFT_CS_PIN && COOL_RELAY_PIN != TFT_DC_PIN && COOL_RELAY_PIN != TFT_RST_PIN &&
              HIGH_FAN_RELAY_PIN != TFT_CS_PIN && HIGH_FAN_RELAY_PIN != TFT_DC_PIN && HIGH_FAN_RELAY_PIN != TFT_RST_PIN &&
              LOW_FAN_RELAY_PIN != TFT_CS_PIN && LOW_FAN_RELAY_PIN != TFT_DC_PIN && LOW_FAN_RELAY_PIN != TFT_RST_PIN,
              "Relay pins must not reuse TFT control pins.");

static_assert(HEAT_RELAY_PIN != TFT_SCK_PIN && HEAT_RELAY_PIN != TFT_MISO_PIN && HEAT_RELAY_PIN != TFT_MOSI_PIN &&
              COOL_RELAY_PIN != TFT_SCK_PIN && COOL_RELAY_PIN != TFT_MISO_PIN && COOL_RELAY_PIN != TFT_MOSI_PIN &&
              HIGH_FAN_RELAY_PIN != TFT_SCK_PIN && HIGH_FAN_RELAY_PIN != TFT_MISO_PIN && HIGH_FAN_RELAY_PIN != TFT_MOSI_PIN &&
              LOW_FAN_RELAY_PIN != TFT_SCK_PIN && LOW_FAN_RELAY_PIN != TFT_MISO_PIN && LOW_FAN_RELAY_PIN != TFT_MOSI_PIN,
              "Relay pins must not reuse TFT SPI bus pins.");

static_assert(TOUCH_CS_PIN != TFT_CS_PIN &&
              TOUCH_CS_PIN != TFT_DC_PIN &&
              TOUCH_CS_PIN != TFT_RST_PIN,
              "TOUCH_CS_PIN must not reuse TFT control pins.");

static_assert(TOUCH_CS_PIN != TFT_SCK_PIN &&
              TOUCH_CS_PIN != TFT_MISO_PIN &&
              TOUCH_CS_PIN != TFT_MOSI_PIN,
              "TOUCH_CS_PIN must not reuse TFT SPI bus pins.");

static_assert(TOUCH_CS_PIN != HEAT_FEEDBACK_PIN &&
              TOUCH_CS_PIN != COOL_FEEDBACK_PIN,
              "TOUCH_CS_PIN must not reuse relay feedback pins.");

// OneWire on ESP32 uses raw GPIO numbers internally.
// On Nano ESP32 with pin remap enabled, physical D7 corresponds to GPIO10.
#if defined(ARDUINO_ARCH_ESP32) && defined(ARDUINO_NANO_ESP32) && defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)
static constexpr uint8_t TEMP_SENSOR_ONEWIRE_PIN = 10; // raw GPIO for header pin D7
#else
static constexpr uint8_t TEMP_SENSOR_ONEWIRE_PIN = TEMP_SENSOR_PIN;
#endif

// Transistor-base relay drive is typically active-high from the MCU pin.
// Can be toggled at runtime via serial command.
static bool relayActiveHigh = true;
// Disable active-low open-drain by default for transistor-base interfaces.
static constexpr bool RELAY_ACTIVE_LOW_OPEN_DRAIN = false;

Adafruit_ST7796S_kbv tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
// Use the library-specific pin value for OneWire bus init.
OneWire oneWire(TEMP_SENSOR_ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

bool heatOn = false;
bool coolOn = false;
bool highFanOn = false;
bool lowFanOn = false;
bool touchStreamEnabled = false;

static inline bool relayDriveIsHigh(bool commandOn) {
  return relayActiveHigh ? commandOn : !commandOn;
}

static inline void writeRelay(uint8_t pin, bool on) {
  if (!relayActiveHigh && RELAY_ACTIVE_LOW_OPEN_DRAIN) {
    if (on) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    } else {
      pinMode(pin, INPUT);
    }
    return;
  }

  pinMode(pin, OUTPUT);
  const bool level = relayActiveHigh ? on : !on;
  digitalWrite(pin, level ? HIGH : LOW);
}

static inline bool relayFeedbackClosed(uint8_t feedbackPin) {
  // Main app wiring uses INPUT_PULLUP with active-LOW relay aux contacts.
  return digitalRead(feedbackPin) == LOW;
}

void applyRelayStates() {
  writeRelay(HEAT_RELAY_PIN, heatOn);
  writeRelay(COOL_RELAY_PIN, coolOn);
  writeRelay(HIGH_FAN_RELAY_PIN, highFanOn);
  writeRelay(LOW_FAN_RELAY_PIN, lowFanOn);
}

void printRelayStateLine() {
  const bool heatClosed = relayFeedbackClosed(HEAT_FEEDBACK_PIN);
  const bool coolClosed = relayFeedbackClosed(COOL_FEEDBACK_PIN);
  const bool heatOk = (heatOn == heatClosed);
  const bool coolOk = (coolOn == coolClosed);

  Serial.printf("Relays | HEAT:%s fb:%s %s | COOL:%s fb:%s %s | FAN_H:%s FAN_L:%s\n",
                heatOn ? "ON" : "OFF",
                heatClosed ? "CLOSED" : "OPEN",
                heatOk ? "OK" : "MISMATCH",
                coolOn ? "ON" : "OFF",
                coolClosed ? "CLOSED" : "OPEN",
                coolOk ? "OK" : "MISMATCH",
                highFanOn ? "ON" : "OFF",
                lowFanOn ? "ON" : "OFF");
}

void printRelayGpioStateLine(const char* label, uint8_t pin, bool commandOn) {
  const bool expectedHigh = relayDriveIsHigh(commandOn);
  const int readback = digitalRead(pin);
  Serial.printf("GPIO %-6s pin:%u cmd:%s exp:%s read:%s (%d)\n",
                label,
                (unsigned)pin,
                commandOn ? "ON" : "OFF",
                expectedHigh ? "HIGH" : "LOW",
                (readback == HIGH) ? "HIGH" : "LOW",
                readback);
}

void printRelayGpioSnapshot() {
  Serial.println("Relay GPIO snapshot (requested command vs output level):");
  Serial.printf("Relay polarity: %s (ON drives %s)\n",
                relayActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW",
                relayActiveHigh ? "HIGH" : "LOW");
  if (!relayActiveHigh && RELAY_ACTIVE_LOW_OPEN_DRAIN) {
    Serial.println("Relay drive mode: ACTIVE_LOW_OPEN_DRAIN (OFF=high-impedance)");
  }
  printRelayGpioStateLine("HEAT", HEAT_RELAY_PIN, heatOn);
  printRelayGpioStateLine("COOL", COOL_RELAY_PIN, coolOn);
  printRelayGpioStateLine("FAN_H", HIGH_FAN_RELAY_PIN, highFanOn);
  printRelayGpioStateLine("FAN_L", LOW_FAN_RELAY_PIN, lowFanOn);
}

void printRelayPinMapping() {
  Serial.println("Relay pin mapping (Arduino pin -> raw GPIO):");
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("HEAT D%u -> GPIO %d\n", (unsigned)HEAT_RELAY_PIN, digitalPinToGPIONumber(HEAT_RELAY_PIN));
  Serial.printf("COOL D%u -> GPIO %d\n", (unsigned)COOL_RELAY_PIN, digitalPinToGPIONumber(COOL_RELAY_PIN));
  Serial.printf("FAN_H D%u -> GPIO %d\n", (unsigned)HIGH_FAN_RELAY_PIN, digitalPinToGPIONumber(HIGH_FAN_RELAY_PIN));
  Serial.printf("FAN_L D%u -> GPIO %d\n", (unsigned)LOW_FAN_RELAY_PIN, digitalPinToGPIONumber(LOW_FAN_RELAY_PIN));
#else
  Serial.println("Pin mapping helper only implemented for ESP32 targets.");
#endif
}

void toggleRelayPolarity() {
  relayActiveHigh = !relayActiveHigh;
  applyRelayStates();
  Serial.printf("Relay polarity toggled: %s (ON drives %s)\n",
                relayActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW",
                relayActiveHigh ? "HIGH" : "LOW");
  printRelayGpioSnapshot();
}

void runCoolRelayPinProbe() {
  Serial.println("COOL relay pin probe (D5): forcing raw HIGH then LOW...");

  const bool savedCoolOn = coolOn;
  pinMode(COOL_RELAY_PIN, OUTPUT);

  digitalWrite(COOL_RELAY_PIN, HIGH);
  delay(300);
  Serial.printf("D5 forced HIGH, readback=%s (%d)\n",
                digitalRead(COOL_RELAY_PIN) == HIGH ? "HIGH" : "LOW",
                digitalRead(COOL_RELAY_PIN));

  digitalWrite(COOL_RELAY_PIN, LOW);
  delay(300);
  Serial.printf("D5 forced LOW, readback=%s (%d)\n",
                digitalRead(COOL_RELAY_PIN) == HIGH ? "HIGH" : "LOW",
                digitalRead(COOL_RELAY_PIN));

  coolOn = savedCoolOn;
  applyRelayStates();
  Serial.println("COOL probe complete. Relay command state restored.");
}

void drawDisplayTestScreen() {
  // Ensure touch controller is deselected while writing to TFT on shared SPI bus.
  digitalWrite(TOUCH_CS_PIN, HIGH);
  tft.fillScreen(ST7796S_BLACK);
  tft.setRotation(1);

  tft.fillRect(0, 0, 480, 40, ST7796S_BLUE);
  tft.setTextColor(ST7796S_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.print("Thermostat Bring-Up Test");

  tft.fillRect(20, 60, 140, 80, ST7796S_RED);
  tft.fillRect(170, 60, 140, 80, ST7796S_GREEN);
  tft.fillRect(320, 60, 140, 80, ST7796S_CYAN);

  tft.drawRect(20, 160, 440, 140, ST7796S_WHITE);
  tft.setTextColor(ST7796S_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(30, 175);
  tft.print("Use Serial Monitor:");
  tft.setTextColor(ST7796S_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 205);
  tft.print("d=display  t=temp  1/2/3/4=relay");
  tft.setCursor(30, 230);
  tft.print("x=touch once  v=touch stream");
  tft.setCursor(30, 255);
  tft.print("a=all on  z=all off  b=blink");
}

void runDisplayColorTest() {
  Serial.println("Display: running solid color fill test...");
  digitalWrite(TOUCH_CS_PIN, HIGH);

  tft.setRotation(1);
  tft.fillScreen(ST7796S_RED);
  delay(500);
  tft.fillScreen(ST7796S_GREEN);
  delay(500);
  tft.fillScreen(ST7796S_BLUE);
  delay(500);
  tft.fillScreen(ST7796S_WHITE);
  delay(500);
  tft.fillScreen(ST7796S_BLACK);
  delay(250);

  drawDisplayTestScreen();
  Serial.println("Display: color fill test complete.");
}

void runDisplayModeSweep() {
  Serial.println("Display: mode sweep (MADCTL/COLMOD/inversion)...");
  digitalWrite(TOUCH_CS_PIN, HIGH);

  const uint8_t madctlModes[] = {
    0x48, // MX + BGR
    0x40, // MX + RGB
    0x88, // MY + BGR
    0x80  // MY + RGB
  };
  const uint8_t colmodModes[] = {
    0x55, // 16-bit RGB565
    0x66  // 18-bit RGB666
  };

  for (uint8_t i = 0; i < sizeof(madctlModes); ++i) {
    for (uint8_t j = 0; j < sizeof(colmodModes); ++j) {
      for (uint8_t inv = 0; inv < 2; ++inv) {
        tft.writeCommand(0x36); // MADCTL
        tft.spiWrite(madctlModes[i]);

        tft.writeCommand(0x3A); // COLMOD
        tft.spiWrite(colmodModes[j]);

        tft.writeCommand(inv ? 0x21 : 0x20); // INVON / INVOFF
        delay(10);

        Serial.printf("Display mode #%u MADCTL=0x%02X COLMOD=0x%02X INV=%s\n",
                      (unsigned)(i * 4 + j * 2 + inv + 1),
                      madctlModes[i],
                      colmodModes[j],
                      inv ? "ON" : "OFF");

        tft.fillScreen(ST7796S_RED);
        delay(250);
        tft.fillScreen(ST7796S_GREEN);
        delay(250);
        tft.fillScreen(ST7796S_BLUE);
        delay(250);
      }
    }
  }

  // Restore expected baseline.
  tft.writeCommand(0x36);
  tft.spiWrite(0x48);
  tft.writeCommand(0x3A);
  tft.spiWrite(0x55);
  tft.writeCommand(0x20);
  delay(5);

  drawDisplayTestScreen();
  Serial.println("Display: mode sweep complete.");
}

void reinitializeDisplay() {
  Serial.println("Display: reinitializing TFT...");

  digitalWrite(TOUCH_CS_PIN, HIGH);

  // Some ST7796S boards need an explicit hardware reset pulse after bus activity.
  pinMode(TFT_RST_PIN, OUTPUT);
  digitalWrite(TFT_RST_PIN, LOW);
  delay(20);
  digitalWrite(TFT_RST_PIN, HIGH);
  delay(120);

  tft.begin(TFT_SPI_HZ);
  // Force 16-bit/pixel mode; some ST7796S clones power up in a different format.
  tft.writeCommand(0x3A);
  tft.spiWrite(0x55); // COLMOD: 16-bit (RGB565)
  delay(5);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  drawDisplayTestScreen();
  Serial.printf("Display: init complete at %lu Hz SPI.\n", (unsigned long)TFT_SPI_HZ);
}

void printMenu() {
  Serial.println();
  Serial.println("=== Board Bring-Up Menu ===");
  Serial.println("m : show this menu");
  Serial.println("d : redraw display test");
  Serial.println("u : reinitialize TFT + redraw display test");
  Serial.println("y : run TFT solid color fill test");
  Serial.println("j : run TFT mode sweep diagnostic");
  Serial.println("t : read temperature now");
  Serial.println("s : scan candidate pins for DS18B20 bus/device");
  Serial.println("x : read touch now");
  Serial.println("v : toggle live touch stream");
  Serial.println("q : run QA sequence with PASS/FAIL summary");
  Serial.println("1 : toggle HEAT relay");
  Serial.println("2 : toggle COOL relay");
  Serial.println("3 : toggle HIGH FAN relay");
  Serial.println("4 : toggle LOW FAN relay");
  Serial.println("f : read relay feedback now");
  Serial.println("g : print relay GPIO output snapshot");
  Serial.println("n : print relay pin mapping (Dn -> raw GPIO)");
  Serial.println("o : toggle relay polarity active-high/active-low");
  Serial.println("k : force-probe COOL relay pin (D5)");
  Serial.println("p : timed HEAT/COOL feedback PASS/FAIL test");
  Serial.println("a : all relays ON");
  Serial.println("z : all relays OFF");
  Serial.println("b : relay chase test");
  Serial.println();
}

void readAndPrintTouch(bool includeNotPressed = true) {
  const bool pressed = touch.touched();
  if (!pressed) {
    if (includeNotPressed) {
      Serial.println("Touch: not pressed");
    }
    return;
  }

  const TS_Point p = touch.getPoint();
  Serial.printf("Touch: pressed rawX=%d rawY=%d z=%d\n", p.x, p.y, p.z);
}

void printRomCode(const uint8_t addr[8]) {
  for (uint8_t i = 0; i < 8; ++i) {
    Serial.printf("%02X", addr[i]);
    if (i < 7) {
      Serial.print(":");
    }
  }
}

void scanOneWireCandidate(const char* label, uint8_t pin) {
  OneWire probe(pin);
  const bool presence = probe.reset();

  uint8_t addr[8] = {0};
  uint8_t deviceCount = 0;
  probe.reset_search();
  while (probe.search(addr)) {
    ++deviceCount;
  }

  Serial.printf("%s (pin %u) -> presence:%s devices:%u",
                label,
                (unsigned)pin,
                presence ? "YES" : "NO",
                (unsigned)deviceCount);

  if (deviceCount > 0) {
    probe.reset_search();
    if (probe.search(addr)) {
      Serial.print(" first_rom:");
      printRomCode(addr);
    }
  }
  Serial.println();
}

void runOneWirePinScan() {
  Serial.println("OneWire pin scan (looking for DS18B20)...");
  scanOneWireCandidate("TEMP_SENSOR_PIN macro", TEMP_SENSOR_PIN);
  scanOneWireCandidate("TEMP_SENSOR_ONEWIRE_PIN", TEMP_SENSOR_ONEWIRE_PIN);

  // Commonly confused header labels.
  scanOneWireCandidate("D6 macro", D6);
  scanOneWireCandidate("D7 macro", D7);
  scanOneWireCandidate("D8 macro", D8);
  scanOneWireCandidate("D10 macro", D10);

  // Raw GPIO values often used when remap translation is bypassed.
  scanOneWireCandidate("raw GPIO 6", 6);
  scanOneWireCandidate("raw GPIO 7", 7);
  scanOneWireCandidate("raw GPIO 8", 8);
  scanOneWireCandidate("raw GPIO 9", 9);
  scanOneWireCandidate("raw GPIO 10", 10);
  scanOneWireCandidate("raw GPIO 11", 11);
  scanOneWireCandidate("raw GPIO 12", 12);
  Serial.println();
}

bool waitForFeedbackState(uint8_t feedbackPin, bool shouldBeClosed, uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (relayFeedbackClosed(feedbackPin) == shouldBeClosed) {
      return true;
    }
    delay(10);
  }
  return relayFeedbackClosed(feedbackPin) == shouldBeClosed;
}

void runTimedFeedbackTest() {
  const uint32_t settleMs = 250;
  const uint32_t holdMs = 2000;
  const uint32_t timeoutMs = 1500;

  Serial.println("Running timed relay feedback test...");

  heatOn = coolOn = highFanOn = lowFanOn = false;
  applyRelayStates();
  delay(settleMs);

  bool heatPass = true;
  bool coolPass = true;

  // HEAT ON -> feedback CLOSED, then OFF -> OPEN
  heatOn = true;
  applyRelayStates();
  heatPass &= waitForFeedbackState(HEAT_FEEDBACK_PIN, true, timeoutMs);
  delay(holdMs);
  heatOn = false;
  applyRelayStates();
  heatPass &= waitForFeedbackState(HEAT_FEEDBACK_PIN, false, timeoutMs);

  // COOL ON -> feedback CLOSED, then OFF -> OPEN
  coolOn = true;
  applyRelayStates();
  coolPass &= waitForFeedbackState(COOL_FEEDBACK_PIN, true, timeoutMs);
  delay(holdMs);
  coolOn = false;
  applyRelayStates();
  coolPass &= waitForFeedbackState(COOL_FEEDBACK_PIN, false, timeoutMs);

  printRelayStateLine();
  Serial.printf("Timed feedback result | HEAT:%s COOL:%s OVERALL:%s\n",
                heatPass ? "PASS" : "FAIL",
                coolPass ? "PASS" : "FAIL",
                (heatPass && coolPass) ? "PASS" : "FAIL");
}

bool runTimedFeedbackTestAndReturn() {
  const uint32_t settleMs = 250;
  const uint32_t holdMs = 2000;
  const uint32_t timeoutMs = 1500;

  Serial.println("Running timed relay feedback test...");

  heatOn = coolOn = highFanOn = lowFanOn = false;
  applyRelayStates();
  delay(settleMs);

  bool heatPass = true;
  bool coolPass = true;

  heatOn = true;
  applyRelayStates();
  heatPass &= waitForFeedbackState(HEAT_FEEDBACK_PIN, true, timeoutMs);
  delay(holdMs);
  heatOn = false;
  applyRelayStates();
  heatPass &= waitForFeedbackState(HEAT_FEEDBACK_PIN, false, timeoutMs);

  coolOn = true;
  applyRelayStates();
  coolPass &= waitForFeedbackState(COOL_FEEDBACK_PIN, true, timeoutMs);
  delay(holdMs);
  coolOn = false;
  applyRelayStates();
  coolPass &= waitForFeedbackState(COOL_FEEDBACK_PIN, false, timeoutMs);

  const bool overall = heatPass && coolPass;
  printRelayStateLine();
  Serial.printf("Timed feedback result | HEAT:%s COOL:%s OVERALL:%s\n",
                heatPass ? "PASS" : "FAIL",
                coolPass ? "PASS" : "FAIL",
                overall ? "PASS" : "FAIL");
  return overall;
}

bool runTemperatureQaTest() {
  sensors.requestTemperatures();
  const float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Temp QA: FAIL (DS18B20 disconnected)");
    return false;
  }

  const float tempF = tempC * 9.0f / 5.0f + 32.0f;
  Serial.printf("Temp QA: PASS (%.2f C / %.2f F)\n", tempC, tempF);
  return true;
}

bool runTouchQaTest(uint32_t timeoutMs) {
  Serial.printf("Touch QA: touch panel within %lu ms...\n", (unsigned long)timeoutMs);
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (touch.touched()) {
      const TS_Point p = touch.getPoint();
      Serial.printf("Touch QA: PASS rawX=%d rawY=%d z=%d\n", p.x, p.y, p.z);
      return true;
    }
    delay(20);
  }

  Serial.println("Touch QA: FAIL (no touch detected)");
  return false;
}

void runQaSequence() {
  Serial.println();
  Serial.println("=== QA Sequence Start ===");

  // Display is a visual checkpoint (cannot be fully auto-validated).
  drawDisplayTestScreen();
  Serial.println("Display QA: CHECK (screen redrawn; verify visually)");

  const bool tempPass = runTemperatureQaTest();
  const bool relayPass = runTimedFeedbackTestAndReturn();
  const bool touchPass = runTouchQaTest(5000);

  const bool overallPass = tempPass && relayPass && touchPass;
  Serial.println("=== QA Summary ===");
  Serial.printf("Display: CHECK\n");
  Serial.printf("Temp: %s\n", tempPass ? "PASS" : "FAIL");
  Serial.printf("Relays+Feedback: %s\n", relayPass ? "PASS" : "FAIL");
  Serial.printf("Touch: %s\n", touchPass ? "PASS" : "FAIL");
  Serial.printf("OVERALL (auto checks): %s\n", overallPass ? "PASS" : "FAIL");
  Serial.println("=== QA Sequence End ===");
  Serial.println();
}

void readAndPrintTemp() {
  sensors.requestTemperatures();
  const float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("DS18B20: not found / disconnected");
    return;
  }

  const float tempF = tempC * 9.0f / 5.0f + 32.0f;
  Serial.printf("DS18B20: %.2f C  %.2f F\n", tempC, tempF);
}

void relayChaseTest() {
  Serial.println("Running relay chase test...");

  heatOn = coolOn = highFanOn = lowFanOn = false;
  applyRelayStates();

  heatOn = true; applyRelayStates(); printRelayStateLine(); delay(600);
  heatOn = false; coolOn = true; applyRelayStates(); printRelayStateLine(); delay(600);
  coolOn = false; highFanOn = true; applyRelayStates(); printRelayStateLine(); delay(600);
  highFanOn = false; lowFanOn = true; applyRelayStates(); printRelayStateLine(); delay(600);

  heatOn = coolOn = highFanOn = lowFanOn = false;
  applyRelayStates();
  printRelayStateLine();
  Serial.println("Relay chase complete.");
}

void handleCommand(char c) {
  switch (c) {
    case 'm':
      printMenu();
      break;
    case 'd':
      drawDisplayTestScreen();
      Serial.println("Display test screen redrawn.");
      break;
    case 'u':
      reinitializeDisplay();
      break;
    case 'y':
      runDisplayColorTest();
      break;
    case 'j':
      runDisplayModeSweep();
      break;
    case 't':
      readAndPrintTemp();
      break;
    case 's':
      runOneWirePinScan();
      break;
    case 'x':
      readAndPrintTouch(true);
      break;
    case 'v':
      touchStreamEnabled = !touchStreamEnabled;
      Serial.printf("Touch stream: %s\n", touchStreamEnabled ? "ON" : "OFF");
      break;
    case 'q':
      runQaSequence();
      break;
    case '1':
      heatOn = !heatOn;
      applyRelayStates();
      printRelayStateLine();
      break;
    case '2':
      coolOn = !coolOn;
      applyRelayStates();
      printRelayStateLine();
      break;
    case '3':
      highFanOn = !highFanOn;
      applyRelayStates();
      printRelayStateLine();
      break;
    case '4':
      lowFanOn = !lowFanOn;
      applyRelayStates();
      printRelayStateLine();
      break;
    case 'f':
      printRelayStateLine();
      break;
    case 'g':
      printRelayGpioSnapshot();
      break;
    case 'n':
      printRelayPinMapping();
      break;
    case 'o':
      toggleRelayPolarity();
      break;
    case 'k':
      runCoolRelayPinProbe();
      break;
    case 'a':
      heatOn = coolOn = highFanOn = lowFanOn = true;
      applyRelayStates();
      printRelayStateLine();
      printRelayGpioSnapshot();
      break;
    case 'z':
      heatOn = coolOn = highFanOn = lowFanOn = false;
      applyRelayStates();
      printRelayStateLine();
      break;
    case 'b':
      relayChaseTest();
      break;
    case 'p':
      runTimedFeedbackTest();
      break;
    default:
      if (c != '\n' && c != '\r') {
        Serial.printf("Unknown command: %c\n", c);
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Explicitly bind the SPI peripheral to the header SPI pins used by TFT/touch.
  SPI.begin(TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN);

  pinMode(HEAT_RELAY_PIN, OUTPUT);
  pinMode(COOL_RELAY_PIN, OUTPUT);
  pinMode(HIGH_FAN_RELAY_PIN, OUTPUT);
  pinMode(LOW_FAN_RELAY_PIN, OUTPUT);
  pinMode(HEAT_FEEDBACK_PIN, INPUT_PULLUP);
  pinMode(COOL_FEEDBACK_PIN, INPUT_PULLUP);
  applyRelayStates();

  sensors.begin();
  const uint8_t deviceCount = sensors.getDeviceCount();

  // Shared SPI: hold both chip-select lines high before peripheral init.
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);

  touch.begin();
  touch.setRotation(1);

  reinitializeDisplay();

  Serial.println("Board bring-up test started.");
  Serial.printf("TEMP_SENSOR_PIN = D7 (macro value: %u)\n", (unsigned)TEMP_SENSOR_PIN);
  Serial.printf("TEMP_SENSOR_ONEWIRE_PIN used by OneWire: %u\n", (unsigned)TEMP_SENSOR_ONEWIRE_PIN);
  Serial.printf("Touch pins: CS=%u IRQ=%u\n", (unsigned)TOUCH_CS_PIN, (unsigned)TOUCH_IRQ_PIN);
  Serial.printf("SPI pins: SCK=%u MISO=%u MOSI=%u\n",
                (unsigned)TFT_SCK_PIN, (unsigned)TFT_MISO_PIN, (unsigned)TFT_MOSI_PIN);
  Serial.printf("Relay feedback pins: HEAT=%u COOL=%u (active LOW)\n",
                (unsigned)HEAT_FEEDBACK_PIN, (unsigned)COOL_FEEDBACK_PIN);
  Serial.printf("Relay drive polarity: %s\n", relayActiveHigh ? "ACTIVE_HIGH" : "ACTIVE_LOW");
  Serial.printf("DS18B20 devices found on bus: %u\n", (unsigned)deviceCount);
  runOneWirePinScan();
  printRelayPinMapping();
  printRelayStateLine();
  printRelayGpioSnapshot();
  printMenu();
}

void loop() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    handleCommand(c);
  }

  static uint32_t lastTempMs = 0;
  const uint32_t now = millis();
  if (now - lastTempMs >= 5000) {
    lastTempMs = now;
   // readAndPrintTemp();
  }

  static uint32_t lastTouchMs = 0;
  if (touchStreamEnabled && (now - lastTouchMs >= 100)) {
    lastTouchMs = now;
    readAndPrintTouch(false);
  }
}
