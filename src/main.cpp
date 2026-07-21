#include "iot_configs.h"

#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <cstdlib>
#include <math.h>
#include <string.h>
#include <time.h>

#include "Adafruit_GFX.h"
#include "Adafruit_ST7796S_kbv.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LocationWeather.h"
#include <TemperatureSensor.h>
#include "TemperatureControlSystem.h"
#include "SerialLogger.h"
#include "UIContext.h"
#include "UIManager.h"
#include "InfoPage.h"
#include "WeatherInfoPage.h"
#include "DeviceAuth.h"
#include "DeviceEnrollPage.h"
#include "ThermostatHomePage.h"
#include "EmbeddedWebServer.h"
#include "FirebaseSync.h"
#include "AppState.h"
#include "MenuPage.h"
#include "MenuItems.h"
#include "KeyboardPage.h"
#include "TouchInput_XPT2046.h"
#include "UITransition.h"
#include "UIStatusBar.h"
#include "SplashPage.h"
#include <Preferences.h>


// --------------------------------------------------
// Pin definitions
// --------------------------------------------------
#define TFT_CS    D10
#define TFT_DC    D8
#define TFT_RST   D9

#define TOUCH_CS   A2
#define TOUCH_IRQ  D5

#define BACKLIGHT_PIN A0
#define PRESENCE_PIN  A1
#define TEMP_SENSOR_PIN D10
// Relay auxiliary feedback inputs (active LOW via INPUT_PULLUP).
// Wire relay aux contact between pin and GND. Change to -1 to disable.
#define HIGH_FAN_RELAY_PIN A3
#define LOW_FAN_RELAY_PIN A4
#define HEAT_RELAY_PIN D6
#define COOL_RELAY_PIN D2
#define HEAT_FEEDBACK_PIN  D3
#define COOL_FEEDBACK_PIN  D4
// Set to 0 if your sensor is active-low.
#define PRESENCE_ACTIVE_HIGH 1

// --------------------------------------------------
// Globals
// --------------------------------------------------
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"
#define PST_TIME_ZONE -8
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define UNIX_TIME_NOV_13_2017 1510592825
#define TOUCH_DEBUG 0
#define TOUCH_USE_IRQ 0
#define TOUCH_UI_OFFSET_X 0
#define TOUCH_UI_OFFSET_Y 0
#define TOUCH_UI_OFFSET_Y_SETTINGS 0
#define TOUCH_UI_OFFSET_X_KEYBOARD 0
#define TOUCH_UI_OFFSET_Y_KEYBOARD 0
#define TOUCH_UI_SCALE_X_KEYBOARD_PCT 100
#define TOUCH_MAP_LOG 0
#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)
#ifndef FW_BUILD_NUMBER
#define FW_BUILD_NUMBER 0
#endif
static unsigned long next_weather_update_time_ms = 0;
static const uint32_t WEATHER_UPDATE_FREQUENCY_MS = 60UL * 60UL * 1000UL;
static const uint32_t WEATHER_RETRY_FREQUENCY_MS  = 60UL * 1000UL;
static const uint32_t WEATHER_BUSY_RECHECK_MS = 1000UL;
static const uint32_t TASK_WDT_TIMEOUT_S = 30;
// How often the DS18B20 is actually read. The DS18B20 takes ~750 ms per
// conversion; polling faster than this wastes time and returns stale values.
// Increase (e.g. 10000) to slow down how often the displayed temp can change.
static const uint32_t TEMP_SENSOR_POLL_MS = 5000UL;   // 5 seconds
static bool backlightOn = true;
static uint32_t lastUserActivityMs = 0;
static volatile bool presenceIrqPending = false;
static uint32_t lastPresenceIrqHandledMs = 0;
static const uint32_t PRESENCE_IRQ_DEBOUNCE_MS = 50UL;
RTC_DATA_ATTR static uint32_t bootCount = 0;
static esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
static bool showResetBannerOnBoot = false;
static bool wifiBannerVisible = false;
static char wifiBannerText[48] = "";
static bool wifiBannerIsError = true;
static uint32_t wifiBannerUntilMs = 0;
static const uint32_t backlightTimeoutMs = 300000UL; // 5 minutes
float latitude = 44.9630f;
float longitude = -92.9649f;

struct WeatherSnapshot {
  float outsideTempF;
  float dayHighF;
  float dayLowF;
  int humidity;
  float windMph;
  char description[32];
  char sunrise[16];
  char sunset[16];
};

static TaskHandle_t weatherTaskHandle = nullptr;
#if FIREBASE_ENABLED
static TaskHandle_t firebaseTaskHandle = nullptr;
#endif
static portMUX_TYPE weatherStateMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool weatherRequestPending = false;
static volatile bool weatherFetchInProgress = false;
static volatile bool weatherResultReady = false;
static bool weatherLastFetchSuccess = false;
static WeatherSnapshot weatherPending = {};

ModeContext modeContext(nullptr); 
static char wifiSsidRuntime[33] = IOT_CONFIG_WIFI_SSID;
static char wifiPassRuntime[65] = IOT_CONFIG_WIFI_PASSWORD;
static char wifiSsidEdit[33] = "";
static char wifiPassEdit[65] = "";
static const char* ssid = wifiSsidRuntime;
static const char* password = wifiPassRuntime;
LocationWeather currLocWeather(CONFIG_WEATHER_URL, CONFIG_WEATHER_KEY);
Adafruit_ST7796S_kbv tft(TFT_CS, TFT_DC, TFT_RST);
TouchInput_XPT2046 touch(TOUCH_CS, TOUCH_IRQ);
TemperatureSensor tempSensor(TEMP_SENSOR_PIN);
TemperatureController tempController(70, 50, 90, HEAT_RELAY_PIN, COOL_RELAY_PIN, HEAT_FEEDBACK_PIN, COOL_FEEDBACK_PIN, HIGH_FAN_RELAY_PIN, LOW_FAN_RELAY_PIN, &modeContext);

void IRAM_ATTR onTouchInterrupt() {
  touch.notifyInterrupt();
}

void IRAM_ATTR onPresenceInterrupt() {
  presenceIrqPending = true;
}

// Forward declarations
void openSettingsMenu(UIContext& ctx);
void onSettingsBack(UIContext& ctx);
void openAbout(UIContext& ctx);
void openWifiSsidEdit(UIContext& ctx);
void openWifiPassEdit(UIContext& ctx);
void reconnectWifiNow(UIContext& ctx);
void calibrateTouch(UIContext& ctx);

static void startWiFiConnect();
static void updateWiFiAsync();
static void applyWifiCredentialsAndReconnect();
static bool loadTouchCal(TouchInput_XPT2046::Cal& c);
static void populateStatusBar(UIStatusState& st);
static void mapTouchToUi(int16_t& x, int16_t& y);
static const char* currentPageName();
static const char* hvacModeToString(HvacMode mode);
static HvacMode hvacModeFromString(const String& modeStr);
static const char* resetReasonToString(esp_reset_reason_t reason);
static void weatherFetchTask(void*);
#if FIREBASE_ENABLED
static void firebaseSyncTask(void*);
#endif
static bool requestWeatherRefreshAsync();
static void processWeatherRefreshResult(uint32_t now);
void applyMode(HvacMode mode);
static void onWebHostToggle(bool v);
static void tickSchedule();
void loadSchedule();
void saveSchedule();
static void initTaskWatchdog();
static inline void feedTaskWatchdog();

// --------------------------------------------------
// UI state + pages
// --------------------------------------------------
ThermostatState appState;
bool webExternalMode = true; // persisted in NVS under "web"/"external"

InfoPage aboutPage("System", "Thermostat UI test page.");
WeatherInfoPage weatherInfoPage;
DeviceEnrollPage deviceEnrollPage;
MenuPage<8> settingsPage("Settings", onSettingsBack);
ThermostatHomePage homePage(appState, openSettingsMenu);
KeyboardPage wifiSsidPage("WiFi SSID", wifiSsidEdit, sizeof(wifiSsidEdit), false, nullptr);
KeyboardPage wifiPassPage("WiFi Password", wifiPassEdit, sizeof(wifiPassEdit), true, nullptr);

UIContext uiCtx(tft);
UIManager<6> ui(uiCtx);

ActionItem wifiSsidItem("WiFi SSID", openWifiSsidEdit);
ActionItem wifiPassItem("WiFi Password", openWifiPassEdit);
ActionItem reconnectWifiItem("Reconnect WiFi", reconnectWifiNow);
ActionItem aboutItem("System", openAbout);
ActionItem calibrateTouchItem("Calibrate Touch", calibrateTouch);
TogglePersistItem webHostItem("Web: External", &webExternalMode, onWebHostToggle);

// --------------------------------------------------
// WiFi async state
// --------------------------------------------------
// WiFi async state
// --------------------------------------------------
static uint32_t wifiRetryDelayMs = 10000;
static uint32_t wifiConnectTimeoutMs = 12000;
static uint32_t wifiGiveUpRetryMs = 60UL * 60UL * 1000UL; // 1 hour cooldown after 3 fails
static bool wifiAttemptInProgress = false;
static uint32_t wifiAttemptStartMs = 0;
static uint8_t  wifiFailCount = 0;       // consecutive failed attempts
static bool     wifiGaveUp    = false;   // true after 3 fails; cleared by credential update
static uint32_t wifiGaveUpAtMs = 0;

