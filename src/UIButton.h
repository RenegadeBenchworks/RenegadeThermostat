#pragma once
#include <Arduino.h>
#include "UIContext.h"

using UIButtonCallback = void(*)(UIContext&, void* userData);

enum class UIButtonStyle : uint8_t {
  Outline,
  Filled,
  IconOnly,
  ImageOnly
};

struct UIButton {
  const char* id = nullptr;

  int16_t x = 0;
  int16_t y = 0;
  int16_t w = 0;
  int16_t h = 0;

  const char* label = nullptr;

  // Optional image/icon data for future use
  const uint16_t* image565 = nullptr;
  int16_t imageW = 0;
  int16_t imageH = 0;

  UIButtonStyle style = UIButtonStyle::Outline;

  bool visible = true;
  bool enabled = true;

  UIButtonCallback onPress = nullptr;
  void* userData = nullptr;
};

inline bool pointInButton(int16_t px, int16_t py, const UIButton& b) {
  return b.visible && b.enabled &&
         px >= b.x && px < (b.x + b.w) &&
         py >= b.y && py < (b.y + b.h);
}

inline void pressButton(UIContext& ctx, const UIButton& b) {
  if (b.visible && b.enabled && b.onPress) {
    b.onPress(ctx, b.userData);
  }
}