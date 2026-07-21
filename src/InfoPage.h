#pragma once
#include "UIPage.h"
#include "UIStatusBar.h"
#include "WeatherIconRenderer.h"

class InfoPage : public UIPage {
public:
  InfoPage(const char* t, const char* body) : _title(t), _body(body) {}
  const char* title() const override { return _title; }

  // Hit-test for the Devices (Add Device) button.
  static bool hitDevicesButton(const UIContext& ctx, int16_t x, int16_t y) {
    int dx, dy, dw, dh;
    getDevicesButtonRect(ctx, dx, dy, dw, dh);
    return (x >= dx && x < (dx + dw) && y >= dy && y < (dy + dh));
  }

  // Hit-test for the Info button (top right of the System page header).
  static bool hitInfoButton(const UIContext& ctx, int16_t x, int16_t y) {
    int ix, iy, iw, ih;
    getInfoButtonRect(ctx, ix, iy, iw, ih);
    return (x >= ix && x < (ix + iw) && y >= iy && y < (iy + ih));
  }

  // Shared hit-test for the About page fault reset button.
  static bool hitFaultResetButton(const UIContext& ctx, int16_t x, int16_t y, bool faultActive) {
    if (!faultActive) return false;
    int bx, by, bw, bh;
    getFaultResetButtonRect(ctx, bx, by, bw, bh);
    return (x >= bx && x < (bx + bw) && y >= by && y < (by + bh));
  }

