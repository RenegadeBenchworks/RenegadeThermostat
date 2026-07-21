#pragma once
#include <Arduino.h>
#include "UIPage.h"
#include "MenuItems.h"
#include "UIStatusBar.h"

template <size_t MAX_ITEMS = 12>
class MenuPage : public UIPage, public MenuPageBase {
public:
  using BackFn = void(*)(UIContext&);
  static constexpr int kRowHeight = 36;
  static constexpr int kRowGap = 4;
  static constexpr int kListTopExtra = 16;

  MenuPage(const char* t, BackFn back = nullptr)
  : _title(t), _back(back) {}

  const char* title() const override { return _title; }

  bool add(MenuItem* item) {
    if (_count >= MAX_ITEMS) return false;
    _items[_count++] = item;
    return true;
  }

  void setStatusProvider(void (*fn)(UIStatusState&)) { statusProvider = fn; }

  void onEnter(UIContext& ctx) override {
    _sel = 0; _top = 0; _editing = false;
    ctx.invalidateAll();
  }

  void requestPop() override { popRequested = true; }
  bool consumePopRequested() { bool v = popRequested; popRequested = false; return v; }

  bool handle(UIContext& ctx, UIEvent ev) override {
    if (_count == 0) return false;

    size_t oldSel = _sel;

    switch (ev) {
      case UIEvent::Up:
        if (_sel > 0) _sel--;
        _editing = false;
        updateScroll(ctx);
        markRowDirty(ctx, oldSel);
        markRowDirty(ctx, _sel);
        return true;

      case UIEvent::Down:
        if (_sel + 1 < _count) _sel++;
        _editing = false;
        updateScroll(ctx);
        markRowDirty(ctx, oldSel);
        markRowDirty(ctx, _sel);
        return true;

      case UIEvent::Left:
        if (_editing && _items[_sel]->adjustable()) {
          _items[_sel]->adjust(ctx, -1);
          markRowDirty(ctx, _sel);
          return true;
        }
        return false;

      case UIEvent::Right:
        if (_editing && _items[_sel]->adjustable()) {
          _items[_sel]->adjust(ctx, +1);
          markRowDirty(ctx, _sel);
          return true;
        }
        return false;

      case UIEvent::Select:
        if (_items[_sel]->adjustable()) {
          _editing = !_editing;
          markRowDirty(ctx, _sel);
        } else {
          _items[_sel]->activate(ctx, *this);
          ctx.invalidateRect(0, 0, ctx.width, ctx.theme.statusH);
        }
        return true;

      case UIEvent::Back:
        if (_editing) {
          _editing = false;
          markRowDirty(ctx, _sel);
          return true;
        }
        if (_back) _back(ctx);
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
    drawStatus(ctx);

    // Back button
    drawBackButton(ctx);

    d.setTextSize(th.textSize);
    d.setTextWrap(false);
    d.setTextColor(th.accent);
    d.setCursor(th.pad + 52, th.statusH + th.pad + 2);
    d.print(_title);

    drawVisibleRows(ctx, true, nullptr);
  }

  void renderDirty(UIContext& ctx, const UIRect& dirty) override {
    // Only fall back to full redraw when the dirty rect is large in BOTH
    // dimensions. Thin full-width strips (status bar / banner cleanup) should
    // stay incremental.
    ctx.theme.applyFont(ctx.display);
    if (dirty.w > (int16_t)(ctx.width * 0.75) && dirty.h > (int16_t)(ctx.height * 0.75)) {
      renderFull(ctx);
      return;
    }

    if (dirty.y < (int16_t)ctx.theme.statusH) drawStatus(ctx);
    drawVisibleRows(ctx, false, &dirty);
  }

  // Returns item index (0-based) whose rendered row contains (px,py),
  // or -1 if outside list X bounds. Snaps to nearest row centre in gaps.
  int hitTestRow(int px, int py, const UIContext& ctx) const {
    if (_count == 0) return -1;
    int top  = listTopY(ctx);
    int step = kRowHeight + kRowGap;
    int lx   = (int)ctx.theme.pad;
    int lw   = (int)ctx.width - (int)ctx.theme.pad * 2;
    if (px < lx || px >= lx + lw) return -1;
    for (int i = 0; i < (int)_count; i++) {
      int y = top + i * step;
      if (py >= y && py < y + kRowHeight) return i;
    }
    int lastBottom = top + ((int)_count - 1) * step + kRowHeight;
    if (py >= lastBottom && py <= (int)ctx.height - 1) return (int)_count - 1;
    int best = -1, bestDist = kRowGap;
    for (int i = 0; i < (int)_count; i++) {
      int centre = top + i * step + kRowHeight / 2;
      int dist   = abs(py - centre);
      if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
  }

  // Expose row invalidation so external code (e.g. main touch handler) can
  // refresh a specific row after activating an item directly.
  void markRowDirty(UIContext& ctx, size_t idx) {
    uint8_t vis = visibleCount(ctx);
    if (idx < _top || idx >= _top + vis) return;

    int row = (int)(idx - _top);
    int y = listTopY(ctx) + row * (kRowHeight + kRowGap);
    ctx.invalidateRect(0, y - 2, ctx.width, kRowHeight + 4);
  }

private:
  const char* _title;
  MenuItem* _items[MAX_ITEMS]{};
  size_t _count = 0;

  size_t _sel = 0;
  size_t _top = 0;
  bool _editing = false;

  BackFn _back = nullptr;

  bool popRequested = false;

  void (*statusProvider)(UIStatusState&) = nullptr;
  UIStatusState status{};

  void drawStatus(UIContext& ctx) {
    if (statusProvider) statusProvider(status);
    UIStatusBar::draw(ctx, status);
  }

  void drawBackButton(UIContext& ctx) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    int x = th.pad;
    int y = th.statusH + th.pad;
    int w = 38;
    int h = 24;

    d.drawRect(x, y, w, h, th.accent);
    d.setTextSize(2);
    d.setTextColor(th.accent);
    d.setCursor(x + 10, y + 5);
    d.print("<");
  }

  uint8_t visibleCount(UIContext& ctx) const {
    const auto& th = ctx.theme;
    int listTop = th.statusH + th.pad + th.lineH + kListTopExtra;
    int usable = (int)ctx.height - listTop - th.pad;
    int v = usable / (kRowHeight + kRowGap);
    if (v < 1) v = 1;
    return (uint8_t)v;
  }

  int listTopY(const UIContext& ctx) const {
    const auto& th = ctx.theme;
    return th.statusH + th.pad + th.lineH + kListTopExtra;
  }

  void updateScroll(UIContext& ctx) {
    uint8_t vis = visibleCount(ctx);
    if (_sel < _top) _top = _sel;
    if (_sel >= _top + vis) _top = _sel - vis + 1;
  }

  void drawVisibleRows(UIContext& ctx, bool full, const UIRect* clip) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    uint8_t vis = visibleCount(ctx);
    int y0 = listTopY(ctx);

    for (uint8_t row = 0; row < vis; row++) {
      size_t idx = _top + row;
      if (idx >= _count) break;

      int y = y0 + row * (kRowHeight + kRowGap);

      if (!full && clip) {
        if (y > clip->y + clip->h || (y + kRowHeight) < clip->y) continue;
      }

      bool selected = (idx == _sel);
      int bx = (int)th.pad;
      int bw = (int)ctx.width - (int)th.pad * 2;

      d.fillRoundRect(bx, y, bw, kRowHeight, 8, selected ? th.muted : th.bg);
      d.drawRoundRect(bx, y, bw, kRowHeight, 8, th.muted);

      d.setTextSize(th.textSize);
      d.setTextWrap(false);
      d.setTextColor(selected ? th.bg : th.fg);

      d.setCursor(th.pad + 8, y + 11);
      d.print(_items[idx]->label());

      const char* v = _items[idx]->valueText();
      if (v) {
        int16_t x1, y1;
        uint16_t w, h;
        d.getTextBounds(v, 0, 0, &x1, &y1, &w, &h);
        int vx = (int)ctx.width - (int)th.pad - (int)w - 8;
        d.setCursor(vx, y + 11);
        d.print(v);

        if (selected && _editing && _items[idx]->adjustable()) {
          d.setCursor(vx - th.pad, y + 11);
          d.print("<");
        }
      }
    }
  }
};