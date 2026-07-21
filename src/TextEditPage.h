// FILE: src/TextEditPage.h
#pragma once
#include <Arduino.h>
#include "UIPage.h"

// A simple character editor that works with UIEvent only:
// - Left/Right: move cursor
// - Up/Down: change character (from charset)
// - Tap (Select): accept & save (done)
// - Long press (Back): cancel
//
// Great for SSID/PW entry with touch-only + swipe.
class TextEditPage : public UIPage {
public:
  using OnDoneFn = void(*)(UIContext&);

  enum class Control : uint8_t {
    None,
    MoveLeft,
    MoveRight,
    CharUp,
    CharDown,
    Save,
    Cancel,
    Clear
  };

  // Shared control geometry used by both rendering and main touch hit-testing.
  static constexpr int kCtrlTopY = 208;
  static constexpr int kRowH = 46;
  static constexpr int kRowGap = 8;
  static constexpr int kOuterPad = 8;
  static constexpr int kInnerGap = 8;

  TextEditPage(const char* title,
               char* buffer,
               size_t capacity,
               bool passwordMode = false,
               OnDoneFn onDone = nullptr)
  : _title(title), _buf(buffer), _cap(capacity),
    _password(passwordMode), _onDone(onDone) {}

  const char* title() const override { return _title; }

  void onEnter(UIContext& ctx) override {
    (void)ctx;
    if (!_buf || _cap < 2) return;
    _len = strnlen(_buf, _cap - 1);
    if (_len == 0) {
      _buf[0] = ' ';
      _buf[1] = '\0';
      _len = 1;
    }
    if (_cursor >= _len) _cursor = _len - 1;
    _done = false;
    ctx.invalidateAll();
  }

  bool done() const { return _done; }

  bool handle(UIContext& ctx, UIEvent ev) override {
    if (!_buf || _cap < 2) return false;

    switch (ev) {
      case UIEvent::Left:
        if (_cursor > 0) { _cursor--; markLineDirty(ctx); }
        return true;

      case UIEvent::Right:
        if (_cursor + 1 < _len) { _cursor++; markLineDirty(ctx); }
        return true;

      case UIEvent::Up:
        changeChar(+1);
        markLineDirty(ctx);
        return true;

      case UIEvent::Down:
        changeChar(-1);
        markLineDirty(ctx);
        return true;

      case UIEvent::Select:
        // Trim trailing spaces
        trimRight();
        _done = true;
        if (_onDone) _onDone(ctx);
        return true;

      case UIEvent::Back:
        // cancel: do nothing, caller will pop
        _done = true;
        return true;

      default:
        return false;
    }
  }

  void renderFull(UIContext& ctx) override {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillScreen(th.bg);
    th.applyFont(d);

    d.setTextSize(th.textSize);
    d.setTextWrap(false);

    d.setTextColor(th.accent);
    d.setCursor(th.pad, th.pad);
    d.print(_title);

    // Instructions
    d.setTextSize(1);
    d.setTextColor(th.muted);
    d.setCursor(th.pad, th.pad + th.lineH + 6);
    d.print("Tap buttons: L/R move, +/- edit, Save apply, Cancel exit");

    // Input box
    drawInputLine(ctx);
    drawTouchControls(ctx);
  }

  void renderDirty(UIContext& ctx, const UIRect& dirty) override {
    // Small app: just redraw the input line if dirty intersects it, else full.
    // (Safe default)
    ctx.theme.applyFont(ctx.display);
    (void)dirty;
    drawInputLine(ctx);
  }

private:
  const char* _title;
  char* _buf = nullptr;
  size_t _cap = 0;
  bool _password = false;
  OnDoneFn _onDone = nullptr;

  size_t _len = 0;
  size_t _cursor = 0;
  bool _done = false;

  // Character set: space + printable
  static const char* charset() {
    // Keep it simple: space, A-Z, a-z, 0-9, common symbols.
    return " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_@.!#$%&*+/?";
  }

  void changeChar(int dir) {
    char c = _buf[_cursor];
    const char* cs = charset();
    const int csLen = (int)strlen(cs);

    int idx = 0;
    for (; idx < csLen; idx++) if (cs[idx] == c) break;
    if (idx >= csLen) idx = 0;

    idx += dir;
    if (idx < 0) idx = csLen - 1;
    if (idx >= csLen) idx = 0;

    _buf[_cursor] = cs[idx];

    // Auto-grow if cursor at end and we choose a non-space and capacity allows:
    // Provide a way to extend string by moving right when at last char.
    if (_cursor == _len - 1 && _len + 1 < _cap) {
      // If user makes last char non-space, allow extending by adding a space after it
      if (_buf[_cursor] != ' ') {
        _buf[_len] = ' ';
        _buf[_len + 1] = '\0';
        _len++;
      }
    }
  }

