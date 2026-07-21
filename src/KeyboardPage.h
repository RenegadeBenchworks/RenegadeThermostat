#pragma once
#include <Arduino.h>
#include "UIPage.h"
#include "UIEvent.h"
#include "UIRect.h"

// Touch-only keyboard driven by UIEvent:
// - Swipe: move selection
// - Tap: press key
// - Long-press / Back: cancel
// - OK key: accept/save
//
// Added features:
// - Shift = one-shot uppercase
// - Double-tap Shift = Caps Lock
// - SYM = symbols mode
//
// Layout: 4 rows
class KeyboardPage : public UIPage {
public:
  using OnDoneFn = void(*)(UIContext&, bool accepted);

  KeyboardPage(const char* title,
               char* buffer,
               size_t capacity,
               bool passwordMode = false,
               OnDoneFn onDone = nullptr)
  : _title(title), _buf(buffer), _cap(capacity),
    _password(passwordMode), _onDone(onDone) {}

  const char* title() const override { return _title; }

  void onEnter(UIContext& ctx) override {
    _done = false;
    _accepted = false;
    _shift = false;
    _capsLock = false;
    _symMode = false;
    _lastShiftTapMs = 0;
    _selRow = 0;
    _selCol = 0;

    if (_buf && _cap > 0) {
      _len = strnlen(_buf, _cap - 1);
    } else {
      _len = 0;
    }
    ctx.invalidateAll();
  }

  bool accepted() const { return _accepted; }
  bool done() const { return _done; }

  static bool hitCloseButton(const UIContext& ctx, int16_t x, int16_t y) {
    int bx = ctx.width - 40;
    int by = 0;
    int bw = 40;
    int bh = 32;
    return (x >= bx && x < (bx + bw) && y >= by && y < (by + bh));
  }

  bool tapAt(UIContext& ctx, int16_t x, int16_t y) {
    int row = 0;
    int col = 0;
    if (!hitTestKey(ctx, x, y, row, col)) {
      return false;
    }

    int oldRow = _selRow;
    int oldCol = _selCol;
    _selRow = (int8_t)row;
    _selCol = (int8_t)col;
    invalidateKey(ctx, oldRow, oldCol);
    invalidateKey(ctx, _selRow, _selCol);
    pressSelected(ctx);
    return true;
  }

