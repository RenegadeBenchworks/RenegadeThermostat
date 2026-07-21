#pragma once
#include <Arduino.h>
#include <ctype.h>
#include <math.h>
#include "UIPage.h"
#include "UIStatusBar.h"
#include "AppState.h"
#include "UIButton.h"
#include "WeatherIconRenderer.h"

class ThermostatHomePage : public UIPage {
public:
  using OnOpenSettingsFn = void(*)(UIContext&);

  ThermostatHomePage(ThermostatState& s, OnOpenSettingsFn openSettings)
  : st(s), onOpenSettings(openSettings) {}

  const char* title() const override { return "Thermostat"; }

  void setStatusProvider(void (*fn)(UIStatusState&)) { statusProvider = fn; }

  void markSetpointDirty() { _setpointDirty = true; }
  void markModeDirty()     { _modeDirty = true; }
  void markWeatherDirty()  { _weatherDirty = true; }
  void markWifiDirty()     { _wifiDirty = true; }
  void markFaultDirty()    { _faultDirty = true; }

  bool handle(UIContext& ctx, UIEvent ev) override {
    (void)ctx;
    switch (ev) {
      case UIEvent::Up:
        st.setpointF += 1.0f;
        _setpointDirty = true;
        return true;
      case UIEvent::Down:
        st.setpointF -= 1.0f;
        _setpointDirty = true;
        return true;
      default:
        return false;
    }
  }

  UIButton gearButton(const UIContext& ctx) const {
    const auto& th = ctx.theme;
    UIButton b;
    // Settings touch target: full right edge, taller than status bar for easy tapping.
    b.id = "gear";
    b.x = ctx.width - 110;
    b.y = 0;
    b.w = 110;
    b.h = 30;
    b.style = UIButtonStyle::IconOnly;
    b.visible = true;
    b.enabled = true;
    return b;
  }

  UIButton plusButton(const UIContext&) const {
    UIButton b;
    // Plus touch target (left top card).
    b.id = "plus";
    b.x = 14;
    b.y = 56;
    b.w = 84;
    b.h = 72;
    b.style = UIButtonStyle::IconOnly;
    b.visible = true;
    b.enabled = true;
    return b;
  }

  UIButton minusButton(const UIContext&) const {
    UIButton b;
    // Minus touch target (left bottom card).
    b.id = "minus";
    b.x = 14;
    b.y = 134;
    b.w = 84;
    b.h = 72;
    b.style = UIButtonStyle::IconOnly;
    b.visible = true;
    b.enabled = true;
    return b;
  }

  UIButton modeButton(const UIContext&) const {
    UIButton b;
    // Mode selector touch target (right column).
    b.id = "mode";
    b.x = 334;
    b.y = 56;
    b.w = 132;
    b.h = 162;
    b.style = UIButtonStyle::Outline;
    b.visible = true;
    b.enabled = true;
    return b;
  }

  void renderFull(UIContext& ctx) override {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillScreen(th.bg);
    th.applyFont(d);
    d.setTextWrap(false);

    if (statusProvider) statusProvider(status);
    UIStatusBar::draw(ctx, status);
    // "Settings" is provided via populateStatusBar rightText — no separate draw needed.

    // Main card/frame geometry.
    d.drawRoundRect(14, 56, 84, 72, 8, th.muted);
    d.drawRoundRect(15, 57, 82, 70, 8, 0x5AEB);
    d.drawRoundRect(14, 134, 84, 72, 8, th.muted);
    d.drawRoundRect(15, 135, 82, 70, 8, 0x5AEB);
    d.drawRoundRect(106, 56, 220, 152, 8, th.muted);
    d.drawRoundRect(107, 57, 218, 150, 8, 0x5AEB);
    d.drawRoundRect(334,  48, 132, 36, 6, th.muted);  // Cool
    d.drawRoundRect(334,  90, 132, 36, 6, th.muted);  // Heat
    d.drawRoundRect(334, 132, 132, 36, 6, th.muted);  // Fan
    d.drawRoundRect(334, 174, 132, 36, 6, th.muted);  // Off

    // Bottom weather/sun card geometry.
    d.fillRoundRect(10, 224, 460, 88, 8, th.bg);
    d.drawRoundRect(10, 224, 460, 88, 8, th.muted);
    d.drawFastVLine(250, 232, 72, th.muted);

    drawStepperIcons(d, th);

    syncCachesFromState();
    redrawCurrentTemp(ctx);
    redrawSetpoint(ctx);
    redrawModeChip(ctx);
    redrawRunStatus(ctx);
    redrawWeatherRow(ctx);
    redrawSunRow(ctx);
    redrawFaultBanner(ctx);

    _initialized = true;
    _lastAnimMs = millis();
  }