  // Redraws only the countdown strip — call directly from the main loop for
  // second-by-second updates without triggering a full page re-render.
  void drawCountdown(UIContext& ctx, unsigned long cdMs) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;
    th.applyFont(d);
    const int cdY = th.statusH + 36;
    d.fillRoundRect(14, cdY, ctx.width - 28, 20, 4, th.bg);
    if (cdMs > 0) {
      constexpr uint16_t kAmber = 0xFC60;
      d.drawRoundRect(14, cdY, ctx.width - 28, 20, 4, kAmber);
      d.setTextSize(1);
      d.setTextColor(kAmber);
      d.setCursor(22, cdY + 6);
      unsigned long cdSec = cdMs / 1000UL;
      unsigned long cdMin = cdSec / 60UL;
      cdSec = cdSec % 60UL;
      char cdBuf[56];
      if (cdMin > 0)
        snprintf(cdBuf, sizeof(cdBuf), "Short-cycle delay: %lum %02lus remaining", cdMin, cdSec);
      else
        snprintf(cdBuf, sizeof(cdBuf), "Short-cycle delay: %lus remaining", cdSec);
      d.print(cdBuf);
    }
  }

  void setStatusProvider(void (*fn)(UIStatusState&)) { statusProvider = fn; }
  // Optional: provide body text dynamically (called on each renderFull).
  void setBodyProvider(void (*fn)(char*, size_t)) { bodyProvider = fn; }

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

    if (statusProvider) statusProvider(status);
    UIStatusBar::draw(ctx, status);

    // Back button
    int bx = th.pad;
    int by = th.statusH + th.pad;
    d.drawRect(bx, by, 38, 24, th.accent);
    d.setTextSize(2);
    d.setTextColor(th.accent);
    d.setCursor(bx + 10, by + 5);
    d.print("<");

    d.setTextSize(th.textSize);
    d.setTextWrap(true);

    d.setTextColor(th.accent);
    d.setCursor(th.pad + 52, th.statusH + th.pad + 2);
    d.print(_title);

    // Info button — top right
    {
      int ix, iy, iw, ih;
      getInfoButtonRect(ctx, ix, iy, iw, ih);
      d.drawRoundRect(ix, iy, iw, ih, 4, th.accent);
      d.setTextSize(1);
      d.setTextColor(th.accent);
      d.setCursor(ix + 8, iy + 8);
      d.print("Info");
    }

    // Devices button — left of Info button
    {
      int dx, dy, dw, dh;
      getDevicesButtonRect(ctx, dx, dy, dw, dh);
      d.drawRoundRect(dx, dy, dw, dh, 4, th.accent);
      d.setTextSize(1);
      d.setTextColor(th.accent);
      d.setCursor(dx + 6, dy + 8);
      d.print("Devices");
    }

    // Body: use provider if set, otherwise fall back to static string.
    char dynBody[512];
    const char* bodyText = _body;
    if (bodyProvider) {
      bodyProvider(dynBody, sizeof(dynBody));
      bodyText = dynBody;
    }

    char build[40] = {};
    char heat[24]  = {};
    char cool[24]  = {};
    char fault[64] = {};
    char setptH[12] = {};
    char setptC[12] = {};
    char degH[12]   = {};
    char degC[12]   = {};
    char ip[24]     = {};

    const bool hasCards =
      extractValue(bodyText, "BUILD=", build, sizeof(build)) &&
      extractValue(bodyText, "HEAT=", heat, sizeof(heat)) &&
      extractValue(bodyText, "COOL=", cool, sizeof(cool));

    if (!hasCards) {
      d.setTextColor(th.fg);
      d.setCursor(th.pad, th.statusH + th.pad + th.lineH + 20);
      d.print(bodyText);
      d.setTextWrap(false);
      return;
    }

    extractValue(bodyText, "FAULT=",  fault,  sizeof(fault));
    extractValue(bodyText, "SETPTH=", setptH, sizeof(setptH));
    extractValue(bodyText, "SETPTC=", setptC, sizeof(setptC));
    extractValue(bodyText, "DEGH=",   degH,   sizeof(degH));
    extractValue(bodyText, "DEGC=",   degC,   sizeof(degC));
    extractValue(bodyText, "IP=",     ip,     sizeof(ip));

    // AC short-cycle delay countdown
    char coolDelayBuf[16] = {};
    extractValue(bodyText, "COOLDELAY=", coolDelayBuf, sizeof(coolDelayBuf));
    unsigned long cdMs = (coolDelayBuf[0] != '\0') ? (unsigned long)strtoul(coolDelayBuf, nullptr, 10) : 0UL;

    // Default display for missing data
    if (setptH[0] == '\0') snprintf(setptH, sizeof(setptH), "--");
    if (setptC[0] == '\0') snprintf(setptC, sizeof(setptC), "--");
    if (degH[0]   == '\0') snprintf(degH,   sizeof(degH),   "--");
    if (degC[0]   == '\0') snprintf(degC,   sizeof(degC),   "--");

    // Build stripe
    const int stripeY = th.statusH + 58;

    // Short-cycle delay countdown strip — visible only during the 3-min AC restart delay.
    // cdY sits in the 26px gap between the back-button row (bottom=statusH+34) and the
    // build stripe (top=statusH+58).
    {
      const int cdY = th.statusH + 36;
      d.fillRoundRect(14, cdY, ctx.width - 28, 20, 4, th.bg); // always clear the area
      if (cdMs > 0) {
        constexpr uint16_t kAmber = 0xFC60;
        d.drawRoundRect(14, cdY, ctx.width - 28, 20, 4, kAmber);
        d.setTextSize(1);
        d.setTextColor(kAmber);
        d.setCursor(22, cdY + 6);
        unsigned long cdSec = cdMs / 1000UL;
        unsigned long cdMin = cdSec / 60UL;
        cdSec = cdSec % 60UL;
        char cdBuf[56];
        if (cdMin > 0)
          snprintf(cdBuf, sizeof(cdBuf), "Short-cycle delay: %lum %02lus remaining", cdMin, cdSec);
        else
          snprintf(cdBuf, sizeof(cdBuf), "Short-cycle delay: %lus remaining", cdSec);
        d.print(cdBuf);
      }
    }

    d.fillRoundRect(14, stripeY, ctx.width - 28, 24, 6, th.bg);
    d.drawRoundRect(14, stripeY, ctx.width - 28, 24, 6, th.muted);
    d.setTextSize(1);
    d.setTextColor(th.muted);
    d.setCursor(24, stripeY + 8);
    d.print("Build");
    d.setTextColor(th.fg);
    d.setCursor(78, stripeY + 8);
    d.print(build);

    // IP address to the right of the build date.
    if (ip[0] != '\0') {
      d.setTextColor(th.muted);
      d.setCursor(198, stripeY + 8);
      d.print("IP");
      d.setTextColor(th.fg);
      d.setCursor(216, stripeY + 8);
      d.print(ip);
    }

    // Fault reset button in the build stripe (shown only when a fault is active).
    const bool faultActive = (fault[0] != '\0');
    if (faultActive) {
      int fx, fy, fw, fh;
      getFaultResetButtonRect(ctx, fx, fy, fw, fh);
      drawFaultResetButton(ctx, fx, fy, fw, fh);
    }

    // Runtime cards
    const int cardY = stripeY + 34;
    const int cardH = 102;
    const int cardW = (ctx.width - 14 * 3) / 2;
    const int leftX = 14;
    const int rightX = leftX + cardW + 14;

    drawRuntimeCard(ctx, leftX, cardY, cardW, cardH, "Heat Runtime", heat, 0xFBE4);
    drawRuntimeCard(ctx, rightX, cardY, cardW, cardH, "AC Runtime", cool, 0x5D9B);

    // Reset buttons (below cards)
    const int btnY = cardY + cardH + 6;
    drawResetButton(ctx, leftX,  btnY, cardW, kBtnH, 0xFBE4, 0.0f);
    drawResetButton(ctx, rightX, btnY, cardW, kBtnH, 0x5D9B, 0.0f);

    // Stat rows (below reset buttons)
    const int statsY = btnY + kBtnH + 8;
    drawStatRow(ctx, statsY,      "Avg to setpoint",  setptH, setptC, 0xFBE4, 0x5D9B);
    drawStatRow(ctx, statsY + 22, "Avg \xf8""F change",   degH,   degC,   0xFBE4, 0x5D9B);

    // Optional fault pill
    if (fault[0] != '\0') {
      int fy = statsY + 44 + 4;
      d.fillRoundRect(14, fy, ctx.width - 28, 24, 8, th.danger);
      d.setTextSize(1);
      d.setTextColor(0xFFFF);
      d.setCursor(22, fy + 8);
      d.print("Fault: ");
      d.print(fault);
    }

    d.setTextWrap(false);
  }

  // Called by main.cpp while the user holds a reset button (progress 0.0–1.0).
  // isHeat=true → heat card; isHeat=false → cool card.
  void drawResetProgress(UIContext& ctx, bool isHeat, float progress) {
    const int stripeY = ctx.theme.statusH + 58;
    const int cardY   = stripeY + 34;
    const int cardW   = (ctx.width - 14 * 3) / 2;
    const int btnY    = cardY + kCardH + 6;
    const int x       = isHeat ? 14 : (14 + cardW + 14);
    uint16_t accent   = isHeat ? 0xFBE4 : 0x5D9B;
    drawResetButton(ctx, x, btnY, cardW, kBtnH, accent, progress);
  }

