#include "FirebaseSync.h"

#if FIREBASE_ENABLED

#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "AppState.h"
#include "SerialLogger.h"
#include "TemperatureControlSystem.h"
#include "ThermostatHomePage.h"
#include "UIContext.h"
#include "esp_task_wdt.h"

// ── Symbols provided by main.cpp ──────────────────────────────────────────────
extern ThermostatState        appState;
extern TemperatureController  tempController;
extern ThermostatHomePage     homePage;
extern UIContext              uiCtx;
extern void applyMode(HvacMode mode);
extern void saveSchedule();
// ─────────────────────────────────────────────────────────────────────────────

namespace FirebaseSync {

static bool _scheduleChanged = true; // push schedule on first boot
static uint32_t _nextCommandPollMs = 0;
static uint32_t _nextPeriodicPushMs = 0;
static const uint32_t COMMAND_POLL_INTERVAL_MS = 5000UL;
static const uint32_t FIREBASE_SOCKET_TIMEOUT_MS = 4000UL;

// Build the REST path with the database-secret auth query param.
// buf must be large enough to hold the full path + "?auth=<secret>".
static void buildPath(char* buf, size_t n, const char* path) {
  snprintf(buf, n, "%s?auth=%s", path, FIREBASE_SECRET);
}

// ── Low-level HTTPS helpers ───────────────────────────────────────────────────

static bool fbGet(const char* path, String& out) {
  char fp[256];
  buildPath(fp, sizeof(fp), path);
  WiFiClientSecure ssl;
  ssl.setInsecure(); // skip cert validation — acceptable for LAN/private use
  ssl.setTimeout(FIREBASE_SOCKET_TIMEOUT_MS);
  HttpClient http(ssl, FIREBASE_HOST, 443);
  esp_task_wdt_reset();
  int status = http.get(fp);
  if (status != 0) {
    Logger.Error("FB GET connect error");
    return false;
  }
  status = http.responseStatusCode();
  if (status < 200 || status >= 300) {
    Logger.Error("FB GET HTTP " + String(status));
    return false;
  }
  out = http.responseBody();
  esp_task_wdt_reset();
  return true;
}

static bool fbPatch(const char* path, const String& body) {
  char fp[256];
  buildPath(fp, sizeof(fp), path);
  WiFiClientSecure ssl;
  ssl.setInsecure();
  ssl.setTimeout(FIREBASE_SOCKET_TIMEOUT_MS);
  HttpClient http(ssl, FIREBASE_HOST, 443);
  esp_task_wdt_reset();
  int status = http.patch(fp, "application/json", body.c_str());
  if (status != 0) {
    Logger.Error("FB PATCH connect error");
    return false;
  }
  status = http.responseStatusCode();
  if (status < 200 || status >= 300) {
    Logger.Error("FB PATCH HTTP " + String(status));
    return false;
  }
  http.responseBody(); // drain
  esp_task_wdt_reset();
  return true;
}

static bool fbPut(const char* path, const String& body) {
  char fp[256];
  buildPath(fp, sizeof(fp), path);
  WiFiClientSecure ssl;
  ssl.setInsecure();
  ssl.setTimeout(FIREBASE_SOCKET_TIMEOUT_MS);
  HttpClient http(ssl, FIREBASE_HOST, 443);
  esp_task_wdt_reset();
  int status = http.put(fp, "application/json", body.c_str());
  if (status != 0) {
    Logger.Error("FB PUT connect error");
    return false;
  }
  status = http.responseStatusCode();
  if (status < 200 || status >= 300) {
    Logger.Error("FB PUT HTTP " + String(status));
    return false;
  }
  http.responseBody(); // drain
  esp_task_wdt_reset();
  return true;
}

// ── State push ────────────────────────────────────────────────────────────────

static void pushState() {
  JsonDocument doc;
  doc["roomTempF"] = appState.roomTempF;
  doc["setpointF"] = appState.setpointF;
  switch (appState.mode) {
    case HvacMode::Heat: doc["mode"] = "Heat"; break;
    case HvacMode::Cool: doc["mode"] = "Cool"; break;
    default:             doc["mode"] = "Off";  break;
  }
  doc["fault"]    = appState.faultActive;
  doc["faultMsg"] = appState.faultActive ? appState.faultMsg : "";
  doc["ts"]       = (long)time(nullptr);
  String body;
  serializeJson(doc, body);
  if (fbPatch("/thermostat/state.json", body)) {
    Logger.Info("Firebase: state pushed (" + String(appState.roomTempF, 1) + "F, sp=" + String(appState.setpointF, 0) + "F)");
  }
}

// ── Schedule push ─────────────────────────────────────────────────────────────

static void pushSchedule() {
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
  String body;
  serializeJson(doc, body);
  if (fbPut("/thermostat/schedule.json", body)) {
    _scheduleChanged = false;
    Logger.Info("Firebase: schedule pushed");
  }
}

// ── Command application ───────────────────────────────────────────────────────

static void applyScheduleObj(JsonObject sched) {
  if (sched.isNull()) return;
  appState.schedule.enabled = sched["enabled"] | false;
  JsonArray days = sched["days"].as<JsonArray>();
  if (days.isNull() || days.size() != 7) return;
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
  saveSchedule();        // persist to NVS
  _scheduleChanged = true; // push back to /thermostat/schedule on next tick
  Logger.Info("Firebase: schedule applied");
}

static bool pollAndApplyCommands() {
  String body;
  if (!fbGet("/thermostat/commands.json", body)) return false;

  // Firebase returns the string "null" when the node is empty / never written.
  if (body == "null" || body.length() == 0) return false;

  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    Logger.Error("Firebase: commands JSON parse error");
    return false;
  }