  void update(UIContext& ctx, uint32_t nowMs) override {
    if (!_initialized) {
      syncCachesFromState();
      _initialized = true;
      _lastAnimMs = nowMs;
      return;
    }

    float dt = (nowMs - _lastAnimMs) / 1000.0f;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.10f) dt = 0.10f;
    _lastAnimMs = nowMs;

    float targetTemp = st.roomTempF;
    float diff = targetTemp - _displayRoomTemp;
    if (fabs(diff) > 0.01f) {
      float step = 3.0f * dt;
      _displayRoomTemp += (fabs(diff) <= step) ? diff : (diff > 0.0f ? step : -step);
      redrawCurrentTemp(ctx);
    }

    if (_setpointDirty || fabs(_lastSetpointF - st.setpointF) > 0.001f) {
      _lastSetpointF = st.setpointF;
      redrawSetpoint(ctx);
      _setpointDirty = false;
    }

    if (_modeDirty || _lastMode != st.mode || _lastFanSpeed != st.fanSpeed) {
      const HvacMode prevMode  = _lastMode;
      const FanSpeed prevSpeed = _lastFanSpeed;
      _lastMode     = st.mode;
      _lastFanSpeed = st.fanSpeed;
      // Targeted redraw: only repaint the button(s) whose appearance changed.
      if (prevMode != st.mode) {
        drawOneModeButton(ctx, modeToSlot(prevMode));  // deactivate old
        drawOneModeButton(ctx, modeToSlot(st.mode));   // activate new
      }
      if (prevSpeed != st.fanSpeed) {
        drawOneModeButton(ctx, 2);  // Fan row — speed label changed
      }
      redrawRunStatus(ctx);
      _modeDirty = false;
    }

    if (_lastHeatRunning != st.heatRunning || _lastCoolRunning != st.coolRunning) {
      _lastHeatRunning = st.heatRunning;
      _lastCoolRunning = st.coolRunning;
    }

    if (_weatherDirty ||
        fabs(_lastOutsideTempF - st.weather.outsideTempF) > 0.001f ||
        fabs(_lastDayHighF - st.weather.dayHighF) > 0.001f ||
        fabs(_lastDayLowF - st.weather.dayLowF) > 0.001f ||
        strcmp(_lastDescription, st.weather.description) != 0) {
      _lastOutsideTempF = st.weather.outsideTempF;
      _lastDayHighF = st.weather.dayHighF;
      _lastDayLowF = st.weather.dayLowF;
      copyString(_lastDescription, st.weather.description, sizeof(_lastDescription));
      redrawWeatherRow(ctx);
      _weatherDirty = false;
    }

    if (strcmp(_lastSunrise, st.weather.sunrise) != 0 ||
        strcmp(_lastSunset, st.weather.sunset) != 0) {
      copyString(_lastSunrise, st.weather.sunrise, sizeof(_lastSunrise));
      copyString(_lastSunset, st.weather.sunset, sizeof(_lastSunset));
      redrawSunRow(ctx);
    }

    if (_wifiDirty ||
        _lastWifiState != st.wifiState ||
        (_lastWifiRssi != st.wifiRssi && st.wifiState == WifiState::Connected)) {
      _lastWifiState = st.wifiState;
      _lastWifiRssi  = st.wifiRssi;
      redrawWifiStatus(ctx);
      _wifiDirty = false;
    }

    if (_faultDirty || _lastFaultActive != st.faultActive) {
      redrawFaultBanner(ctx);
      _faultDirty = false;
    }
  }

