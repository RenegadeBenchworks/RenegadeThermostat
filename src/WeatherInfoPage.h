#pragma once
#include "UIPage.h"
#include "UIStatusBar.h"
#include "WeatherIconRenderer.h"

// Full-screen guide explaining each weather icon shown on the home page.
class WeatherInfoPage : public UIPage {
public:
  const char* title() const override { return "Weather Guide"; }

  void setStatusProvider(void (*fn)(UIStatusState&)) { _statusProvider = fn; }

  bool handle(UIContext& ctx, UIEvent ev) override {
    if (ev == UIEvent::Back || ev == UIEvent::Select) {
      ctx.invalidateRect(0, 0, ctx.width, ctx.theme.statusH);
      return true;
    }
    return false;
  }

  void renderFull(UIContext& ctx) override {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillScreen(th.bg);
    th.applyFont(d);

    if (_statusProvider) _statusProvider(_status);
    UIStatusBar::draw(ctx, _status);

    // Back button (matches InfoPage geometry so hit-test is identical)
    const int bx = th.pad;
    const int by = th.statusH + th.pad;
    d.drawRect(bx, by, 38, 24, th.accent);
    d.setTextSize(2);
    d.setTextColor(th.accent);
    d.setCursor(bx + 10, by + 5);
    d.print("<");

    // Title
    d.setTextSize(th.textSize);
    d.setTextColor(th.accent);
    d.setCursor(th.pad + 52, th.statusH + th.pad + 2);
    d.print("Weather Guide");

    drawEntries(ctx);
    d.setTextWrap(false);
  }

  // Hit-test for the back button — mirrors the back button in InfoPage.
  static bool hitBack(const UIContext& ctx, int16_t x, int16_t y) {
    (void)ctx;
    const int bx = 8;
    const int by = ctx.theme.statusH + ctx.theme.pad;
    return (x >= bx && x < bx + 38 && y >= by && y < by + 24)
        || (y <= 60 && x <= 220);
  }

private:
  void (*_statusProvider)(UIStatusState&) = nullptr;
  UIStatusState _status{};

  struct Entry {
    const uint16_t* bitmap;
    const char*     name;
    const char*     line1;
    const char*     line2;
  };

  static const Entry* entries() {
    static const Entry e[] = {
      { melt_face_45x45,
        "Melty Face",
        "The steering wheel is now lava.",
        nullptr },
      { sun_hot_45x45,
        "Sunny Day",
        "Campground weather level:",
        "elite" },
      { hoodie_45x45,
        "Hoodie",
        "Classic 'bring a hoodie just in case'",
        "weather." },
      { snowflake_45x45,
        "Snowflake",
        "Your propane bill just got nervous.",
        nullptr },
      { freezy_face_45x45,
        "Freezy Face",
        "Nope.",
        nullptr },
    };
    return e;
  }

  static constexpr int kNumEntries = 5;

  static void drawEntries(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    // Content area starts below the back-button/title bar.
    const int contentY = ctx.theme.statusH + 50;
    const int rowH     = (ctx.height - contentY) / kNumEntries;
    const int iconX    = 14;
    const int textX    = iconX + WeatherIconRenderer::FACE_W + 10;

    const Entry* e = entries();

    for (int i = 0; i < kNumEntries; ++i) {
      const int rowY = contentY + i * rowH;

      // Subtle separator between rows
      if (i > 0) {
        d.drawFastHLine(iconX, rowY, ctx.width - iconX * 2, th.muted);
      }

      // Icon — centred vertically in the row
      const int iconY = rowY + (rowH - WeatherIconRenderer::FACE_H) / 2;
      WeatherIconRenderer::drawRGBBitmapProgmem(
          ctx, iconX, iconY,
          e[i].bitmap,
          WeatherIconRenderer::FACE_W,
          WeatherIconRenderer::FACE_H,
          false);

      // Name (accent colour)
      d.setTextSize(1);
      d.setTextWrap(false);
      d.setTextColor(th.accent);
      d.setCursor(textX, rowY + 6);
      d.print(e[i].name);

      // Description lines (foreground colour)
      d.setTextColor(th.fg);
      if (e[i].line1) {
        d.setCursor(textX, rowY + 19);
        d.print(e[i].line1);
      }
      if (e[i].line2) {
        d.setCursor(textX, rowY + 31);
        d.print(e[i].line2);
      }
    }

    d.setTextWrap(false);
  }
};