// --------------------------------------------------
// Helpers
// --------------------------------------------------
static bool pointInRect(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return (x >= rx && x < (rx + rw) && y >= ry && y < (ry + rh));
}

static void showWifiBanner(const char* text, uint32_t durationMs, bool isError = true) {
  strncpy(wifiBannerText, text, sizeof(wifiBannerText) - 1);
  wifiBannerText[sizeof(wifiBannerText) - 1] = '\0';
  wifiBannerIsError = isError;
  wifiBannerVisible = true;
  wifiBannerUntilMs = millis() + durationMs;
}

static void updateWifiBanner(uint32_t now) {
  if (!wifiBannerVisible) return;

  const auto& th = uiCtx.theme;
  const int bw = (int)uiCtx.width - (int)th.pad * 2;
  const int bh = 24;
  const int bx = (int)th.pad;
  const int by = (int)uiCtx.height - bh - (int)th.pad;

  if ((int32_t)(now - wifiBannerUntilMs) >= 0) {
    wifiBannerVisible = false;

    // In Settings, first clear the banner strip (otherwise parts can persist
    // because MenuPage dirty rendering does not clear empty background below
    // rows), then invalidate so overlapped row borders are repainted.
    if (ui.current() == &settingsPage) {
      uiCtx.display.fillRect(bx - 1, by - 1, bw + 2, bh + 2, th.bg);
    }

    // Redraw exactly the area under the banner so overlapped UI (like the
    // bottom edge of the last Settings row) is restored correctly.
    uiCtx.invalidateRect(bx - 1, by - 1, bw + 2, bh + 2);
    return;
  }

  auto& d = uiCtx.display;

  uint16_t bannerColor = wifiBannerIsError ? th.danger : th.accent;
  d.fillRoundRect(bx, by, bw, bh, 8, bannerColor);
  d.drawRoundRect(bx, by, bw, bh, 8, th.highlight);
  d.setTextSize(1);
  d.setTextWrap(false);
  d.setTextColor(th.bg);
  d.setCursor(bx + 8, by + 8);
  d.print(wifiBannerText);
}

static void mapTouchToUi(int16_t& x, int16_t& y) {
  // Centralized mapping lets us tune global touch alignment in one place.
  int xOff = TOUCH_UI_OFFSET_X;
  int yOff = TOUCH_UI_OFFSET_Y;

  UIPage* page = ui.current();
  if (page == &settingsPage) {
    yOff += TOUCH_UI_OFFSET_Y_SETTINGS;
  }

  if (page == &wifiSsidPage || page == &wifiPassPage) {
    // Keyboard-specific affine mapping compensates for panel compression.
    int32_t centeredX = (int32_t)x - (uiCtx.width / 2);
    centeredX = (centeredX * TOUCH_UI_SCALE_X_KEYBOARD_PCT) / 100;
    x = (int16_t)((uiCtx.width / 2) + centeredX);
    xOff += TOUCH_UI_OFFSET_X_KEYBOARD;
    yOff += TOUCH_UI_OFFSET_Y_KEYBOARD;
  }

  x = (int16_t)(x + xOff);
  y = (int16_t)(y + yOff);
  x = constrain(x, 0, uiCtx.width - 1);
  y = constrain(y, 0, uiCtx.height - 1);
}

static const char* currentPageName() {
  UIPage* p = ui.current();
  if (p == &homePage) return "home";
  if (p == &settingsPage) return "settings";
  if (p == &aboutPage) return "system";
  if (p == &wifiSsidPage) return "wifi-ssid";
  if (p == &wifiPassPage) return "wifi-pass";
  if (p == &weatherInfoPage) return "weather-info";
  if (p == &deviceEnrollPage) return "enroll";
  return "unknown";
}

static const char* hvacModeToString(HvacMode mode) {
  switch (mode) {
    case HvacMode::Heat: return "Heat";
    case HvacMode::Cool: return "Cool";
    case HvacMode::Fan:  return "Fan";
    case HvacMode::Off:
    default:             return "Off";
  }
}

static HvacMode hvacModeFromString(const String& modeStr) {
  if (modeStr.equalsIgnoreCase("HEAT")) return HvacMode::Heat;
  if (modeStr.equalsIgnoreCase("COOL")) return HvacMode::Cool;
  if (modeStr.equalsIgnoreCase("FAN"))  return HvacMode::Fan;
  return HvacMode::Off;
}

static const char* resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "OTHER";
  }
}

void applyMode(HvacMode mode) {
  appState.mode = mode;
  const char* modeName = hvacModeToString(mode);
  modeContext.setModeFromAzure(String(modeName));
  homePage.markModeDirty();
}

static void onWebHostToggle(bool v) {
  Preferences prefs;
  if (prefs.begin("web", false)) {
    prefs.putBool("external", v);
    prefs.end();
  }
}

// --------------------------------------------------
// Schedule persistence + tick
// --------------------------------------------------
static uint16_t schedLastKey = 0xFFFF; // tracks last applied period key

void saveSchedule() {
  JsonDocument doc;
  doc["enabled"] = appState.schedule.enabled;
  JsonArray days = doc["days"].to<JsonArray>();
  for (int d = 0; d < 7; d++) {
    JsonObject day = days.add<JsonObject>();
    auto addP = [&](const char* k, const SchedulePeriod& p) {
      JsonObject obj = day[k].to<JsonObject>();
      obj["h"]   = p.startHour;
      obj["min"] = p.startMinute;
      obj["sp"]  = p.setpointF;
    };
    addP("morning", appState.schedule.days[d].morning);
    addP("day",     appState.schedule.days[d].day);
    addP("night",   appState.schedule.days[d].night);
  }
  String json;
  serializeJson(doc, json);
  Preferences prefs;
  if (prefs.begin("sched", false)) {
    prefs.putString("json", json);
    prefs.end();
    Logger.Info("Schedule saved");
  }
#if FIREBASE_ENABLED
  FirebaseSync::notifyScheduleChanged();
#endif
}

void loadSchedule() {
  Preferences prefs;
  if (!prefs.begin("sched", true)) return;
  String json = prefs.getString("json", "");
  prefs.end();
  if (json.length() == 0) return;

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    Logger.Error("Schedule JSON parse error");
    return;
  }
  if (!doc["days"].is<JsonArray>() || doc["days"].as<JsonArray>().size() != 7) return;

  appState.schedule.enabled = doc["enabled"] | false;
  JsonArray days = doc["days"].as<JsonArray>();
  for (int d = 0; d < 7; d++) {
    auto loadP = [&](const char* k, SchedulePeriod& p) {
      JsonObject obj = days[d][k];
      if (!obj.isNull()) {
        p.startHour   = (uint8_t)(obj["h"]   | (int)p.startHour);
        p.startMinute = (uint8_t)(obj["min"]  | (int)p.startMinute);
        p.setpointF   = obj["sp"] | p.setpointF;
      }
    };
    loadP("morning", appState.schedule.days[d].morning);
    loadP("day",     appState.schedule.days[d].day);
    loadP("night",   appState.schedule.days[d].night);
  }
  Logger.Info("Schedule loaded from NVS");
}

// Called every ~60 s. When schedule is enabled, applies the scheduled setpoint
// whenever the time period changes (morning/day/night). Manual setpoint changes
// made within a period are kept until the next period boundary.
static void tickSchedule() {
  if (!appState.schedule.enabled) return;

  time_t nowTime = time(nullptr);
  // UNIX_TIME_NOV_13_2017 used as a sentinel: if time is less, NTP hasn't synced yet.
  if (nowTime < UNIX_TIME_NOV_13_2017) return;

  struct tm tmBuf;
  localtime_r(&nowTime, &tmBuf);
  // tm_wday: 0=Sun, 1=Mon ... 6=Sat — convert to 0=Mon ... 6=Sun
  int day = (tmBuf.tm_wday + 6) % 7;

  const ScheduleDay& sday = appState.schedule.days[day];
  const SchedulePeriod* periods[3] = { &sday.morning, &sday.day, &sday.night };

  int totalMinutes = tmBuf.tm_hour * 60 + tmBuf.tm_min;

  // Find current period: latest whose start time <= current time
  int currentPeriod = -1;
  for (int i = 2; i >= 0; i--) {
    int pMin = (int)periods[i]->startHour * 60 + (int)periods[i]->startMinute;
    if (totalMinutes >= pMin) {
      currentPeriod = i;
      break;
    }
  }

  uint16_t key;
  float targetSp;
  if (currentPeriod < 0) {
    // Before the first period of the day — use previous day's night setpoint.
    int prevDay = (day + 6) % 7;
    key      = (uint16_t)(prevDay * 10 + 9); // 9 = "prev-night"
    targetSp = appState.schedule.days[prevDay].night.setpointF;
  } else {
    key      = (uint16_t)(day * 10 + currentPeriod);
    targetSp = periods[currentPeriod]->setpointF;
  }

  if (key != schedLastKey) {
    schedLastKey = key;
    appState.setpointF = targetSp;
    tempController.updateTemperatureSettingFromAzure((int)targetSp);
    homePage.markSetpointDirty();
    uiCtx.invalidateAll();
    Logger.Info("Schedule applied new setpoint");
  }
}