private:
  ThermostatState& st;
  OnOpenSettingsFn onOpenSettings;
  void (*statusProvider)(UIStatusState&) = nullptr;
  UIStatusState status{};

  bool _initialized = false;
  uint32_t _lastAnimMs = 0;
  bool _setpointDirty = false;
  bool _modeDirty = false;
  bool _weatherDirty = false;
  bool _wifiDirty = false;
  bool _faultDirty = false;

  float _displayRoomTemp = 0.0f;
  float _lastSetpointF = 0.0f;
  HvacMode _lastMode = HvacMode::Off;
  FanSpeed _lastFanSpeed = FanSpeed::Auto;
  float _lastOutsideTempF = 0.0f;
  float _lastDayHighF = 0.0f;
  float _lastDayLowF = 0.0f;
  char _lastDescription[32]{};
  char _lastSunrise[16]{};
  char _lastSunset[16]{};
  WifiState _lastWifiState = WifiState::Idle;
  int _lastWifiRssi = 0;
  bool _lastFaultActive = false;
  bool _lastHeatRunning = false;
  bool _lastCoolRunning = false;

  void syncCachesFromState() {
    _displayRoomTemp = st.roomTempF;
    _lastSetpointF = st.setpointF;
    _lastMode = st.mode;
    _lastFanSpeed = st.fanSpeed;
    _lastOutsideTempF = st.weather.outsideTempF;
    _lastDayHighF = st.weather.dayHighF;
    _lastDayLowF = st.weather.dayLowF;
    copyString(_lastDescription, st.weather.description, sizeof(_lastDescription));
    copyString(_lastSunrise, st.weather.sunrise, sizeof(_lastSunrise));
    copyString(_lastSunset, st.weather.sunset, sizeof(_lastSunset));
    _lastWifiState = st.wifiState;
    _lastWifiRssi = st.wifiRssi;
    _lastFaultActive = st.faultActive;
    _lastHeatRunning = st.heatRunning;
    _lastCoolRunning = st.coolRunning;
  }

  static void copyString(char* dst, const char* src, size_t dstSize) {
    if (dstSize == 0) return;
    strncpy(dst, src ? src : "", dstSize - 1);
    dst[dstSize - 1] = '\0';
  }

  void redrawCurrentTemp(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    // Clear + redraw region for current inside temperature.
    d.fillRect(116, 70, 200, 74, th.bg);

    d.setTextSize(2);
    d.setTextColor(th.fg);
    d.setCursor(168, 70);
    d.print("Inside");

    char curBuf[16];
    snprintf(curBuf, sizeof(curBuf), "%.0f", _displayRoomTemp);
    d.setTextSize(6);
    d.setTextColor(th.fg);

    int16_t x1, y1;
    uint16_t w, h;
    d.getTextBounds(curBuf, 0, 0, &x1, &y1, &w, &h);
    // Center anchor for the large numeric temp.
    int startX = 195 - (int)w / 2;
    const int degRad = 6;
    d.setCursor(startX, 102);
    d.print(curBuf);
    d.drawCircle(startX + (int)w + 4 + degRad, 102 + degRad + 2, degRad, th.fg);

  }

  void redrawSetpoint(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    // Clear + redraw region for setpoint text.
    d.fillRect(126, 172, 180, 20, th.bg);

    char setBuf[16];
    snprintf(setBuf, sizeof(setBuf), "Set %d", (int)lroundf(st.setpointF));
    d.setTextSize(2);
    d.setTextColor(th.muted);

    int16_t x1, y1;
    uint16_t w, h;
    d.getTextBounds(setBuf, 0, 0, &x1, &y1, &w, &h);
    // Center anchor for "Set XX" line.
    int startX = 216 - (int)w / 2;
    const int degRad = 2;
    d.setCursor(startX, 176);
    d.print(setBuf);
    d.drawCircle(startX + (int)w + 3 + degRad, 176 + degRad + 1, degRad, th.muted);
  }

  // Draw a single mode-column button (slot: 0=Cool, 1=Heat, 2=Fan, 3=Off).
  // Clears only the 36 px strip for that button and redraws it from state.
  void drawOneModeButton(UIContext& ctx, int8_t slot) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    static const uint16_t kCool = 0x14BF;
    static const uint16_t kHeat = 0xFB00;
    static const uint16_t kFan  = 0x07FF;
    static const int16_t  kBtnY[4] = { 48, 90, 132, 174 };
    const int16_t y = kBtnY[slot];

    d.fillRect(334, y, 132, 36, th.bg);
    d.drawRoundRect(334, y, 132, 36, 6, th.muted);

    switch (slot) {
      case 0: {  // Cool
        const bool active = (st.mode == HvacMode::Cool);
        if (active) d.fillRoundRect(336, y + 2, 128, 32, 6, kCool);
        d.setTextSize(2);
        d.setTextColor(active ? 0xFFFF : th.fg);
        d.setCursor(372, y + 10); d.print("Cool");
        drawCoolIcon(d, 346, y + 18, active ? 0xFFFF : kCool);
        break;
      }
      case 1: {  // Heat
        const bool active = (st.mode == HvacMode::Heat);
        if (active) d.fillRoundRect(336, y + 2, 128, 32, 6, kHeat);
        d.setTextSize(2);
        d.setTextColor(active ? 0xFFFF : th.fg);
        d.setCursor(372, y + 10); d.print("Heat");
        drawHeatIcon(d, 346, y + 18, active ? 0xFFFF : kHeat);
        break;
      }
      case 2: {  // Fan — always cyan; no active fill
        d.setTextSize(2);
        d.setTextColor(kFan);
        d.setCursor(372, y + 3); d.print("Fan");
        d.setTextSize(1);
        const char* fanLabel = "Auto";
        if (st.fanSpeed == FanSpeed::High) fanLabel = "High";
        else if (st.fanSpeed == FanSpeed::Low) fanLabel = "Low";
        d.setCursor(372, y + 21); d.print(fanLabel);
        drawFanIcon(d, 346, y + 18, kFan);
        break;
      }
      default: {  // Off (slot 3)
        const bool active = (st.mode == HvacMode::Off);
        if (active) d.fillRoundRect(336, y + 2, 128, 32, 6, 0x4208);
        d.setTextSize(2);
        d.setTextColor(active ? 0xFFFF : th.fg);
        d.setCursor(372, y + 10); d.print("Off");
        d.drawCircle(346, y + 18, 5, active ? 0xFFFF : th.muted);
        break;
      }
    }
  }

  static int8_t modeToSlot(HvacMode m) {
    switch (m) {
      case HvacMode::Cool: return 0;
      case HvacMode::Heat: return 1;
      case HvacMode::Fan:  return 2;
      default:             return 3;  // Off
    }
  }

  void redrawModeChip(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    // Clear the full column including the 6 px gaps between buttons, then
    // delegate each button's draw to the shared single-button helper.
    d.fillRect(334, 48, 132, 170, th.bg);
    for (int8_t i = 0; i < 4; ++i) drawOneModeButton(ctx, i);
  }

  void redrawWeatherRow(UIContext& ctx) {
    auto& d = ctx.display;
    // Left bottom card content bounds.
    const auto& th = ctx.theme;
    d.fillRect(16, 230, 228, 76, th.bg);

    // Current weather bitmap anchor (45x45).
    WeatherIconRenderer::drawCurrentWeatherIcon(ctx, 24, 236, st.weather.description, st.weather.outsideTempF);

    char descBuf[20];
    formatWeatherLabel(st.weather.description, descBuf, sizeof(descBuf));

    // Condition text position.
    d.setTextSize(1);
    d.setTextColor(th.fg);
    d.setCursor(84, 233);
    d.print(descBuf);

    // Outside temperature value position.
    d.setTextSize(5);
    d.setCursor(112, 250);
    d.print((int)lroundf(st.weather.outsideTempF));
    d.drawCircle(d.getCursorX() + 5, 260, 4, 0x0000);

    // High/low summary position.
    d.setTextSize(1);
    d.setCursor(58, 290);
    d.print("H:");
    d.print((int)lroundf(st.weather.dayHighF));
    d.print("   L:");
    d.print((int)lroundf(st.weather.dayLowF));
  }

  void redrawSunRow(UIContext& ctx) {
    auto& d = ctx.display;
    // Right bottom card content bounds.
    const auto& th = ctx.theme;
    d.fillRect(278, 230, 172, 78, th.bg);

    // Sunrise/sunset bitmap anchors (moved slightly right to avoid overlapping divider).
    WeatherIconRenderer::drawSunrise(ctx, 260, 232);
    WeatherIconRenderer::drawSunset(ctx, 256, 275);

    // Sunrise label/time positions.
    d.setTextSize(1);
    d.setTextColor(th.fg);
    d.setCursor(342, 232);
    d.print("Sunrise");
    d.setTextSize(2);
    d.setCursor(340, 250);
    d.print(st.weather.sunrise);

    // Sunset label/time positions.
    d.setTextSize(1);
    d.setCursor(342, 272);
    d.print("Sunset");
    d.setTextSize(2);
    d.setCursor(340, 286);
    d.print(st.weather.sunset);
  }

  void redrawWifiStatus(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    if (statusProvider) statusProvider(status);
    UIStatusBar::draw(ctx, status);
    d.fillRect(8, 26, 60, 26, th.bg);
    redrawRunStatus(ctx);
  }

  void redrawRunStatus(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    // HVAC status banner box (x, y, w, h).
    const int w = 178;
    const int h = 24;
    const int x = (ctx.width - w) / 2;
    const int y = 26;
    d.fillRoundRect(x, y, w, h, 6, th.bg);
    d.drawRoundRect(x, y, w, h, 6, th.muted);

    d.setTextSize(1);
    d.setTextWrap(false);
    const char* label = "Off";
    uint16_t color = th.muted;
    if (st.mode == HvacMode::Heat) {
      label = "Heating";
      color = 0xFBE4;
    } else if (st.mode == HvacMode::Cool) {
      label = "Cooling";
      color = 0x5D9B;
    }
    d.setTextColor(color);
    int16_t tx1, ty1;
    uint16_t tw, thh;
    d.getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &thh);
    d.setCursor(x + (w - (int)tw) / 2, y + 8);
    d.print(label);
  }

  void redrawFaultBanner(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    // Fault banner strip position and size.
    d.fillRect(106, 210, 360, 12, th.bg);
    _lastFaultActive = st.faultActive;

    if (!st.faultActive) return;
    constexpr uint16_t kRed = 0xF800;
    d.fillRoundRect(106, 210, 360, 12, 4, kRed);
    d.setTextSize(1);
    d.setTextColor(0xFFFF);
    d.setCursor(112, 212);
    d.print("! FAULT: Reset on mode tap or System page");
  }

  void drawWifiStatus(Adafruit_GFX& d, const UITheme& th) {
    int x = 12;
    int y = 32;
    d.fillRect(x - 2, y - 2, 50, 24, th.bg);

    if (st.wifiState == WifiState::Connected) {
      d.fillRect(x + 0, y + 10, 4, 8, th.fg);
      d.fillRect(x + 8, y + 6, 4, 12, th.fg);
      d.fillRect(x + 16, y + 2, 4, 16, th.fg);
      return;
    }

    if (st.wifiState == WifiState::Connecting) {
      d.drawRect(x + 0, y + 10, 4, 8, th.muted);
      d.drawRect(x + 8, y + 6, 4, 12, th.muted);
      d.drawRect(x + 16, y + 2, 4, 16, th.muted);
      return;
    }

    uint16_t bad = 0xF800;
    d.drawRect(x + 0, y + 10, 4, 8, bad);
    d.drawRect(x + 8, y + 6, 4, 12, bad);
    d.drawRect(x + 16, y + 2, 4, 16, bad);
    d.drawLine(x + 24, y + 0, x + 44, y + 20, bad);
    d.drawLine(x + 44, y + 0, x + 24, y + 20, bad);
  }

  static void drawStepperIcons(Adafruit_GFX& d, const UITheme& th) {
    // Up arrow center anchor.
    int ux = 56, uy = 90;
    d.drawLine(ux - 14, uy + 8, ux, uy - 8, th.fg);
    d.drawLine(ux + 14, uy + 8, ux, uy - 8, th.fg);
    d.drawLine(ux - 13, uy + 8, ux + 1, uy - 8, th.fg);
    d.drawLine(ux + 13, uy + 8, ux - 1, uy - 8, th.fg);

    // Down arrow center anchor.
    int dx = 56, dy = 168;
    d.drawLine(dx - 14, dy - 8, dx, dy + 8, th.fg);
    d.drawLine(dx + 14, dy - 8, dx, dy + 8, th.fg);
    d.drawLine(dx - 13, dy - 8, dx + 1, dy + 8, th.fg);
    d.drawLine(dx + 13, dy - 8, dx - 1, dy + 8, th.fg);
  }

  static void formatWeatherLabel(const char* src, char* dst, size_t dstSize) {
    if (!dst || dstSize == 0) return;
    dst[0] = '\0';
    if (!src) return;

    bool newWord = true;
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di < dstSize - 1; ++si) {
      char c = src[si];
      if (c == ' ') {
        newWord = true;
        dst[di++] = c;
      } else {
        dst[di++] = (char)(newWord ? toupper((unsigned char)c) : tolower((unsigned char)c));
        newWord = false;
      }
      if (di >= 16 && src[si + 1] != '\0') {
        if (di < dstSize - 4) {
          dst[di++] = '.';
          dst[di++] = '.';
          dst[di++] = '.';
        }
        break;
      }
    }
    dst[di] = '\0';
  }

  static void drawSunGlyph(Adafruit_GFX& d, int cx, int cy, uint16_t color, bool sunrise) {
    d.drawCircle(cx, cy, 5, color);
    d.drawFastHLine(cx - 12, cy + 10, 24, color);
    d.drawFastVLine(cx, cy - 11, 4, color);
    d.drawFastVLine(cx - 9, cy - 8, 3, color);
    d.drawFastVLine(cx + 9, cy - 8, 3, color);
    if (sunrise) {
      d.drawLine(cx - 7, cy + 6, cx, cy + 1, color);
      d.drawLine(cx + 7, cy + 6, cx, cy + 1, color);
    } else {
      d.drawLine(cx - 7, cy + 3, cx, cy + 8, color);
      d.drawLine(cx + 7, cy + 3, cx, cy + 8, color);
    }
  }

  static void drawWeatherGlyph(Adafruit_GFX& d, int x, int y, float outsideF) {
    d.drawCircle(x + 10, y - 14, 8, 0xFD20);
    d.drawCircle(x + 18, y - 6, 8, 0x7BEF);
    d.drawCircle(x + 8, y - 4, 10, 0x7BEF);
    d.drawCircle(x + 28, y - 4, 10, 0x7BEF);
    d.drawRoundRect(x + 6, y - 4, 26, 12, 5, 0x7BEF);
    if (outsideF < 60.0f) {
      d.drawLine(x + 14, y + 10, x + 10, y + 16, 0x5D9B);
      d.drawLine(x + 24, y + 10, x + 20, y + 16, 0x5D9B);
    }
  }

  static void drawCoolIcon(Adafruit_GFX& d, int x, int y, uint16_t color) {
    // 8-armed snowflake: 4 straight arms + 4 diagonal arms with center dot
    d.drawFastHLine(x - 6, y, 13, color);
    d.drawFastVLine(x, y - 6, 13, color);
    d.drawLine(x - 4, y - 4, x + 4, y + 4, color);
    d.drawLine(x + 4, y - 4, x - 4, y + 4, color);
    d.fillRect(x - 1, y - 1, 3, 3, color);
  }

  static void drawHeatIcon(Adafruit_GFX& d, int x, int y, uint16_t color) {
    // Flame shape: filled circle base + filled triangle pointing up
    d.fillCircle(x, y + 2, 4, color);
    d.fillTriangle(x - 3, y + 2, x + 3, y + 2, x, y - 6, color);
  }

  static void drawFanIcon(Adafruit_GFX& d, int x, int y, uint16_t color) {
    // Simplified fan/propeller: center hub + 3 blades at 120° intervals
    d.fillCircle(x, y, 2, color);
    d.drawLine(x, y, x,     y - 7, color);   // blade up
    d.drawLine(x, y - 7, x + 3, y - 8, color);
    d.drawLine(x, y, x + 6, y + 4, color);   // blade lower-right
    d.drawLine(x + 6, y + 4, x + 7, y + 7, color);
    d.drawLine(x, y, x - 6, y + 4, color);   // blade lower-left
    d.drawLine(x - 6, y + 4, x - 4, y + 7, color);
  }
};