// FILE: UIStatusBar.h
#pragma once
#include "UIContext.h"

struct UIStatusState {
  const char* leftText  = "";
  const char* rightText = "";
  bool showWifi = true;
  bool wifi = false;
  int  rssi = 0; // optional
};

class UIStatusBar {
public:
  static void draw(UIContext& ctx, const UIStatusState& st) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillRect(0, 0, ctx.width, th.statusH, th.muted);

    th.applyFont(d);
    d.setTextSize(1);
    d.setTextWrap(false);
    d.setTextColor(th.bg);

    int textX = th.pad;

    // Keep the WiFi glyph in the primary status slot on the left edge.
    if (st.showWifi) {
      int iconX = th.pad;
      int iconY = 4;
      uint16_t c = st.wifi ? th.accent : th.danger;
      if (st.wifi) {
        d.fillRect(iconX + 0, iconY + 6, 2, 4, c);
        d.fillRect(iconX + 4, iconY + 4, 2, 6, c);
        d.fillRect(iconX + 8, iconY + 2, 2, 8, c);
      } else {
        d.drawRect(iconX + 0, iconY + 6, 2, 4, c);
        d.drawRect(iconX + 4, iconY + 4, 2, 6, c);
        d.drawRect(iconX + 8, iconY + 2, 2, 8, c);
        d.drawLine(iconX + 12, iconY + 2, iconX + 16, iconY + 10, c);
      }
      textX = iconX + 22;
    }

    d.setCursor(textX, 4);
    d.print(st.leftText);

    // Right aligned
    int16_t x1, y1;
    uint16_t w, h;
    d.getTextBounds(st.rightText, 0, 0, &x1, &y1, &w, &h);
    int rx = (int)ctx.width - (int)th.pad - (int)w;
    d.setCursor(rx, 4);
    d.print(st.rightText);

    // restore typical size is page’s job; keep minimal side effects
  }
};