static void waitForTouchRelease() {
  // Do NOT call ui.tick() here: after push/pop the new page is already fully
  // rendered; calling update()+renderDirty() ~200x/sec during release detection
  // is wasteful and can cause flicker on pages that animate.
  while (touch.isPressed()) {
    feedTaskWatchdog();
    delay(5);
  }
  Logger.Verbose("touch released");
}

static void cycleMode() {
  HvacMode nextMode = appState.mode;
  switch (appState.mode) {
    case HvacMode::Off:  nextMode = HvacMode::Heat; break;
    case HvacMode::Heat: nextMode = HvacMode::Cool; break;
    case HvacMode::Cool:
    case HvacMode::Fan:
    default:             nextMode = HvacMode::Off;  break;
  }
  applyMode(nextMode);
}

static void setBacklight(bool on) {
  if (on == backlightOn) return;

  backlightOn = on;
  digitalWrite(BACKLIGHT_PIN, on ? HIGH : LOW);

  Serial.print("Backlight ");
  Serial.println(on ? "ON" : "OFF");
}

static bool presenceDetected() {
  int raw = digitalRead(PRESENCE_PIN);

#if PRESENCE_ACTIVE_HIGH
  bool levelActive = (raw == HIGH);
#else
  bool levelActive = (raw == LOW);
#endif

  bool edgeActive = false;
  bool hadIrq = false;
  noInterrupts();
  hadIrq = presenceIrqPending;
  presenceIrqPending = false;
  interrupts();

  if (hadIrq) {
    uint32_t now = millis();
    if (now - lastPresenceIrqHandledMs >= PRESENCE_IRQ_DEBOUNCE_MS) {
      edgeActive = true;
      lastPresenceIrqHandledMs = now;
    }
  }

  return levelActive || edgeActive;
}

static void registerUserActivity() {
  lastUserActivityMs = millis();
  setBacklight(true);
}

static void updateBacklightManager() {
  uint32_t now = millis();

  // Presence immediately wakes / keeps alive
  if (presenceDetected()) {
    lastUserActivityMs = now;
    setBacklight(true);
  }

  // Timeout turns display off
  if (backlightOn && (now - lastUserActivityMs >= backlightTimeoutMs)) {
    setBacklight(false);
  }
}

static void populateStatusBar(UIStatusState& st) {
  st.leftText = "";

  switch (appState.wifiState) {
    case WifiState::Connected:
      st.wifi = true;
      st.rssi = WiFi.RSSI();
      // Show Settings label in status bar when on the home page.
      st.rightText = (ui.current() == &homePage) ? "Settings" : "";
      break;
    case WifiState::Connecting:
      st.wifi = false;
      st.rssi = 0;
      st.rightText = (ui.current() == &homePage) ? "Settings" : "";
      break;
    case WifiState::Failed:
      st.wifi = false;
      st.rssi = 0;
      st.rightText = (ui.current() == &homePage) ? "Settings" : "";
      break;
    case WifiState::Idle:
    default:
      st.wifi = false;
      st.rssi = 0;
      st.rightText = (ui.current() == &homePage) ? "Settings" : "";
      break;
  }
}


// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  bootResetReason = esp_reset_reason();
  ++bootCount;
  const char* resetReason = resetReasonToString(bootResetReason);
  Serial.printf("Boot #%lu reset reason: %s (%d)\n", (unsigned long)bootCount, resetReason, (int)bootResetReason);
  showResetBannerOnBoot = (bootResetReason != ESP_RST_POWERON && bootResetReason != ESP_RST_UNKNOWN);

  initTaskWatchdog();
  feedTaskWatchdog();
  delay(200);
  feedTaskWatchdog();
  tft.begin(27000000); // use a stable SPI clock for the TFT panel
  tft.setRotation(1);

  pinMode(HEAT_FEEDBACK_PIN, INPUT_PULLUP);
  pinMode(COOL_FEEDBACK_PIN, INPUT_PULLUP);
  pinMode(COOL_RELAY_PIN, OUTPUT);
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  pinMode(HIGH_FAN_RELAY_PIN, OUTPUT);
  pinMode(LOW_FAN_RELAY_PIN , OUTPUT);
  pinMode(COOL_FEEDBACK_PIN, INPUT_PULLUP);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  backlightOn = true;

  // Show splash screen for 2 seconds on boot.
  SplashPage::show(uiCtx, 2000);
  feedTaskWatchdog();

  // Keep default state LOW when sensor output is idle/disconnected.
  pinMode(PRESENCE_PIN, INPUT_PULLDOWN);
  int presenceIrq = digitalPinToInterrupt(PRESENCE_PIN);
  if (presenceIrq != NOT_AN_INTERRUPT) {
    attachInterrupt(presenceIrq, onPresenceInterrupt, CHANGE);
  } else {
    Serial.println("Presence pin has no interrupt support; using polling fallback.");
  }

  tempSensor.begin();


  lastUserActivityMs = millis();

  uiCtx.width = tft.width();
  uiCtx.height = tft.height();
  uiCtx.invalidateAll();
  // startup icon test removed

  // Initial thermostat state
  appState.roomTempF = tempSensor.getTemperatureF();
  appState.setpointF = (float)tempController.getTemperatureSetting();
  appState.mode = HvacMode::Off;
  appState.fanSpeed = FanSpeed::Auto;
  tempController.setFanSpeed(appState.fanSpeed);
  applyMode(appState.mode);
  appState.adjustingSetpoint = false;

  appState.weather.outsideTempF = 0.0f;
  appState.weather.dayHighF = 0.0f;
  appState.weather.dayLowF = 0.0f;
  appState.weather.humidity = 0;
  appState.weather.windMph = 0.0f;
  strncpy(appState.weather.description, "Updating...", sizeof(appState.weather.description) - 1);
  appState.weather.description[sizeof(appState.weather.description) - 1] = '\0';

  strncpy(appState.weather.sunrise, "--:--", sizeof(appState.weather.sunrise) - 1);
  appState.weather.sunrise[sizeof(appState.weather.sunrise) - 1] = '\0';

  strncpy(appState.weather.sunset, "--:--", sizeof(appState.weather.sunset) - 1);
  appState.weather.sunset[sizeof(appState.weather.sunset) - 1] = '\0';

  appState.wifiState = WifiState::Idle;
  appState.wifiRssi = 0;
  appState.wifiConfigured = false;
  appState.wifiLastAttemptMs = 0;
  strncpy(appState.wifiSsid, wifiSsidRuntime, sizeof(appState.wifiSsid) - 1);
  appState.wifiSsid[sizeof(appState.wifiSsid) - 1] = '\0';
  strncpy(appState.wifiPass, wifiPassRuntime, sizeof(appState.wifiPass) - 1);
  appState.wifiPass[sizeof(appState.wifiPass) - 1] = '\0';

  // Touch calibration
  TouchInput_XPT2046::Cal cal;
  cal.screenW = 480;
  cal.screenH = 320;
  cal.rotation = 1;
  cal.xMin = 278;
  cal.xMax = 3775;
  cal.yMin = 383;
  cal.yMax = 3696;

  loadTouchCal(cal);  // override with NVS values if present
  touch.setCalibration(cal);
  touch.begin();

  touch.setTapMaxMs(250);
  touch.setLongPressMs(700);
  touch.setSwipeMinPx(60);
  touch.setSwipeMaxMs(450);
  touch.setDebounceMs(3);

#if TOUCH_USE_IRQ
  int touchIrq = digitalPinToInterrupt(TOUCH_IRQ);
  if (touchIrq != NOT_AN_INTERRUPT) {
    attachInterrupt(touchIrq, onTouchInterrupt, FALLING);
  }
