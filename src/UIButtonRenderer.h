#pragma once
#include "UIButton.h"

class UIButtonRenderer {
public:
  static void draw(UIContext& ctx, const UIButton& b,
                   uint16_t borderColor,
                   uint16_t textColor,
                   uint16_t fillColor,
                   uint8_t textSize = 2,
                   uint8_t radius = 6) {
    if (!b.visible) return;

    auto& d = ctx.display;

    if (b.style == UIButtonStyle::Filled) {
      d.fillRoundRect(b.x, b.y, b.w, b.h, radius, fillColor);
    }

    if (b.style != UIButtonStyle::ImageOnly) {
      d.drawRoundRect(b.x, b.y, b.w, b.h, radius, borderColor);
    }

    if (b.image565 && b.imageW > 0 && b.imageH > 0) {
      int ix = b.x + (b.w - b.imageW) / 2;
      int iy = b.y + (b.h - b.imageH) / 2;
      d.drawRGBBitmap(ix, iy, b.image565, b.imageW, b.imageH);
    }

    if (b.label) {
      d.setTextSize(textSize);
      d.setTextColor(textColor);

      int16_t x1, y1;
      uint16_t tw, th;
      d.getTextBounds((char*)b.label, 0, 0, &x1, &y1, &tw, &th);

      int tx = b.x + (b.w - (int)tw) / 2;
      int ty = b.y + (b.h - (int)th) / 2 + 2;

      d.setCursor(tx, ty);
      d.print(b.label);
    }
  }
};