private:
  const char* _title;
  const char* _body;

  void (*statusProvider)(UIStatusState&) = nullptr;
  void (*bodyProvider)(char*, size_t) = nullptr;
  UIStatusState status{};

  // Shared layout constants (used by renderFull and drawResetProgress).
  static constexpr int kCardH = 102;
  static constexpr int kBtnH  = 26;

  static void getDevicesButtonRect(const UIContext& ctx, int& x, int& y, int& w, int& h) {
    // Left of the Info button with a 6px gap.
    int ix, iy, iw, ih;
    getInfoButtonRect(ctx, ix, iy, iw, ih);
    w = 60;
    h = ih;
    x = ix - 6 - w;
    y = iy;
  }

  static void getInfoButtonRect(const UIContext& ctx, int& x, int& y, int& w, int& h) {
    w = 44;
    h = 24;
    x = ctx.width - ctx.theme.pad - w;
    y = ctx.theme.statusH + ctx.theme.pad;
  }

  static void getFaultResetButtonRect(const UIContext& ctx, int& x, int& y, int& w, int& h) {
    const int stripeY = ctx.theme.statusH + 58;
    w = 124;
    h = 18;
    x = ctx.width - 14 - w - 6;
    y = stripeY + 3;
  }

  static bool extractValue(const char* body, const char* key, char* out, size_t outSize) {
    if (!body || !key || !out || outSize == 0) return false;
    const char* p = strstr(body, key);
    if (!p) return false;
    p += strlen(key);
    size_t n = 0;
    while (p[n] != '\0' && p[n] != '\n' && n < outSize - 1) {
      out[n] = p[n];
      ++n;
    }
    out[n] = '\0';
    return n > 0;
  }

  static void drawRuntimeCard(UIContext& ctx,
                              int x,
                              int y,
                              int w,
                              int h,
                              const char* label,
                              const char* value,
                              uint16_t accent) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillRoundRect(x, y, w, h, 10, th.bg);
    d.drawRoundRect(x, y, w, h, 10, th.muted);
    d.fillRect(x + 2, y + 2, w - 4, 6, accent);

    d.setTextSize(1);
    d.setTextColor(th.muted);
    d.setCursor(x + 10, y + 18);
    d.print(label);

    // Small mode glyph in top-right corner of each card.
    const bool isHeat = strstr(label, "Heat") != nullptr;
    if (isHeat) {
      drawHeatGlyph(d, x + w - 26, y + 17, accent);
    } else {
      drawCoolGlyph(d, x + w - 26, y + 17, accent);
    }

    d.setTextSize(3);
    d.setTextColor(th.fg);
    d.setCursor(x + 10, y + 42);
    d.print(value);
  }

  static void drawHeatGlyph(Adafruit_GFX& d, int x, int y, uint16_t color) {
    // Simple flame-like glyph.
    d.drawCircle(x, y, 5, color);
    d.drawFastVLine(x, y - 6, 12, color);
    d.drawLine(x - 4, y + 2, x, y - 7, color);
    d.drawLine(x + 4, y + 2, x, y - 7, color);
  }

  static void drawCoolGlyph(Adafruit_GFX& d, int x, int y, uint16_t color) {
    // Snowflake-like glyph.
    d.drawFastHLine(x - 6, y, 13, color);
    d.drawFastVLine(x, y - 6, 13, color);
    d.drawLine(x - 5, y - 5, x + 5, y + 5, color);
    d.drawLine(x + 5, y - 5, x - 5, y + 5, color);
  }

  // Draws a "Hold to Reset" button with an optional progress fill (0.0–1.0).
  static void drawResetButton(UIContext& ctx,
                              int x, int y, int w, int h,
                              uint16_t accent, float progress) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    progress = constrain(progress, 0.0f, 1.0f);
    int fillW = (int)(progress * (w - 2));

    // Background fill
    d.fillRoundRect(x, y, w, h, 5, th.bg);

    // Progress bar fill (left to right, accent color)
    if (fillW > 0) {
      d.fillRoundRect(x + 1, y + 1, fillW, h - 2, 5, accent);
    }

    // Border
    d.drawRoundRect(x, y, w, h, 5, accent);

    // Label — invert colour once fill covers more than half
    uint16_t textColor = (progress >= 0.5f) ? th.bg : accent;
    d.setTextSize(1);
    d.setTextColor(textColor);
    d.setTextWrap(false);

    // Centre the label
    const char* label = "Hold to Reset";
    int16_t tx1, ty1;
    uint16_t tw, tht;
    d.getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &tht);
    d.setCursor(x + (w - (int)tw) / 2, y + (h - 8) / 2);
    d.print(label);
  }

  // Draws a two-value stat row: [label]  H: <hVal>   C: <cVal>
  static void drawStatRow(UIContext& ctx, int y,
                          const char* label,
                          const char* hVal, const char* cVal,
                          uint16_t hColor, uint16_t cColor) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.setTextSize(1);
    d.setTextWrap(false);

    // Label
    d.setTextColor(th.muted);
    d.setCursor(14, y);
    d.print(label);
    d.print(":");

    // Heat value
    const int hx = 200;
    d.setTextColor(hColor);
    d.setCursor(hx, y);
    d.print("H:");
    d.setTextColor(th.fg);
    d.setCursor(hx + 14, y);
    d.print(hVal);

    // Cool value
    const int cx = 300;
    d.setTextColor(cColor);
    d.setCursor(cx, y);
    d.print("C:");
    d.setTextColor(th.fg);
    d.setCursor(cx + 14, y);
    d.print(cVal);
  }

  static void drawFaultResetButton(UIContext& ctx, int x, int y, int w, int h) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillRoundRect(x, y, w, h, 5, th.danger);
    d.drawRoundRect(x, y, w, h, 5, 0xFFFF);
    d.setTextSize(1);
    d.setTextColor(0xFFFF);
    d.setTextWrap(false);
    d.setCursor(x + 12, y + 6);
    d.print("Reset Fault");
  }
};