#endif

  settingsPage.add(&wifiSsidItem);
  settingsPage.add(&wifiPassItem);
  settingsPage.add(&reconnectWifiItem);
  settingsPage.add(&aboutItem);
  settingsPage.add(&calibrateTouchItem);
  settingsPage.add(&webHostItem);
  homePage.setStatusProvider(populateStatusBar);
  settingsPage.setStatusProvider(populateStatusBar);
  aboutPage.setStatusProvider(populateStatusBar);
  weatherInfoPage.setStatusProvider(populateStatusBar);
  deviceEnrollPage.setStatusProvider(populateStatusBar);
  DeviceAuth::begin();

  // Load persisted HVAC runtimes and stat accumulators from NVS.
  {
    Preferences prefs;
    if (prefs.begin("hvac", true)) {
      tempController.setHeatRunMs((unsigned long)prefs.getUInt("heatRunSec", 0) * 1000UL);
      tempController.setCoolRunMs((unsigned long)prefs.getUInt("coolRunSec", 0) * 1000UL);
      tempController.setHeatSetptStats(
        (unsigned long)prefs.getUInt("htSetptSec", 0) * 1000UL,
        prefs.getUInt("htSetptCnt", 0));
      tempController.setCoolSetptStats(
        (unsigned long)prefs.getUInt("ctSetptSec", 0) * 1000UL,
        prefs.getUInt("ctSetptCnt", 0));
      tempController.setHeatDegStats(
        (unsigned long)prefs.getUInt("htDegSec", 0) * 1000UL,
        prefs.getUInt("htDegCnt", 0));
      tempController.setCoolDegStats(
        (unsigned long)prefs.getUInt("ctDegSec", 0) * 1000UL,
        prefs.getUInt("ctDegCnt", 0));
      prefs.end();
    }
  }

  // Load persisted web host mode.
  {
    Preferences prefs;
    if (prefs.begin("web", true)) {
      webExternalMode = prefs.getBool("external", false);
      prefs.end();
    }
  }

  // Load persisted temperature schedule.
  loadSchedule();

  // System page: dynamic body showing firmware info + runtime stats.
  aboutPage.setBodyProvider([](char* buf, size_t len) {
    unsigned long heatSec = tempController.getHeatRunMs() / 1000UL;
    unsigned long coolSec = tempController.getCoolRunMs() / 1000UL;

    // Avg time to reach setpoint — format as minutes (or "--" if no data).
    auto fmtMin = [](unsigned long ms, char* out, size_t n) {
      if (ms == 0) { snprintf(out, n, "--"); return; }
      unsigned long sec = ms / 1000UL;
      if (sec < 60) snprintf(out, n, "%lus", sec);
      else          snprintf(out, n, "%lum", sec / 60UL);
    };
    char setptH[10], setptC[10], degH[10], degC[10];
    fmtMin(tempController.getAvgHeatSetptMs(), setptH, sizeof(setptH));
    fmtMin(tempController.getAvgCoolSetptMs(), setptC, sizeof(setptC));
    fmtMin(tempController.getAvgHeatMsPerDeg(), degH,  sizeof(degH));
    fmtMin(tempController.getAvgCoolMsPerDeg(), degC,  sizeof(degC));

    // Include IP address so it can be displayed on the System page.
    String ip = WiFi.localIP().toString();
    snprintf(buf, len,
      "BUILD=" __DATE__ " B%lu %s\n"
      "HEAT=%luh %02lum\n"
      "COOL=%luh %02lum\n"
      "IP=%s\n"
      "SETPTH=%s\n"
      "SETPTC=%s\n"
      "DEGH=%s\n"
      "DEGC=%s\n"
      "FAULT=%s\n"
      "COOLDELAY=%lu",
      (unsigned long)bootCount,
      resetReasonToString(bootResetReason),
      heatSec / 3600UL, (heatSec % 3600UL) / 60UL,
      coolSec / 3600UL, (coolSec % 3600UL) / 60UL,
      ip.c_str(),
      setptH, setptC, degH, degC,
      appState.faultActive ? appState.faultMsg : "",
      tempController.getCoolDelayRemaining());
  });

  ui.begin(&homePage, millis());

  if (weatherTaskHandle == nullptr) {
    BaseType_t weatherTaskOk = xTaskCreate(
      weatherFetchTask,
      "weatherFetch",
      8192,
      nullptr,
      tskIDLE_PRIORITY + 1,
      &weatherTaskHandle);
    if (weatherTaskOk != pdPASS) {
      weatherTaskHandle = nullptr;
      Logger.Error("Weather task create failed; weather updates disabled");
    }
  }

#if FIREBASE_ENABLED
  if (firebaseTaskHandle == nullptr) {
    BaseType_t firebaseTaskOk = xTaskCreate(
      firebaseSyncTask,
      "firebaseSync",
      8192,
      nullptr,
      tskIDLE_PRIORITY + 1,
      &firebaseTaskHandle);
    if (firebaseTaskOk != pdPASS) {
      firebaseTaskHandle = nullptr;
      Logger.Error("Firebase task create failed; cloud sync disabled");
    }
  }
#endif

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  feedTaskWatchdog();
  startWiFiConnect();
  homePage.markWifiDirty();

  if (showResetBannerOnBoot) {
    char resetBanner[48];
    snprintf(resetBanner, sizeof(resetBanner), "Recovered from reset: %s", resetReasonToString(bootResetReason));
    showWifiBanner(resetBanner, 4000, true);
  }
}

static void initTaskWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t cfg = {
    .timeout_ms = TASK_WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
}

static inline void feedTaskWatchdog() {
  esp_task_wdt_reset();
}

// --------------------------------------------------
// Touch calibration
// --------------------------------------------------
// Calibration math for rotation=1 (landscape):
//   sx = screenW-1 - (rawY-yMin)*(screenH-1)/(yMax-yMin)
//   sy = (rawX-xMin)*(screenW-1)/(xMax-xMin)
// So rawX drives the screen-Y axis, rawY drives the screen-X axis (inverted).

// Bump this when the calibration formula changes so stale NVS data is ignored.
static constexpr uint8_t TOUCH_CAL_VERSION = 2;

static void saveTouchCal(const TouchInput_XPT2046::Cal& c) {
  Preferences prefs;
  if (prefs.begin("touch", false)) {
    prefs.putUChar("ver",  TOUCH_CAL_VERSION);
    prefs.putShort("xmin", c.xMin);
    prefs.putShort("xmax", c.xMax);
    prefs.putShort("ymin", c.yMin);
    prefs.putShort("ymax", c.yMax);
    prefs.end();
    Logger.Info("Touch cal saved");
  }
}

static bool loadTouchCal(TouchInput_XPT2046::Cal& c) {
  Preferences prefs;
  if (!prefs.begin("touch", true)) return false;
  if (!prefs.isKey("xmin")) { prefs.end(); return false; }
  // Ignore calibrations saved with an older (buggy) formula
  if (prefs.getUChar("ver", 0) < TOUCH_CAL_VERSION) {
    prefs.end();
    Logger.Info("Touch cal outdated, using defaults");
    return false;
  }
  c.xMin = prefs.getShort("xmin", c.xMin);
  c.xMax = prefs.getShort("xmax", c.xMax);
  c.yMin = prefs.getShort("ymin", c.yMin);
  c.yMax = prefs.getShort("ymax", c.yMax);
  prefs.end();
  Logger.Info("Touch cal loaded from NVS");
  return true;
}

static void drawCalTarget(int16_t x, int16_t y, uint16_t color) {
  tft.drawFastHLine(x - 15, y, 31, color);
  tft.drawFastVLine(x, y - 15, 31, color);
  tft.drawCircle(x, y, 8, color);
}

static void waitRawTap(int16_t& rx, int16_t& ry) {
  // Drain any current press first
  while (touch.isPressed()) {
    feedTaskWatchdog();
    delay(10);
  }
  feedTaskWatchdog();
  delay(150);
  while (true) {
    feedTaskWatchdog();
    if (touch.readRawPoint(rx, ry)) {
      while (touch.isPressed()) {
        feedTaskWatchdog();
        delay(5);
      }
      return;
    }
    delay(10);
  }
}

// Blocking full-screen calibration: show two targets, record raw taps,
// compute and save new xMin/xMax/yMin/yMax.
static void runTouchCalibration() {
  const int16_t SW = (int16_t)uiCtx.width;   // 480
  const int16_t SH = (int16_t)uiCtx.height;  // 320

  // Target positions — keep well inside the display edges
  const int16_t CAL_X1 = 40,  CAL_Y1 = 40;
  const int16_t CAL_X2 = 440, CAL_Y2 = 280;

  // --- Target 1 ---
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(2);
  tft.setCursor(100, 140);
  tft.print("Touch Calibration");
  tft.setTextSize(1);
  tft.setCursor(130, 165);
  tft.print("Tap the crosshair (1/2)");
  drawCalTarget(CAL_X1, CAL_Y1, 0xFFFF);

  int16_t rx1, ry1;
  waitRawTap(rx1, ry1);

  // --- Target 2 ---
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(1);
  tft.setCursor(130, 165);
  tft.print("Tap the crosshair (2/2)");
  drawCalTarget(CAL_X2, CAL_Y2, 0xFFFF);

  int16_t rx2, ry2;
  waitRawTap(rx2, ry2);

  // --- Solve for cal parameters (rotation=1, corrected dimension-aware formula) ---
  // sy = (rawX-xMin)*(SH-1)/(xMax-xMin)  [rawX drives screen Y, scale by SH-1]
  //   => xRange = (rx2-rx1)*(SH-1)/(CAL_Y2-CAL_Y1)
  int32_t xRange = (int32_t)(rx2 - rx1) * (SH - 1) / (CAL_Y2 - CAL_Y1);
  int16_t xMin = (int16_t)(rx1 - (int32_t)CAL_Y1 * xRange / (SH - 1));
  int16_t xMax = (int16_t)(xMin + xRange);

  // sx = (SW-1) - (rawY-yMin)*(SW-1)/(yMax-yMin)  [rawY drives screen X, scale by SW-1]
  //   => yRange = (ry1-ry2)*(SW-1)/(CAL_X2-CAL_X1)
  int32_t yRange = (int32_t)(ry1 - ry2) * (SW - 1) / (CAL_X2 - CAL_X1);
  int16_t yMin = (int16_t)(ry1 - (int32_t)(SW - 1 - CAL_X1) * yRange / (SW - 1));
  int16_t yMax = (int16_t)(yMin + yRange);

  // Sanity: ensure min < max
  if (xMin > xMax) { int16_t t = xMin; xMin = xMax; xMax = t; }
  if (yMin > yMax) { int16_t t = yMin; yMin = yMax; yMax = t; }

  TouchInput_XPT2046::Cal newCal;
  newCal.screenW = SW;
  newCal.screenH = SH;
  newCal.rotation = 1;
  newCal.xMin = xMin;
  newCal.xMax = xMax;
  newCal.yMin = yMin;
  newCal.yMax = yMax;

  touch.setCalibration(newCal);
  saveTouchCal(newCal);

  // --- Confirmation screen ---
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(2);
  tft.setCursor(110, 120);
  tft.print("Calibration saved!");
  tft.setTextSize(1);
  tft.setCursor(60, 155);
  tft.printf("xMin=%d xMax=%d", (int)xMin, (int)xMax);
  tft.setCursor(60, 170);
  tft.printf("yMin=%d yMax=%d", (int)yMin, (int)yMax);
  delay(2500);

  uiCtx.invalidateAll();
}

