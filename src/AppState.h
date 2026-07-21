// FILE: src/AppState.h
#pragma once
#include <Arduino.h>

enum class HvacMode : uint8_t {
  Off,
  Heat,
  Cool,
  Fan
};

enum class FanSpeed : uint8_t {
  Auto,
  Low,
  High
};

enum class WifiState : uint8_t {
  Idle,
  Connecting,
  Connected,
  Failed
};

struct WeatherData {
  float outsideTempF = 72.0f;
  float dayHighF = 78.0f;
  float dayLowF = 58.0f;
  int humidity = 45;
  float windMph = 5.0f;
  char description[32] = "Clear sky";

  // NEW
  char sunrise[16] = "6:41 AM";
  char sunset[16]  = "7:13 PM";

  uint32_t lastUpdateMs = 0;
};

// --------------------------------------------------
// Schedule
// --------------------------------------------------
struct SchedulePeriod {
  uint8_t startHour   = 7;
  uint8_t startMinute = 0;
  float   setpointF   = 70.0f;
  SchedulePeriod() = default;
  SchedulePeriod(uint8_t h, uint8_t m, float sp) : startHour(h), startMinute(m), setpointF(sp) {}
};

struct ScheduleDay {
  SchedulePeriod morning = {7,  0, 70.0f};
  SchedulePeriod day     = {9,  0, 72.0f};
  SchedulePeriod night   = {22, 0, 68.0f};
};

struct ThermostatSchedule {
  bool        enabled = false;
  ScheduleDay days[7]; // 0=Mon, 1=Tue, 2=Wed, 3=Thu, 4=Fri, 5=Sat, 6=Sun
};

struct ThermostatState {
  float roomTempF = 70.5f;
  float setpointF = 72.0f;
  bool  adjustingSetpoint = false;

  HvacMode mode = HvacMode::Off;
  FanSpeed fanSpeed = FanSpeed::Auto;
  bool heatRunning = false;
  bool coolRunning = false;

  WeatherData weather;

  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  bool wifiConnected = false;
  WifiState wifiState = WifiState::Idle;
  int wifiRssi = 0;
  bool wifiConfigured = false;
  uint32_t wifiLastAttemptMs = 0;

  // HVAC fault state
  bool faultActive = false;
  char faultMsg[64] = "";

  // Temperature schedule
  ThermostatSchedule schedule;
};