  bool handle(UIContext& ctx, UIEvent ev) override {
    switch (ev) {
      case UIEvent::Left:
        moveSel(ctx, 0, -1);
        return true;

      case UIEvent::Right:
        moveSel(ctx, 0, +1);
        return true;

      case UIEvent::Up:
        moveSel(ctx, -1, 0);
        return true;

      case UIEvent::Down:
        moveSel(ctx, +1, 0);
        return true;

      case UIEvent::Select:
        pressSelected(ctx);
        return true;

      case UIEvent::Back:
        _done = true;
        _accepted = false;
        if (_onDone) _onDone(ctx, false);
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
    d.setTextWrap(false);

    // Title
    d.setTextSize(th.textSize);
    d.setTextColor(th.accent);
    d.setCursor(th.pad, th.pad);
    d.print(_title);

    // Dedicated close target: easier to hit than a text key in the bottom row.
    int bx = ctx.width - 36;
    int by = 4;
    int bw = 30;
    int bh = 24;
    d.drawRect(bx, by, bw, bh, th.accent);
    d.setCursor(bx + 10, by + 6);
    d.print("X");

    drawPreview(ctx, true);
    drawKeyboard(ctx, true);
  }

  void renderDirty(UIContext& ctx, const UIRect& dirty) override {
    const auto& th = ctx.theme;
    int previewTop = th.pad + th.lineH + 6;
    int previewBottom = previewTop + (th.lineH + 20) + 28;
    int keyboardTop = keyboardTopY(ctx);

    int dirtyTop = dirty.y;
    int dirtyBottom = dirty.y + dirty.h;

    if (dirtyTop < previewBottom && dirtyBottom > previewTop) {
      drawPreview(ctx, true);
    }
    if (dirtyBottom > keyboardTop) {
      // Paint only key cells that intersect the dirty rect — avoids full keyboard redraw.
      for (int r = 0; r < ROWS; r++) {
        uint8_t count = 0;
        getRow(r, count);
        for (int c = 0; c < (int)count; c++) {
          int kx, ky, kw, kh;
          if (!keyRect(ctx, r, c, kx, ky, kw, kh)) continue;
          if (kx + kw <= dirty.x || kx >= dirty.x + dirty.w) continue;
          if (ky + kh <= dirty.y || ky >= dirty.y + dirty.h) continue;
          drawSingleKey(ctx, r, c);
        }
      }
    }
  }

private:
  const char* _title;
  char* _buf = nullptr;
  size_t _cap = 0;
  bool _password = false;
  OnDoneFn _onDone = nullptr;

  size_t _len = 0;
  bool _shift = false;
  bool _capsLock = false;
  bool _symMode = false;
  bool _done = false;
  bool _accepted = false;
  uint32_t _lastShiftTapMs = 0;

  int8_t _selRow = 0;
  int8_t _selCol = 0;

  enum KeyType : uint8_t {
    KChar,
    KSpace,
    KBack,
    KShift,
    KSym,
    KClear,
    KOk
  };

  struct Key {
    KeyType type;
    char ch;
    const char* label;
    uint8_t wUnits;
  };

  static constexpr uint8_t ROWS = 4;

  static int keyboardTopY(const UIContext& ctx) {
    const auto& th = ctx.theme;
    return th.pad + th.lineH + 6 + (th.lineH + 20) + 28;
  }

  static int keyboardRowHeight(const UIContext& ctx) {
    const auto& th = ctx.theme;
    int top = keyboardTopY(ctx);
    int bottom = ctx.height - th.pad;
    int areaH = bottom - top;
    int rowH = areaH / ROWS;
    if (rowH < 40) rowH = 40;
    return rowH;
  }

  static void invalidatePreview(UIContext& ctx) {
    const auto& th = ctx.theme;
    int y = th.pad + th.lineH + 6;
    int h = (th.lineH + 20) + 28;
    ctx.invalidateRect(0, y, ctx.width, h);
  }

  static void invalidateKeyboard(UIContext& ctx) {
    int top = keyboardTopY(ctx);
    ctx.invalidateRect(0, top, ctx.width, ctx.height - top);
  }

  static bool keyRect(const UIContext& ctx, int row, int col, int& x, int& y, int& w, int& h) {
    if (row < 0 || row >= (int)ROWS) return false;

    uint8_t count = 0;
    const Key* keys = getRow(row, count);
    if (col < 0 || col >= (int)count) return false;

    const auto& th = ctx.theme;
    int rowH = keyboardRowHeight(ctx);
    int top = keyboardTopY(ctx);
    int units = unitsForRow(row);
    int unitW = (ctx.width - th.pad * 2) / units;
    if (unitW < 8) unitW = 8;

    int px = th.pad;
    for (int k = 0; k < col; k++) {
      px += keys[k].wUnits * unitW;
    }

    x = px;
    y = top + row * rowH;
    w = keys[col].wUnits * unitW;
    h = rowH - 8;
    return true;
  }

  static void invalidateKey(UIContext& ctx, int row, int col) {
    int x = 0, y = 0, w = 0, h = 0;
    if (!keyRect(ctx, row, col, x, y, w, h)) return;
    ctx.invalidateRect(x, y, w, h);
  }

  // Row-specific unit counts so rows can have different layouts cleanly.
  static uint8_t unitsForRow(int row) {
    switch (row) {
      case 0: return 20; // 10 keys * 2
      case 1: return 20; // 10 keys * 2
      case 2: return 20; // 9 keys * 2 + back * 2
      default: return 30; // shift+sym+7 letters+space+clr+ok
    }
  }

  static const Key* row0(uint8_t& n) {
    static const Key r[] = {
      {KChar,'1',nullptr,2},{KChar,'2',nullptr,2},{KChar,'3',nullptr,2},{KChar,'4',nullptr,2},{KChar,'5',nullptr,2},
      {KChar,'6',nullptr,2},{KChar,'7',nullptr,2},{KChar,'8',nullptr,2},{KChar,'9',nullptr,2},{KChar,'0',nullptr,2},
    };
    n = sizeof(r)/sizeof(r[0]); return r;
  }

  static const Key* row1(uint8_t& n) {
    static const Key r[] = {
      {KChar,'q',nullptr,2},{KChar,'w',nullptr,2},{KChar,'e',nullptr,2},{KChar,'r',nullptr,2},{KChar,'t',nullptr,2},
      {KChar,'y',nullptr,2},{KChar,'u',nullptr,2},{KChar,'i',nullptr,2},{KChar,'o',nullptr,2},{KChar,'p',nullptr,2},
    };
    n = sizeof(r)/sizeof(r[0]); return r;
  }

  static const Key* row2(uint8_t& n) {
    static const Key r[] = {
      {KChar,'a',nullptr,2},{KChar,'s',nullptr,2},{KChar,'d',nullptr,2},{KChar,'f',nullptr,2},{KChar,'g',nullptr,2},
      {KChar,'h',nullptr,2},{KChar,'j',nullptr,2},{KChar,'k',nullptr,2},{KChar,'l',nullptr,2},
      {KBack,0,"<-",2},
    };
    n = sizeof(r)/sizeof(r[0]); return r;
  }

  static const Key* row3(uint8_t& n) {
    static const Key r[] = {
      {KShift,0,"Shift",4},
      {KSym,  0,"SYM",  4},
      {KChar,'z',nullptr,2},{KChar,'x',nullptr,2},{KChar,'c',nullptr,2},{KChar,'v',nullptr,2},
      {KChar,'b',nullptr,2},{KChar,'n',nullptr,2},{KChar,'m',nullptr,2},
      {KSpace,0,"Space",4},
      {KClear,0,"Clr",2},
      {KOk,0,"OK",2},
    };
    n = sizeof(r)/sizeof(r[0]); return r;
  }

  static const Key* getRow(int row, uint8_t& n) {
    switch (row) {
      case 0: return row0(n);
      case 1: return row1(n);
      case 2: return row2(n);
      default: return row3(n);
    }
  }

  char applyCase(char c) const {
    if (c >= 'a' && c <= 'z') {
      if (_capsLock || _shift) return (char)(c - 'a' + 'A');
    }
    return c;
  }

  char symbolForKey(int row, int col, char fallback) const {
    // Tuned for useful Wi-Fi/password entry.
    static const char symRow0[] = {'!','@','#','$','%','^','&','*','(',')'};
    static const char symRow1[] = {'[',']','{','}','<','>','_','=','+','-'};
    static const char symRow2[] = {':',';','"','\'',',','.','?','/','\\'};

    if (row == 0 && col >= 0 && col < (int)(sizeof(symRow0)/sizeof(symRow0[0]))) return symRow0[col];
    if (row == 1 && col >= 0 && col < (int)(sizeof(symRow1)/sizeof(symRow1[0]))) return symRow1[col];
    if (row == 2 && col >= 0 && col < 9) return symRow2[col];

    return fallback;
  }

  void drawPreview(UIContext& ctx, bool clearArea) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    int y = th.pad + th.lineH + 6;
    int h = th.lineH + 20;

    if (clearArea) d.fillRect(0, y, ctx.width, h + 28, th.bg);

    d.setTextSize(1);
    d.setTextColor(th.muted);
    d.setCursor(th.pad, y);
    d.print("Shift=next  Shift x2=CAPS  SYM=symbols  OK=save  Long=cancel");

    int boxY = y + 12;
    d.drawRect(th.pad, boxY + 8, ctx.width - th.pad*2, th.lineH + 10, th.muted);

    d.setTextSize(th.textSize);
    d.setTextColor(th.fg);
    d.setCursor(th.pad + 6, boxY + 12);

    if (!_buf) return;

    if (_password) {
      char tmp[80];
      size_t L = strnlen(_buf, min(_cap - 1, sizeof(tmp) - 1));
      for (size_t i = 0; i < L; i++) tmp[i] = (_buf[i] == ' ') ? ' ' : '*';
      tmp[L] = '\0';
      d.print(tmp);
    } else {
      d.print(_buf);
    }
  }