void calibrateTouch(UIContext& ctx) {
  (void)ctx;
  runTouchCalibration();
}

void openSettingsMenu(UIContext& ctx) {
  (void)ctx;
  ui.push(&settingsPage, millis(), UITransitionType::None);
  waitForTouchRelease();
}

void onSettingsBack(UIContext& ctx) {
  (void)ctx;
  ui.pop(millis(), UITransitionType::None);
  waitForTouchRelease();
}

void openAbout(UIContext& ctx) {
  (void)ctx;
  ui.push(&aboutPage, millis(), UITransitionType::None);
  waitForTouchRelease();
}

void openWifiSsidEdit(UIContext& ctx) {
  (void)ctx;
  strncpy(wifiSsidEdit, wifiSsidRuntime, sizeof(wifiSsidEdit) - 1);
  wifiSsidEdit[sizeof(wifiSsidEdit) - 1] = '\0';
  ui.push(&wifiSsidPage, millis(), UITransitionType::None);
  waitForTouchRelease();
}

void openWifiPassEdit(UIContext& ctx) {
  (void)ctx;
  strncpy(wifiPassEdit, wifiPassRuntime, sizeof(wifiPassEdit) - 1);
  wifiPassEdit[sizeof(wifiPassEdit) - 1] = '\0';
  ui.push(&wifiPassPage, millis(), UITransitionType::None);
  waitForTouchRelease();
}

void reconnectWifiNow(UIContext& ctx) {
  (void)ctx;
  applyWifiCredentialsAndReconnect();
}

static void showReconnectFeedback() {
  showWifiBanner("Reconnecting WiFi...", 1200, false);
}

static void applyWifiCredentialsAndReconnect() {
  if (ui.current() == &wifiSsidPage) {
    strncpy(wifiSsidRuntime, wifiSsidEdit, sizeof(wifiSsidRuntime) - 1);
    wifiSsidRuntime[sizeof(wifiSsidRuntime) - 1] = '\0';
  }

  if (ui.current() == &wifiPassPage) {
    strncpy(wifiPassRuntime, wifiPassEdit, sizeof(wifiPassRuntime) - 1);
    wifiPassRuntime[sizeof(wifiPassRuntime) - 1] = '\0';
  }

  strncpy(appState.wifiSsid, wifiSsidRuntime, sizeof(appState.wifiSsid) - 1);
  appState.wifiSsid[sizeof(appState.wifiSsid) - 1] = '\0';
  strncpy(appState.wifiPass, wifiPassRuntime, sizeof(appState.wifiPass) - 1);
  appState.wifiPass[sizeof(appState.wifiPass) - 1] = '\0';

  Logger.Info("WiFi credentials updated from Settings; reconnecting.");
  wifiFailCount = 0;
  wifiGaveUp    = false;
  wifiGaveUpAtMs = 0;
  WiFi.disconnect(true, true);
  appState.wifiLastAttemptMs = 0;
  startWiFiConnect();
  homePage.markWifiDirty();
}

// --------------------------------------------------
// WiFi async
// --------------------------------------------------
static void startWiFiConnect() {
  if (!ssid || !password || strlen(ssid) == 0) {
    appState.wifiState = WifiState::Failed;
    appState.wifiConfigured = false;
    showWifiBanner("WiFi reconnect failed", 2800, true);
    homePage.markWifiDirty();
    if (ui.current() != &homePage) {
      uiCtx.invalidateRect(0, 0, uiCtx.width, uiCtx.theme.statusH);
    }
    return;
  }

  Logger.Verbose("WiFi: starting async connect");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  appState.wifiConfigured = true;
  appState.wifiState = WifiState::Connecting;
  appState.wifiLastAttemptMs = millis();

  wifiAttemptInProgress = true;
  wifiAttemptStartMs = millis();
  homePage.markWifiDirty();
  if (ui.current() != &homePage) {
    uiCtx.invalidateRect(0, 0, uiCtx.width, uiCtx.theme.statusH);
  }
}