  bool anyApplied = false;

  // ── setpoint ──────────────────────────────────────────────────────────────
  if (!doc["setpoint"].isNull()) {
    float sp = doc["setpoint"].as<float>();
    appState.setpointF = sp;
    tempController.updateTemperatureSettingFromAzure((int)sp);
    homePage.markSetpointDirty();
    anyApplied = true;
    Logger.Info("Firebase: setpoint → " + String(sp));
  }

  // ── mode ──────────────────────────────────────────────────────────────────
  if (!doc["mode"].isNull()) {
    String modeStr = doc["mode"].as<String>();
    HvacMode newMode = HvacMode::Off;
    if (modeStr.equalsIgnoreCase("Heat"))      newMode = HvacMode::Heat;
    else if (modeStr.equalsIgnoreCase("Cool")) newMode = HvacMode::Cool;
    applyMode(newMode);
    anyApplied = true;
    Logger.Info("Firebase: mode → " + modeStr);
  }

  // ── fault reset ───────────────────────────────────────────────────────────
  if (doc["resetFault"].as<bool>()) {
    tempController.clearFault();
    appState.faultActive  = false;
    appState.faultMsg[0]  = '\0';
    homePage.markFaultDirty();
    anyApplied = true;
    Logger.Info("Firebase: fault reset");
  }

  // ── schedule ──────────────────────────────────────────────────────────────
  if (!doc["schedule"].isNull()) {
    applyScheduleObj(doc["schedule"].as<JsonObject>());
    anyApplied = true;
  }

  // Clear the commands node so they aren't applied again.
  if (anyApplied) {
    static const char clearBody[] =
      "{\"setpoint\":null,\"mode\":null,\"resetFault\":false,\"schedule\":null}";
    fbPut("/thermostat/commands.json", clearBody);
  }

  return anyApplied;
}

// ── Meters push ──────────────────────────────────────────────────────────────

static void pushMeters() {
  JsonDocument doc;
  doc["heatRunSec"]      = (unsigned long)(tempController.getHeatRunMs() / 1000UL);
  doc["coolRunSec"]      = (unsigned long)(tempController.getCoolRunMs() / 1000UL);
  doc["avgHeatSetptSec"] = (unsigned long)(tempController.getAvgHeatSetptMs() / 1000UL);
  doc["avgCoolSetptSec"] = (unsigned long)(tempController.getAvgCoolSetptMs() / 1000UL);
  doc["heatCycles"]      = tempController.getHeatSetptCount();
  doc["coolCycles"]      = tempController.getCoolSetptCount();
  String body;
  serializeJson(doc, body);
  if (fbPatch("/thermostat/meters.json", body)) {
    Logger.Info("Firebase: meters pushed");
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

void tick() {
  const uint32_t now = millis();
  esp_task_wdt_reset();

  // Poll commands frequently for responsive cloud control.
  bool applied = false;
  if ((int32_t)(now - _nextCommandPollMs) >= 0) {
    _nextCommandPollMs = now + COMMAND_POLL_INTERVAL_MS;
    applied = pollAndApplyCommands();
    esp_task_wdt_reset();
    if (applied) {
      // Push updated state right away so remote UI reflects the applied command.
      pushState();
      esp_task_wdt_reset();
    }
  }

  // Keep heavier state/meter pushes on the configured periodic cadence.
  if (_nextPeriodicPushMs == 0 || (int32_t)(now - _nextPeriodicPushMs) >= 0) {
    _nextPeriodicPushMs = now + FIREBASE_SYNC_INTERVAL_MS;
    pushState();
    esp_task_wdt_reset();
    pushMeters();
    esp_task_wdt_reset();
    if (_scheduleChanged) {
      pushSchedule();
      esp_task_wdt_reset();
    }
  }
}

void notifyScheduleChanged() {
  _scheduleChanged = true;
}

} // namespace FirebaseSync

#endif // FIREBASE_ENABLED
