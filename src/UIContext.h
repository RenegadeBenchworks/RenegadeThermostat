// FILE: src/UIContext.h
#pragma once
#include <Adafruit_GFX.h>
#include "UITheme.h"
#include "UIRect.h"

struct UIContext {
  Adafruit_GFX& display;
  UITheme theme;

  uint16_t width;
  uint16_t height;

  UIRect dirty;

  explicit UIContext(Adafruit_GFX& d)
  : display(d), width(d.width()), height(d.height()) {}

  void invalidateAll() { dirty = UIRect::Full(width, height); }

  void invalidateRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    UIRect r; r.x=x; r.y=y; r.w=w; r.h=h; r.valid=true;
    dirty.include(r);
  }
};