static void updateWiFiAsync() {
  wl_status_t s = WiFi.status();
  uint32_t now = millis();

  if (s == WL_CONNECTED) {
    if (appState.wifiState != WifiState::Connected) {
      Logger.Verbose("WiFi: connected");
      Logger.Verbose(WiFi.localIP().toString());
      appState.wifiState = WifiState::Connected;
      homePage.markWifiDirty();
      // Sync NTP so the schedule can use real time.
      configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST - GMT_OFFSET_SECS, NTP_SERVERS);
      // Start embedded web server so users on LAN can access the UI
      initEmbeddedWebServer();
      if (ui.current() != &homePage) {
        uiCtx.invalidateRect(0, 0, uiCtx.width, uiCtx.theme.statusH);
      }
    }

    appState.wifiRssi = WiFi.RSSI();
    wifiAttemptInProgress = false;
    wifiFailCount = 0;
    wifiGaveUp = false;
    wifiGaveUpAtMs = 0;
    return;
  }

  if (wifiAttemptInProgress) {
    if ((now - wifiAttemptStartMs) > wifiConnectTimeoutMs) {
      Logger.Verbose("WiFi: connect timeout");
      WiFi.disconnect();
      wifiAttemptInProgress = false;
      appState.wifiState = WifiState::Failed;
      wifiFailCount++;
      if (wifiFailCount >= 3) {
        wifiGaveUp = true;
        wifiGaveUpAtMs = now;
        showWifiBanner("WiFi failed (3x) - retrying in 1 hour", 5000, true);
        Logger.Info("WiFi: gave up after 3 failures; will auto-retry in 1 hour");
      } else {
        showWifiBanner("WiFi reconnect failed", 2800, true);
      }
      appState.wifiLastAttemptMs = now;
      homePage.markWifiDirty();
      if (ui.current() != &homePage) {
        uiCtx.invalidateRect(0, 0, uiCtx.width, uiCtx.theme.statusH);
      }
    } else {
      if (appState.wifiState != WifiState::Connecting) {
        appState.wifiState = WifiState::Connecting;
        homePage.markWifiDirty();
        if (ui.current() != &homePage) {
          uiCtx.invalidateRect(0, 0, uiCtx.width, uiCtx.theme.statusH);
        }
      }
    }
    return;
  }

  if ((now - appState.wifiLastAttemptMs) > wifiRetryDelayMs) {
    if (wifiGaveUp) {
      if ((now - wifiGaveUpAtMs) < wifiGiveUpRetryMs) return;
      Logger.Info("WiFi: 1-hour cooldown elapsed; retrying");
      wifiGaveUp = false;
      wifiFailCount = 0;
      wifiGaveUpAtMs = 0;
    }
    Logger.Verbose("WiFi: retrying");
    startWiFiConnect();
  }
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------
void loop() {
  uint32_t now = millis();

  // Always run WiFi state machine so UI state transitions (e.g. Connecting -> Connected)
  // are reflected even after WL_CONNECTED becomes true.
  updateWiFiAsync();
  processWeatherRefreshResult(now);

  if (WiFi.status() == WL_CONNECTED && now >= next_weather_update_time_ms) {
    // Skip weather fetch requests while touch input is active.
    if (touch.hasPendingTouch()) {
      next_weather_update_time_ms = now + 500UL;
    } else if (requestWeatherRefreshAsync()) {
      // Keep a retry deadline while an async request is in flight.
      next_weather_update_time_ms = now + WEATHER_RETRY_FREQUENCY_MS;
    } else {
      next_weather_update_time_ms = now + WEATHER_BUSY_RECHECK_MS;
    }
  }

  feedTaskWatchdog();
  updateBacklightManager();
  static uint32_t nextTempPollMs = 0;
  static bool hasDisplayedRoomTemp = false;
  static int lastDisplayedRoomTemp = 0;
  if (now >= nextTempPollMs) {
    float sampledRoomTempF = tempSensor.getTemperatureF();
    int displayedRoomTemp = (int)lroundf(sampledRoomTempF);

    // Always update the controller so fault/relay timers run every poll cycle.
    // Only update the displayed state when the whole-degree value changes.
    appState.roomTempF = sampledRoomTempF;
    tempController.updateCurrentTemperature(displayedRoomTemp);
    if (!hasDisplayedRoomTemp || displayedRoomTemp != lastDisplayedRoomTemp) {
      hasDisplayedRoomTemp = true;
      lastDisplayedRoomTemp = displayedRoomTemp;
    }
    nextTempPollMs = now + TEMP_SENSOR_POLL_MS;
  }

  // Publish confirmed relay state for home-page run indication.
  // Only shows as running when the feedback pin confirms the relay physically closed.
  appState.heatRunning = tempController.isHeatRelayConfirmed();
  appState.coolRunning = tempController.isCoolRelayConfirmed();

  // Propagate fault state to appState so the home page can display it.
  if (tempController.hasFault() != appState.faultActive) {
    appState.faultActive = tempController.hasFault();
    strncpy(appState.faultMsg, tempController.getFaultMsg(), sizeof(appState.faultMsg) - 1);
    appState.faultMsg[sizeof(appState.faultMsg) - 1] = '\0';
    if (ui.current() == &homePage) homePage.markFaultDirty();
  }

  // Periodically persist runtime accumulators and stats to NVS (every 5 min).
  static uint32_t nextRuntimeSaveMs = 0;
  if (now >= nextRuntimeSaveMs) {
    Preferences prefs;
    if (prefs.begin("hvac", false)) {
      prefs.putUInt("heatRunSec", (uint32_t)(tempController.getHeatRunMs() / 1000UL));
      prefs.putUInt("coolRunSec", (uint32_t)(tempController.getCoolRunMs() / 1000UL));
      prefs.putUInt("htSetptSec", (uint32_t)(tempController.getHeatSetptTotalMs() / 1000UL));
      prefs.putUInt("htSetptCnt", tempController.getHeatSetptCount());
      prefs.putUInt("ctSetptSec", (uint32_t)(tempController.getCoolSetptTotalMs() / 1000UL));
      prefs.putUInt("ctSetptCnt", tempController.getCoolSetptCount());
      prefs.putUInt("htDegSec",   (uint32_t)(tempController.getHeatDegTotalMs() / 1000UL));
      prefs.putUInt("htDegCnt",   tempController.getHeatDegTotal());
      prefs.putUInt("ctDegSec",   (uint32_t)(tempController.getCoolDegTotalMs() / 1000UL));
      prefs.putUInt("ctDegCnt",   tempController.getCoolDegTotal());
      prefs.end();
    }
    nextRuntimeSaveMs = now + 5UL * 60UL * 1000UL;
  }

  // Check schedule and apply setpoint at period boundaries (once per minute).
  static uint32_t nextSchedTickMs = 0;
  if (now >= nextSchedTickMs) {
    tickSchedule();
    nextSchedTickMs = now + 60UL * 1000UL;
  }

  // Throttle UI tick to avoid excessive redraws (reduces flicker).
  static const uint32_t UI_TICK_MS = 250; // 20Hz UI refresh
  static uint32_t nextUiTickMs = 0;
  if (now >= nextUiTickMs) {
    ui.tick(now, UIEvent::None);
    nextUiTickMs = now + UI_TICK_MS;
  }

  // Update only the countdown strip on the System page each second.
  // Calling drawCountdown() directly avoids a full page re-render.
  {
    static uint32_t nextCdRefreshMs = 0;
    static bool lastCdActive = false;
    if (ui.current() == &aboutPage) {
      unsigned long cdMs = tempController.getCoolDelayRemaining();
      bool cdActive = cdMs > 0;
      if (cdActive) {
        // Throttled 1-second update while delay is running.
        if (now >= nextCdRefreshMs) {
          aboutPage.drawCountdown(uiCtx, cdMs);
          nextCdRefreshMs = now + 1000;
        }
      } else if (lastCdActive) {
        // Timer just reached zero — clear immediately without waiting for the next interval.
        aboutPage.drawCountdown(uiCtx, 0);
      }
      lastCdActive = cdActive;
    }
  }

  static bool wasDown = false;
  static uint32_t downMs = 0;
  static int16_t downRawX = 0;
  static int16_t downRawY = 0;
  static int16_t downX = 0;
  static int16_t downY = 0;
  static bool touchHandledOnPress = false;
  static int8_t holdResetTarget = 0;   // 0=none, 1=heat, 2=cool (about-page long-press)
  static const uint32_t resetHoldMs = 1500; // hold duration to trigger reset
  static int8_t repeatDirection = 0; // +1 for plus, -1 for minus
  static uint32_t nextRepeatMs = 0;
  static const uint32_t repeatStartDelayMs = 450;
  static const uint32_t repeatIntervalMs = 160;

  // Match ThermostatHomePage::modeButton geometry and keep it excluded from
  // broad plus/minus zones so mode taps don't mutate setpoint.
  auto inModeZone = [&](int16_t tx, int16_t ty) -> bool {
    return (ui.current() == &homePage) && pointInRect(tx, ty, 334, 48, 132, 170);
  };

  auto inPlusZone = [&](int16_t tx, int16_t ty) -> bool {
    return (ui.current() == &homePage)
      && pointInRect(tx, ty, 14, 56, 84, 72)
      && !inModeZone(tx, ty);
  };

  auto inMinusZone = [&](int16_t tx, int16_t ty) -> bool {
    return (ui.current() == &homePage)
      && pointInRect(tx, ty, 14, 134, 84, 72)
      && !inModeZone(tx, ty);
  };

  auto handleImmediateSetpointTap = [&](int16_t tx, int16_t ty) -> bool {
    if (ui.current() != &homePage) {
      return false;
    }

    if (inPlusZone(tx, ty)) {
      Logger.Verbose("plus");
      appState.setpointF += 1.0f;
      tempController.updateTemperatureSettingFromAzure((int)appState.setpointF);
      homePage.markSetpointDirty();
      return true;
    }

    if (inMinusZone(tx, ty)) {
      Logger.Verbose("minus");
      appState.setpointF -= 1.0f;
      tempController.updateTemperatureSettingFromAzure((int)appState.setpointF);
      homePage.markSetpointDirty();
      return true;
    }

    return false;
  };

  auto handleTap = [&](int16_t tx, int16_t ty) -> bool {
    // ---------------- HOME PAGE ----------------
    if (ui.current() == &homePage) {
      // Gear/settings hit target: covers full right edge, taller than status bar for easy tapping.
      const int gearX  = uiCtx.width - 110;
      const int gearY  = 0;
      const int gearW  = 110;
      const int gearH  = 30;

      const int minusX = 14;
      const int minusY = 134;
      const int minusW = 84;
      const int minusH = 72;

      const int plusX  = 14;
      const int plusY  = 56;
      const int plusW  = 84;
      const int plusH  = 72;

      // Match ThermostatHomePage::modeButton geometry for reliable hit testing.
      const int modeX  = 334;
      const int modeY  = 48;
      const int modeW  = 132;
      const int modeH  = 170;

      if (pointInRect(tx, ty, gearX, gearY, gearW, gearH)) {
        Logger.Verbose("gear -> settings");
        ui.push(&settingsPage, now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      if (pointInRect(tx, ty, modeX, modeY, modeW, modeH)) {
        if (appState.faultActive) {
          // Reset fault and resume normal operation in the current mode.
          Logger.Verbose("fault reset");
          tempController.clearFault();
          appState.faultActive = false;
          appState.faultMsg[0] = '\0';
          homePage.markFaultDirty();
        } else {
          // Rows: Cool (y=48,h=36), Heat (y=90,h=36), Fan (y=132,h=36), Off (y=174,h=36).
          if (ty < modeY + 39) {
            Logger.Verbose("mode cool");
            applyMode(HvacMode::Cool);
          } else if (ty < modeY + 81) {
            Logger.Verbose("mode heat");
            applyMode(HvacMode::Heat);
          } else if (ty < modeY + 123) {
            Logger.Verbose("mode fan");
            // Fan button cycles Auto -> High -> Low.
            // Auto: fan runs high while AC is active.
            // High/Low: fan runs continuously, even if HVAC mode is Off.
            if (appState.fanSpeed == FanSpeed::Auto) {
              appState.fanSpeed = FanSpeed::High;
            } else if (appState.fanSpeed == FanSpeed::High) {
              appState.fanSpeed = FanSpeed::Low;
            } else {
              appState.fanSpeed = FanSpeed::Auto;
            }
            tempController.setFanSpeed(appState.fanSpeed);
            homePage.markModeDirty();
          } else {
            Logger.Verbose("mode off");
            applyMode(HvacMode::Off);
          }
        }
        return true;
      }

      if (pointInRect(tx, ty, plusX, plusY, plusW, plusH)) return false;
      if (pointInRect(tx, ty, minusX, minusY, minusW, minusH)) return false;

      Logger.Verbose("home tap ignored");
      return false;
    }

    // ---------------- SETTINGS PAGE ----------------
    if (ui.current() == &settingsPage) {
      // Matches MenuPage::drawBackButton
      const int backX = 8;
      const int backY = 26;
      const int backW = 38;
      const int backH = 24;

      if (pointInRect(tx, ty, backX, backY, backW, backH)
          || (ty <= 60 && tx <= 220)) {
        Logger.Verbose("settings back");
        ui.pop(now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      // MenuPage owns its geometry - use its hit-test directly.
      int row = settingsPage.hitTestRow(tx, ty, uiCtx);
#if TOUCH_MAP_LOG
      Serial.print("CAL:settings row="); Serial.print(row);
      Serial.print(" tx="); Serial.print(tx);
      Serial.print(" ty="); Serial.println(ty);
#endif
      if (row == 0) {
        Logger.Verbose("open wifi ssid editor");
        openWifiSsidEdit(uiCtx);
        return true;
      }
      if (row == 1) {
        Logger.Verbose("open wifi password editor");
        openWifiPassEdit(uiCtx);
        return true;
      }
      if (row == 2) {
        Logger.Verbose("reconnect wifi");
        showReconnectFeedback();
        reconnectWifiNow(uiCtx);
        waitForTouchRelease();
        return true;
      }
      if (row == 3) {
        Logger.Verbose("open system");
        ui.push(&aboutPage, now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }
      if (row == 4) {
        Logger.Verbose("calibrate touch");
        calibrateTouch(uiCtx);
        return true;
      }
      if (row == 5) {
        Logger.Verbose("toggle web external");
        webHostItem.activate(uiCtx, settingsPage);
        settingsPage.markRowDirty(uiCtx, 5);
        return true;
      }

      Logger.Verbose("settings tap ignored");
      return false;
    }

    // ---------------- SYSTEM PAGE ----------------
    if (ui.current() == &aboutPage) {
      // Matches InfoPage back button location
      const int backX = 8;
      const int backY = 26;
      const int backW = 38;
      const int backH = 24;

      if (pointInRect(tx, ty, backX, backY, backW, backH)
          || (ty <= 60 && tx <= 220)) {
        Logger.Verbose("system back");
        ui.pop(now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      if (InfoPage::hitInfoButton(uiCtx, tx, ty)) {
        Logger.Verbose("weather info");
        ui.push(&weatherInfoPage, now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      if (InfoPage::hitDevicesButton(uiCtx, tx, ty)) {
        Logger.Verbose("add device");
        deviceEnrollPage.activate();
        ui.push(&deviceEnrollPage, now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      if (InfoPage::hitFaultResetButton(uiCtx, tx, ty, appState.faultActive)) {
        Logger.Verbose("system fault reset");
        tempController.clearFault();
        appState.faultActive = false;
        appState.faultMsg[0] = '\0';
        homePage.markFaultDirty();
        uiCtx.invalidateAll();
        ui.current()->renderFull(uiCtx);
        uiCtx.dirty.clear();
        waitForTouchRelease();
        return true;
      }

      // Reset button hit-test (layout mirrors InfoPage constants).
      // stripeY = statusH(18) + 58 = 76; cardY = 76+34 = 110;
      // cardH = 102; btnY = cardY + cardH + 6 = 218; btnH = 26;
      // cardW = (480 - 42) / 2 = 219; leftX=14; rightX=247
      const int resetBtnY = 218;
      const int resetBtnH = 26;
      const int resetCardW = (uiCtx.width - 42) / 2;
      if (pointInRect(tx, ty, 14, resetBtnY, resetCardW, resetBtnH)) {
        return true; // handled as hold — tracking starts below on press-down
      }
      if (pointInRect(tx, ty, 247, resetBtnY, resetCardW, resetBtnH)) {
        return true; // handled as hold
      }

      Logger.Verbose("about tap ignored");
      return false;
    }

    // ---------------- WEATHER INFO PAGE ----------------
    if (ui.current() == &weatherInfoPage) {
      if (WeatherInfoPage::hitBack(uiCtx, tx, ty)) {
        Logger.Verbose("weather info back");
        ui.pop(now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }
      return false;
    }

    // ---------------- DEVICE ENROLL PAGE ----------------
    if (ui.current() == &deviceEnrollPage) {
      if (DeviceEnrollPage::hitBack(uiCtx, tx, ty)) {
        Logger.Verbose("enroll back");
        ui.pop(now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }
      return false;
    }

    // ---------------- WIFI EDIT PAGES ----------------
    if (ui.current() == &wifiSsidPage || ui.current() == &wifiPassPage) {
      const int backX = 8;
      const int backY = 26;
      const int backW = 38;
      const int backH = 24;

      if (KeyboardPage::hitCloseButton(uiCtx, tx, ty)
          || pointInRect(tx, ty, backX, backY, backW, backH)
          || (ty <= 60 && tx <= 220)) {
        Logger.Verbose("wifi edit back");
        ui.tick(now, UIEvent::Back);
        ui.pop(now, UITransitionType::None);
        waitForTouchRelease();
        return true;
      }

      KeyboardPage* kb = (ui.current() == &wifiSsidPage) ? &wifiSsidPage : &wifiPassPage;
      if (kb->tapAt(uiCtx, tx, ty)) {
        if (kb->done()) {
          if (kb->accepted()) {
            applyWifiCredentialsAndReconnect();
          }
          ui.pop(now, UITransitionType::None);
          waitForTouchRelease();
        }
        return true;
      }

      Logger.Verbose("wifi edit tap ignored");
      return false;
    }

    return false;
  };

  int16_t sxRaw = 0, syRaw = 0;
  bool touching = touch.readScreenPoint(sxRaw, syRaw);
  int16_t sx = sxRaw;
  int16_t sy = syRaw;
  mapTouchToUi(sx, sy);

  // Working correction for this panel
  if (touching) {
    bool wasBacklightOff = !backlightOn;

    // Treat only touch-down as user activity. Updating activity on every sampled
    // touch frame can keep the backlight alive indefinitely if the touch line is noisy.
    if (!wasDown) {
      registerUserActivity();
    }

    if (wasBacklightOff) {
      updateWifiBanner(now);
      feedTaskWatchdog();
      delay(1);
      return;
    }

    if (!wasDown) {
      wasDown = true;
      downMs = now;
      downRawX = sxRaw;
      downRawY = syRaw;
      downX = sx;
      downY = sy;
      touchHandledOnPress = false;
      holdResetTarget = 0;
      repeatDirection = inPlusZone(downX, downY) ? 1 : (inMinusZone(downX, downY) ? -1 : 0);
      nextRepeatMs = now + repeatStartDelayMs;

    #if TOUCH_DEBUG
      Serial.print("touch x=");
      Serial.print(downX);
      Serial.print(" y=");
      Serial.println(downY);
    #endif

      // Detect about-page reset button press-down (long-hold target).
      if (ui.current() == &aboutPage) {
        const int resetBtnY  = 218;
        const int resetBtnH  = 26;
        const int resetCardW = (uiCtx.width - 42) / 2;
        if (pointInRect(downX, downY, 14, resetBtnY, resetCardW, resetBtnH)) {
          holdResetTarget = 1; // heat
          touchHandledOnPress = true;
        } else if (pointInRect(downX, downY, 247, resetBtnY, resetCardW, resetBtnH)) {
          holdResetTarget = 2; // cool
          touchHandledOnPress = true;
        }
      }

      // Detect enroll-page Remove All button press-down.
      if (holdResetTarget == 0 && ui.current() == &deviceEnrollPage) {
        if (DeviceEnrollPage::hitRemoveAll(uiCtx, downX, downY)
            && DeviceAuth::getDeviceCount() > 0) {
          holdResetTarget = 3; // remove all devices
          touchHandledOnPress = true;
        }
      }

      if (!touchHandledOnPress) {
        touchHandledOnPress = handleImmediateSetpointTap(downX, downY);
      }
      if (!touchHandledOnPress) {
        touchHandledOnPress = handleTap(downX, downY);
      }
      // Tick immediately for home page (setpoint feedback) and keyboard pages
      // (key highlight needs to render before finger lifts).
      // Skip if handleTap navigated away — push/pop already called renderFull.
      UIPage* afterTap = ui.current();
      if (afterTap == &homePage || afterTap == &wifiSsidPage || afterTap == &wifiPassPage) {
        ui.tick(now, UIEvent::None);
      }
    } else if (holdResetTarget == 3 && ui.current() == &deviceEnrollPage) {
      // Animate hold progress on the enroll-page Remove All button.
      uint32_t held = now - downMs;
      float progress = (float)held / (float)resetHoldMs;
      if (progress > 1.0f) progress = 1.0f;
      deviceEnrollPage.drawRemoveAllProgress(uiCtx, progress);

      if (held >= resetHoldMs) {
        Logger.Info("Removing all enrolled devices");
        DeviceAuth::clearAllDevices();
        holdResetTarget = 0;
        waitForTouchRelease();
        // Re-render page so device count updates and button disappears.
        uiCtx.invalidateAll();
        deviceEnrollPage.renderFull(uiCtx);
        uiCtx.dirty.clear();
      }
    } else if (holdResetTarget != 0 && ui.current() == &aboutPage) {
      // Animate hold progress on the about-page reset button.
      uint32_t held = now - downMs;
      float progress = (float)held / (float)resetHoldMs;
      if (progress > 1.0f) progress = 1.0f;
      aboutPage.drawResetProgress(uiCtx, holdResetTarget == 1, progress);

      if (held >= resetHoldMs) {
        // Trigger reset
        if (holdResetTarget == 1) {
          Logger.Info("Resetting heat runtime");
          tempController.resetHeatRunMs();
          tempController.resetHeatStats();
        } else {
          Logger.Info("Resetting cool runtime");
          tempController.resetCoolRunMs();
          tempController.resetCoolStats();
        }
        // Persist zeroed values to NVS immediately
        Preferences prefs;
        if (prefs.begin("hvac", false)) {
          prefs.putUInt("heatRunSec", (uint32_t)(tempController.getHeatRunMs() / 1000UL));
          prefs.putUInt("coolRunSec", (uint32_t)(tempController.getCoolRunMs() / 1000UL));
          prefs.putUInt("htSetptSec", (uint32_t)(tempController.getHeatSetptTotalMs() / 1000UL));
          prefs.putUInt("htSetptCnt", tempController.getHeatSetptCount());
          prefs.putUInt("ctSetptSec", (uint32_t)(tempController.getCoolSetptTotalMs() / 1000UL));
          prefs.putUInt("ctSetptCnt", tempController.getCoolSetptCount());
          prefs.putUInt("htDegSec",   (uint32_t)(tempController.getHeatDegTotalMs() / 1000UL));
          prefs.putUInt("htDegCnt",   tempController.getHeatDegTotal());
          prefs.putUInt("ctDegSec",   (uint32_t)(tempController.getCoolDegTotalMs() / 1000UL));
          prefs.putUInt("ctDegCnt",   tempController.getCoolDegTotal());
          prefs.end();
        }
        holdResetTarget = 0;
        waitForTouchRelease();
        // Re-render the about page so the runtime card shows 0 and button resets.
        uiCtx.invalidateAll();
        ui.current()->renderFull(uiCtx);
        uiCtx.dirty.clear();
      }
    } else if (repeatDirection != 0) {
      bool stillInRepeatZone = (repeatDirection > 0) ? inPlusZone(sx, sy) : inMinusZone(sx, sy);

      if (!stillInRepeatZone) {
        repeatDirection = 0;
      } else if (now >= nextRepeatMs) {
        if (repeatDirection > 0) {
          Logger.Verbose("plus (repeat)");
          appState.setpointF += 1.0f;
        } else {
          Logger.Verbose("minus (repeat)");
          appState.setpointF -= 1.0f;
        }

        tempController.updateTemperatureSettingFromAzure((int)appState.setpointF);
        homePage.markSetpointDirty();
        touchHandledOnPress = true;
        nextRepeatMs = now + repeatIntervalMs;
        ui.tick(now, UIEvent::None);
      }
    }

    updateWifiBanner(now);
    feedTaskWatchdog();
    delay(1);
    return;
  }

  if (wasDown) {
    wasDown = false;
    repeatDirection = 0;
    uint32_t held = now - downMs;

    // If the user released before the reset hold completed, redraw button at 0%.
    if (holdResetTarget != 0) {
      if (ui.current() == &aboutPage) {
        aboutPage.drawResetProgress(uiCtx, holdResetTarget == 1, 0.0f);
      }
      holdResetTarget = 0;
    }

#if TOUCH_DEBUG
    Serial.print("release x=");
    Serial.print(downX);
    Serial.print(" y=");
    Serial.print(downY);
    Serial.print(" held=");
    Serial.println(held);
#endif

#if TOUCH_MAP_LOG
    Serial.print("CAL:page=");
    Serial.print(currentPageName());
    Serial.print(" rawX=");
    Serial.print(downRawX);
    Serial.print(" rawY=");
    Serial.print(downRawY);
    Serial.print(" mapX=");
    Serial.print(downX);
    Serial.print(" mapY=");
    Serial.println(downY);
#endif

    bool keyboardActive = (ui.current() == &wifiSsidPage || ui.current() == &wifiPassPage);
    uint32_t tapMaxMs = keyboardActive ? 5000 : 550;
    if (!touchHandledOnPress && held <= tapMaxMs) {
      (void)handleTap(downX, downY);
      // Force an immediate UI tick for touch responses while preserving
      // the periodic UI throttle. This keeps touch responsive but avoids
      // continuous redraws from the periodic timer.
      nextUiTickMs = 0;
      ui.tick(now, UIEvent::None);
      nextUiTickMs = now + UI_TICK_MS;
    }

    touchHandledOnPress = false;
  }

  // Handle embedded webserver client requests if running
  // (placed AFTER touch to ensure requests aren't blocked during touch input)
  if (embeddedWebServerStarted()) {
    feedTaskWatchdog();
    embeddedWebServerHandleClient();
    feedTaskWatchdog();
  }

  updateWifiBanner(now);
  feedTaskWatchdog();
  delay(1);
}

// --------------------------------------------------
// Weather
// --------------------------------------------------
#if FIREBASE_ENABLED
static void firebaseSyncTask(void*) {
  // Delay first sync attempt after boot to let WiFi/NTP/UI settle.
  vTaskDelay(pdMS_TO_TICKS(15000));

  for (;;) {
    if (appState.wifiState == WifiState::Connected) {
      FirebaseSync::tick();
    }

    // Keep this task low-impact; FirebaseSync::tick handles its own internal
    // polling/periodic cadence.
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
#endif

static void weatherFetchTask(void*) {
  for (;;) {
    bool shouldFetch = false;
    portENTER_CRITICAL(&weatherStateMux);
    if (weatherRequestPending && !weatherFetchInProgress) {
      weatherRequestPending = false;
      weatherFetchInProgress = true;
      shouldFetch = true;
    }
    portEXIT_CRITICAL(&weatherStateMux);

    if (!shouldFetch) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    bool ok = currLocWeather.update(latitude, longitude);
    WeatherSnapshot snapshot = {};
    if (ok) {
      snapshot.outsideTempF = currLocWeather.temperature;
      snapshot.dayHighF = currLocWeather.tempMax;
      snapshot.dayLowF = currLocWeather.tempMin;
      snapshot.humidity = currLocWeather.humidity;
      snapshot.windMph = currLocWeather.windSpeed;
      strncpy(snapshot.description, currLocWeather.description, sizeof(snapshot.description) - 1);
      snapshot.description[sizeof(snapshot.description) - 1] = '\0';
      strncpy(snapshot.sunrise, currLocWeather.sunriseTime, sizeof(snapshot.sunrise) - 1);
      snapshot.sunrise[sizeof(snapshot.sunrise) - 1] = '\0';
      strncpy(snapshot.sunset, currLocWeather.sunsetTime, sizeof(snapshot.sunset) - 1);
      snapshot.sunset[sizeof(snapshot.sunset) - 1] = '\0';
    }

    portENTER_CRITICAL(&weatherStateMux);
    weatherLastFetchSuccess = ok;
    if (ok) {
      weatherPending = snapshot;
    }
    weatherResultReady = true;
    weatherFetchInProgress = false;
    portEXIT_CRITICAL(&weatherStateMux);
  }
}

static bool requestWeatherRefreshAsync() {
  bool queued = false;
  portENTER_CRITICAL(&weatherStateMux);
  if (weatherTaskHandle != nullptr && !weatherRequestPending && !weatherFetchInProgress) {
    weatherRequestPending = true;
    queued = true;
  }
  portEXIT_CRITICAL(&weatherStateMux);
  return queued;
}

static void processWeatherRefreshResult(uint32_t now) {
  bool hasResult = false;
  bool ok = false;
  WeatherSnapshot snapshot = {};

  portENTER_CRITICAL(&weatherStateMux);
  if (weatherResultReady) {
    hasResult = true;
    ok = weatherLastFetchSuccess;
    if (ok) {
      snapshot = weatherPending;
    }
    weatherResultReady = false;
  }
  portEXIT_CRITICAL(&weatherStateMux);

  if (!hasResult) return;

  if (!ok) {
    Logger.Error("Weather update failed");
    next_weather_update_time_ms = now + WEATHER_RETRY_FREQUENCY_MS;
    return;
  }

  appState.weather.outsideTempF = snapshot.outsideTempF;
  appState.weather.dayHighF = snapshot.dayHighF;
  appState.weather.dayLowF = snapshot.dayLowF;
  appState.weather.humidity = snapshot.humidity;
  appState.weather.windMph = snapshot.windMph;

  strncpy(appState.weather.description, snapshot.description, sizeof(appState.weather.description) - 1);
  appState.weather.description[sizeof(appState.weather.description) - 1] = '\0';

  strncpy(appState.weather.sunrise, snapshot.sunrise, sizeof(appState.weather.sunrise) - 1);
  appState.weather.sunrise[sizeof(appState.weather.sunrise) - 1] = '\0';

  strncpy(appState.weather.sunset, snapshot.sunset, sizeof(appState.weather.sunset) - 1);
  appState.weather.sunset[sizeof(appState.weather.sunset) - 1] = '\0';

  appState.weather.lastUpdateMs = now;
  homePage.markWeatherDirty();
  Logger.Info("Weather updated from LocationWeather");
  next_weather_update_time_ms = now + WEATHER_UPDATE_FREQUENCY_MS;
}

// startup icon test removed