  void trimRight() {
    if (!_buf) return;
    size_t L = strnlen(_buf, _cap - 1);
    while (L > 0 && _buf[L - 1] == ' ') {
      _buf[L - 1] = '\0';
      L--;
    }
    if (L == 0) {
      _buf[0] = '\0';
    }
    _len = strnlen(_buf, _cap - 1);
    if (_len == 0) { _len = 1; _buf[0] = ' '; _buf[1] = '\0'; }
    if (_cursor >= _len) _cursor = _len - 1;
  }

  void markLineDirty(UIContext& ctx) {
    // redraw a rectangle where input is shown
    const auto& th = ctx.theme;
    int y = th.pad + th.lineH + 30;
    ctx.invalidateRect(0, y - 10, ctx.width, th.lineH * 3);
  }

  void drawInputLine(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    int y = th.pad + th.lineH + 30;

    // Clear region
    d.fillRect(0, y - 10, ctx.width, th.lineH * 3, th.bg);

    // Draw a simple box
    d.drawRect(th.pad, y - 6, ctx.width - th.pad * 2, th.lineH + 12, th.muted);

    // Render text (password masked)
    d.setTextSize(th.textSize);
    d.setTextColor(th.fg);
    d.setCursor(th.pad + 6, y);

    char renderBuf[96];
    if (_password) {
      size_t L = strnlen(_buf, min(_cap - 1, sizeof(renderBuf) - 1));
      for (size_t i = 0; i < L; i++) renderBuf[i] = (_buf[i] == ' ') ? ' ' : '*';
      renderBuf[L] = '\0';
      d.print(renderBuf);
    } else {
      d.print(_buf);
    }

    // Cursor underline
    // Compute approx cursor X by measuring substring
    char tmp[96];
    size_t L = strnlen(_buf, min(_cap - 1, sizeof(tmp) - 1));
    if (_cursor >= L) return;

    memcpy(tmp, _buf, _cursor);
    tmp[_cursor] = '\0';

    int16_t x1, y1;
    uint16_t w, h;
    d.getTextBounds(tmp, 0, 0, &x1, &y1, &w, &h);

    int cursorX = (th.pad + 6) + (int)w;
    int cursorY = y + th.lineH - 3;
    d.drawFastHLine(cursorX, cursorY, 10, th.accent);
  }

  void drawTouchControls(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    d.fillRect(0, kCtrlTopY - 4, ctx.width, (kRowH * 2) + kRowGap + 8, th.bg);

    drawButton(d, th, navButtonRect(ctx.width, 0), "L");
    drawButton(d, th, navButtonRect(ctx.width, 1), "R");
    drawButton(d, th, navButtonRect(ctx.width, 2), "+");
    drawButton(d, th, navButtonRect(ctx.width, 3), "-");

    drawButton(d, th, actionButtonRect(ctx.width, 0), "Save");
    drawButton(d, th, actionButtonRect(ctx.width, 1), "Cancel");
    drawButton(d, th, actionButtonRect(ctx.width, 2), "Clear");
  }

public:
  static Control hitTestControl(int16_t x, int16_t y, int screenW) {
    for (int i = 0; i < 4; i++) {
      UIRect r = navButtonRect(screenW, i);
      if (pointInRect(x, y, r)) {
        if (i == 0) return Control::MoveLeft;
        if (i == 1) return Control::MoveRight;
        if (i == 2) return Control::CharUp;
        return Control::CharDown;
      }
    }

    for (int i = 0; i < 3; i++) {
      UIRect r = actionButtonRect(screenW, i);
      if (pointInRect(x, y, r)) {
        if (i == 0) return Control::Save;
        if (i == 1) return Control::Cancel;
        return Control::Clear;
      }
    }

    return Control::None;
  }

private:
  static bool pointInRect(int16_t x, int16_t y, const UIRect& r) {
    return (x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h));
  }

  static UIRect navButtonRect(int screenW, int index) {
    int available = screenW - (kOuterPad * 2) - (kInnerGap * 3);
    int w = available / 4;
    int x = kOuterPad + index * (w + kInnerGap);
    UIRect r;
    r.x = (int16_t)x;
    r.y = (int16_t)kCtrlTopY;
    r.w = (int16_t)w;
    r.h = (int16_t)kRowH;
    r.valid = true;
    return r;
  }

  static UIRect actionButtonRect(int screenW, int index) {
    int y = kCtrlTopY + kRowH + kRowGap;
    int available = screenW - (kOuterPad * 2) - (kInnerGap * 2);
    int w = available / 3;
    int x = kOuterPad + index * (w + kInnerGap);
    UIRect r;
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.w = (int16_t)w;
    r.h = (int16_t)kRowH;
    r.valid = true;
    return r;
  }

  static void drawButton(Adafruit_GFX& d, const UITheme& th, const UIRect& r, const char* label) {
    d.drawRoundRect(r.x, r.y, r.w, r.h, 6, th.accent);
    d.setTextColor(th.accent);
    d.setTextSize(2);

    int16_t x1, y1;
    uint16_t tw, thh;
    d.getTextBounds(label, 0, 0, &x1, &y1, &tw, &thh);
    d.setCursor(r.x + (r.w - (int)tw) / 2, r.y + (r.h - (int)thh) / 2 + 2);
    d.print(label);
  }
};