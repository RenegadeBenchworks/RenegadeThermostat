// FILE: src/UIRect.h
#pragma once
#include <Arduino.h>

struct UIRect {
  int16_t x = 0, y = 0, w = 0, h = 0;
  bool valid = false;

  static UIRect Full(uint16_t W, uint16_t H) {
    UIRect r; r.x=0; r.y=0; r.w=(int16_t)W; r.h=(int16_t)H; r.valid=true; return r;
  }

  void clear() { valid = false; x = y = w = h = 0; }

  void include(const UIRect& o) {
    if (!o.valid) return;
    if (!valid) { *this = o; return; }
    int16_t x1 = min(x, o.x);
    int16_t y1 = min(y, o.y);
    int16_t x2 = max<int16_t>(x + w, o.x + o.w);
    int16_t y2 = max<int16_t>(y + h, o.y + o.h);
    x = x1; y = y1; w = x2 - x1; h = y2 - y1;
    valid = true;
  }
};