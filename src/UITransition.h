// FILE: UITransition.h
#pragma once
#include <Arduino.h>
#include "UIContext.h"

// These transitions DO NOT require offscreen buffers.
// They are "curtain/wipe" effects drawn over the screen.
enum class UITransitionType : uint8_t {
  None,
  WipeLeft,   // curtain moves left-to-right
  WipeRight,  // curtain moves right-to-left
};

struct UITransition {
  UITransitionType type = UITransitionType::None;
  uint32_t startMs = 0;
  uint16_t durationMs = 200;
  bool active = false;

  void begin(UITransitionType t, uint32_t now, uint16_t dur) {
    type = t; startMs = now; durationMs = dur; active = (t != UITransitionType::None);
  }

  // returns true if still active
  bool drawOverlay(UIContext& ctx, uint32_t now) {
    if (!active) return false;

    uint32_t elapsed = now - startMs;
    if (elapsed >= durationMs) {
      active = false;
      return false;
    }

    float p = (float)elapsed / (float)durationMs; // 0..1
    int w = ctx.width;

    // Simple easing (smoothstep-ish)
    p = p * p * (3.0f - 2.0f * p);

    int coverW = (int)(w * (1.0f - p));
    if (coverW < 0) coverW = 0;

    // Draw curtain
    switch (type) {
      case UITransitionType::WipeLeft:
        ctx.display.fillRect(0, 0, coverW, ctx.height, ctx.theme.bg);
        break;
      case UITransitionType::WipeRight:
        ctx.display.fillRect(w - coverW, 0, coverW, ctx.height, ctx.theme.bg);
        break;
      default:
        break;
    }

    return true;
  }
};