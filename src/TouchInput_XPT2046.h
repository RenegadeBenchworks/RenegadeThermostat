#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "UIEvent.h"

// XPT2046 touch -> UIEvent mapper
//
// Notes for this project:
// - We use raw screen-point reads for button hitboxes
// - We still support poll() for any future event-driven pages
// - This version applies the left/right mirror fix globally
class TouchInput_XPT2046 {
public:
  struct Cal {
    int16_t xMin = 300;
    int16_t xMax = 3800;
    int16_t yMin = 300;
    int16_t yMax = 3800;

    int16_t screenW = 480;
    int16_t screenH = 320;

    // Rotation used by OUR mapper, not the TFT library
    uint8_t rotation = 2;
  };

  TouchInput_XPT2046(uint8_t touchCS, int8_t touchIRQ = -1)
  : ts(touchCS, touchIRQ), _touchIRQ(touchIRQ) {}

  void begin() {
    ts.begin();

    // Keep library rotation fixed; we do our own mapping below.
    ts.setRotation(0);

    if (_touchIRQ >= 0) {
      pinMode(_touchIRQ, INPUT_PULLUP);
      _irqEnabled = true;
    }
  }

  void setCalibration(const Cal& c) { cal = c; }

  void setTapMaxMs(uint16_t ms)      { tapMaxMs = ms; }
  void setLongPressMs(uint16_t ms)   { longPressMs = ms; }
  void setSwipeMinPx(uint16_t px)    { swipeMinPx = px; }
  void setSwipeMaxMs(uint16_t ms)    { swipeMaxMs = ms; }
  void setDebounceMs(uint16_t ms)    { debounceMs = ms; }

  void IRAM_ATTR notifyInterrupt() {
    _irqPending = true;
  }

  bool hasPendingTouch() const {
    if (_irqEnabled) {
      return _irqPending || digitalRead(_touchIRQ) == LOW;
    }
    return true;
  }

  bool isPressed() {
    return ts.touched();
  }

  UIEvent poll() {
    const uint32_t now = millis();

    if (now - lastPollMs < debounceMs) return UIEvent::None;
    lastPollMs = now;

    if (_irqEnabled && !hasPendingTouch() && !isDown) {
      return UIEvent::None;
    }

    const bool touching = ts.touched();

    if (touching) {
      if (_irqEnabled) _irqPending = false;

      TS_Point p = ts.getPoint();

      int16_t sx = 0, sy = 0;
      if (!mapToScreen(p.x, p.y, sx, sy)) {
        return UIEvent::None;
      }

      if (!isDown) {
        isDown = true;
        downMs = now;
        downX = sx;
        downY = sy;
        lastX = sx;
        lastY = sy;
        swipeEmitted = false;
        longEmitted = false;
      } else {
        lastX = sx;
        lastY = sy;

        if (!swipeEmitted) {
          int dx = (int)lastX - (int)downX;
          int dy = (int)lastY - (int)downY;
          uint32_t dt = now - downMs;

          if (dt <= swipeMaxMs) {
            if (abs(dx) >= (int)swipeMinPx || abs(dy) >= (int)swipeMinPx) {
              swipeEmitted = true;

              if (abs(dx) > abs(dy)) {
                return (dx > 0) ? UIEvent::Right : UIEvent::Left;
              } else {
                return (dy > 0) ? UIEvent::Down : UIEvent::Up;
              }
            }
          }
        }

        if (!longEmitted && (now - downMs) >= longPressMs) {
          longEmitted = true;
          return UIEvent::Back;
        }
      }

      return UIEvent::None;
    }

    if (isDown) {
      isDown = false;

      const uint32_t held = now - downMs;

      if (swipeEmitted || longEmitted) {
        swipeEmitted = false;
        longEmitted = false;
        return UIEvent::None;
      }

      if (held <= tapMaxMs) {
        return UIEvent::Select;
      }

      return UIEvent::None;
    }

    return UIEvent::None;
  }

  bool readScreenPoint(int16_t& sx, int16_t& sy) {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    return mapToScreen(p.x, p.y, sx, sy);
  }

  bool readRawPoint(int16_t& rx, int16_t& ry) {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    rx = p.x;
    ry = p.y;
    return true;
  }

private:
  XPT2046_Touchscreen ts;
  Cal cal;
  int8_t _touchIRQ = -1;

  volatile bool _irqPending = false;
  bool _irqEnabled = false;

  uint16_t tapMaxMs    = 250;
  uint16_t longPressMs = 700;
  uint16_t swipeMinPx  = 60;
  uint16_t swipeMaxMs  = 450;
  uint16_t debounceMs  = 8;

  uint32_t lastPollMs = 0;

  bool isDown = false;
  uint32_t downMs = 0;
  int16_t downX = 0;
  int16_t downY = 0;
  int16_t lastX = 0;
  int16_t lastY = 0;

  bool swipeEmitted = false;
  bool longEmitted = false;

  bool mapToScreen(int16_t rawX, int16_t rawY, int16_t& sx, int16_t& sy) {
    if (cal.xMax <= cal.xMin || cal.yMax <= cal.yMin) return false;

    if (rawX < cal.xMin) rawX = cal.xMin;
    if (rawX > cal.xMax) rawX = cal.xMax;
    if (rawY < cal.yMin) rawY = cal.yMin;
    if (rawY > cal.yMax) rawY = cal.yMax;

    // Odd rotations (1, 3) swap which screen dimension each raw axis maps to.
    // Without this swap, a 480×320 display with rotation=1 can only reach
    // screen-X down to 160 (left third unreachable) and overflows screen-Y.
    bool swapped = (cal.rotation & 1) != 0;
    int32_t scaleX = swapped ? (int32_t)(cal.screenH - 1) : (int32_t)(cal.screenW - 1);
    int32_t scaleY = swapped ? (int32_t)(cal.screenW - 1) : (int32_t)(cal.screenH - 1);
    int32_t nx = (int32_t)(rawX - cal.xMin) * scaleX / (cal.xMax - cal.xMin);
    int32_t ny = (int32_t)(rawY - cal.yMin) * scaleY / (cal.yMax - cal.yMin);

    switch (cal.rotation & 3) {
      case 0:
        sx = (int16_t)nx;
        sy = (int16_t)ny;
        break;

      case 1:
        sx = (int16_t)(cal.screenW - 1 - ny);
        sy = (int16_t)nx;
        break;

      case 2:
        sx = (int16_t)(cal.screenW - 1 - nx);
        sy = (int16_t)(cal.screenH - 1 - ny);
        break;

      case 3:
        sx = (int16_t)ny;
        sy = (int16_t)(cal.screenH - 1 - nx);
        break;
    }

      return true;
  }
};