  void drawSingleKey(UIContext& ctx, int row, int col) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    uint8_t count = 0;
    const Key* keys = getRow(row, count);
    if (col < 0 || col >= (int)count) return;

    int kx, ky, kw, kh;
    if (!keyRect(ctx, row, col, kx, ky, kw, kh)) return;

    bool selected = (row == _selRow && col == _selCol);
    d.fillRect(kx, ky, kw - 3, kh, selected ? th.accent : th.muted);

    const char* lab = nullptr;
    char one[2] = {0, 0};

    if (keys[col].type == KChar) {
      char c = keys[col].ch;
      if (_symMode) c = symbolForKey(row, col, c);
      else c = applyCase(c);
      one[0] = c;
      lab = one;
    } else {
      lab = keys[col].label;
    }

    d.setTextSize(2);
    d.setTextColor(th.bg);

    int16_t bx, by;
    uint16_t bw, bh;
    d.getTextBounds((char*)lab, 0, 0, &bx, &by, &bw, &bh);
    int tx = kx + (kw - 3 - (int)bw) / 2;
    int ty = ky + (kh - (int)bh) / 2 + 2;

    d.setCursor(tx, ty);
    d.print(lab);

    if (keys[col].type == KShift && (_shift || _capsLock)) {
      d.drawRect(kx + 2, ky + 2, kw - 7, kh - 4, _capsLock ? th.danger : th.fg);
    }
    if (keys[col].type == KSym && _symMode) {
      d.drawRect(kx + 2, ky + 2, kw - 7, kh - 4, th.danger);
    }
  }

  void drawKeyboard(UIContext& ctx, bool clearArea) {
    auto& d = ctx.display;
    const auto& th = ctx.theme;

    int top = keyboardTopY(ctx);
    int bottom = ctx.height - th.pad;
    int areaH = bottom - top;

    if (clearArea) d.fillRect(0, top, ctx.width, areaH, th.bg);

    int rowH = keyboardRowHeight(ctx);

    for (int r = 0; r < ROWS; r++) {
      uint8_t count = 0;
      const Key* keys = getRow(r, count);

      int units = unitsForRow(r);
      int unitW = (ctx.width - th.pad*2) / units;
      if (unitW < 8) unitW = 8;

      int x = th.pad;
      int y = top + r * rowH;

      for (uint8_t k = 0; k < count; k++) {
        int w = keys[k].wUnits * unitW;
        int h = rowH - 8;

        bool selected = (r == _selRow && k == _selCol);

        d.fillRect(x, y, w - 3, h, selected ? th.accent : th.muted);

        const char* lab = nullptr;
        char one[2] = {0, 0};

        if (keys[k].type == KChar) {
          char c = keys[k].ch;
          if (_symMode) c = symbolForKey(r, k, c);
          else c = applyCase(c);
          one[0] = c;
          lab = one;
        } else {
          lab = keys[k].label;
        }

        d.setTextSize(2);
        d.setTextColor(th.bg);

        int16_t bx, by;
        uint16_t bw, bh;
        d.getTextBounds((char*)lab, 0, 0, &bx, &by, &bw, &bh);
        int tx = x + (w - 3 - (int)bw) / 2;
        int ty = y + (h - (int)bh) / 2 + 2;

        d.setCursor(tx, ty);
        d.print(lab);

        if (keys[k].type == KShift && (_shift || _capsLock)) {
          d.drawRect(x+2, y+2, w-7, h-4, _capsLock ? th.danger : th.fg);
        }

        if (keys[k].type == KSym && _symMode) {
          d.drawRect(x+2, y+2, w-7, h-4, th.danger);
        }

        x += w;
      }
    }
  }

  void moveSel(UIContext& ctx, int dRow, int dCol) {
    int newRow = _selRow + dRow;
    if (newRow < 0) newRow = 0;
    if (newRow >= (int)ROWS) newRow = ROWS - 1;

    uint8_t count = 0;
    getRow(newRow, count);

    int newCol = _selCol + dCol;
    if (newCol < 0) newCol = 0;
    if (newCol >= (int)count) newCol = count - 1;

    if (dRow != 0) {
      if (_selCol >= (int)count) newCol = count - 1;
      else newCol = _selCol;
    }

    if (newRow != _selRow || newCol != _selCol) {
      _selRow = (int8_t)newRow;
      _selCol = (int8_t)newCol;
      invalidateKeyboard(ctx);
    }
  }

  bool hitTestKey(UIContext& ctx, int16_t px, int16_t py, int& outRow, int& outCol) {
    const auto& th = ctx.theme;

    int top = keyboardTopY(ctx);
    int bottom = ctx.height - th.pad;
    int areaH = bottom - top;

    int rowH = keyboardRowHeight(ctx);

    for (int r = 0; r < ROWS; r++) {
      uint8_t count = 0;
      const Key* keys = getRow(r, count);

      int units = unitsForRow(r);
      int unitW = (ctx.width - th.pad * 2) / units;
      if (unitW < 8) unitW = 8;

      int x = th.pad;
      int y = top + r * rowH;
      int h = rowH - 8;

      for (uint8_t k = 0; k < count; k++) {
        int w = keys[k].wUnits * unitW;
        int keyW = w;

        if (px >= x && px < (x + keyW) && py >= y && py < (y + h)) {
          outRow = r;
          outCol = (int)k;
          return true;
        }

        x += w;
      }
    }

    return false;
  }

  void pressSelected(UIContext& ctx) {
    uint8_t count = 0;
    const Key* keys = getRow(_selRow, count);
    if (_selCol < 0 || _selCol >= (int)count) return;

    const Key& k = keys[_selCol];
    switch (k.type) {
      case KChar: {
        if (!_buf || _cap < 2) break;
        if (_len + 1 >= _cap) break;

        bool clearShift = (_shift && !_capsLock);
        char c = k.ch;
        if (_symMode) c = symbolForKey(_selRow, _selCol, c);
        else c = applyCase(c);

        _buf[_len++] = c;
        _buf[_len] = '\0';

        if (clearShift) _shift = false;

        invalidatePreview(ctx);
        if (clearShift) invalidateKeyboard(ctx);
      } break;

      case KSpace: {
        if (!_buf || _cap < 2) break;
        if (_len + 1 >= _cap) break;

        bool clearShift = (_shift && !_capsLock);
        _buf[_len++] = ' ';
        _buf[_len] = '\0';

        if (clearShift) _shift = false;

        invalidatePreview(ctx);
        if (clearShift) invalidateKeyboard(ctx);
      } break;

      case KBack: {
        if (!_buf) break;
        if (_len == 0) break;

        _len--;
        _buf[_len] = '\0';
        invalidatePreview(ctx);
      } break;

      case KShift: {
        uint32_t now = millis();
        if (now - _lastShiftTapMs < 400) {
          _capsLock = !_capsLock;
          _shift = false;
        } else {
          if (_capsLock) {
            _capsLock = false;
          } else {
            _shift = !_shift;
          }
        }
        _lastShiftTapMs = now;
        invalidateKeyboard(ctx);
      } break;

      case KSym:
        _symMode = !_symMode;
        invalidateKeyboard(ctx);
        break;

      case KClear:
        {
        bool hadLayoutState = _shift || _capsLock || _symMode;
        if (_buf && _cap > 0) _buf[0] = '\0';
        _len = 0;
        _shift = false;
        _capsLock = false;
        _symMode = false;
        invalidatePreview(ctx);
        if (hadLayoutState) invalidateKeyboard(ctx);
        }
        break;

      case KOk:
        _done = true;
        _accepted = true;
        if (_onDone) _onDone(ctx, true);
        break;
    }
  }
};