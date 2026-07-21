// FILE: src/MenuItems.h
#pragma once
#include <Arduino.h>
#include "UIContext.h"

class MenuPageBase;

class MenuItem {
public:
  virtual ~MenuItem() = default;
  virtual const char* label() const = 0;
  virtual const char* valueText() const { return nullptr; }

  virtual void activate(UIContext&, MenuPageBase&) {}
  virtual bool adjustable() const { return false; }
  virtual void adjust(UIContext&, int /*delta*/) {}
};

class MenuPageBase {
public:
  virtual void requestPop() = 0;
};

class ActionItem : public MenuItem {
public:
  using Fn = void(*)(UIContext&);
  ActionItem(const char* text, Fn fn) : _label(text), _fn(fn) {}
  const char* label() const override { return _label; }
  void activate(UIContext& ctx, MenuPageBase&) override { if (_fn) _fn(ctx); }
private:
  const char* _label;
  Fn _fn;
};

class ToggleItem : public MenuItem {
public:
  ToggleItem(const char* text, bool* ptr) : _label(text), _ptr(ptr) {}
  const char* label() const override { return _label; }
  const char* valueText() const override { return (_ptr && *_ptr) ? "ON" : "OFF"; }
  void activate(UIContext&, MenuPageBase&) override { if (_ptr) *_ptr = !*_ptr; }
private:
  const char* _label;
  bool* _ptr;
};

// Like ToggleItem but fires an optional callback after the flip (useful for NVS persistence).
class TogglePersistItem : public MenuItem {
public:
  using OnChange = void(*)(bool newValue);
  TogglePersistItem(const char* text, bool* ptr, OnChange cb = nullptr)
    : _label(text), _ptr(ptr), _cb(cb) {}
  const char* label() const override { return _label; }
  const char* valueText() const override { return (_ptr && *_ptr) ? "ON" : "OFF"; }
  void activate(UIContext&, MenuPageBase&) override {
    if (_ptr) { *_ptr = !*_ptr; if (_cb) _cb(*_ptr); }
  }
private:
  const char* _label;
  bool* _ptr;
  OnChange _cb;
};

class IntItem : public MenuItem {
public:
  IntItem(const char* text, int* ptr, int minV, int maxV, int step=1)
  : _label(text), _ptr(ptr), _minV(minV), _maxV(maxV), _step(step) {}

  const char* label() const override { return _label; }

  const char* valueText() const override {
    if (!_ptr) return nullptr;
    snprintf(_buf, sizeof(_buf), "%d", *_ptr);
    return _buf;
  }

  bool adjustable() const override { return true; }

  void adjust(UIContext&, int delta) override {
    if (!_ptr) return;
    long v = (long)*_ptr + (long)delta * (long)_step;
    if (v < _minV) v = _minV;
    if (v > _maxV) v = _maxV;
    *_ptr = (int)v;
  }

private:
  const char* _label;
  int* _ptr;
  int _minV, _maxV, _step;
  mutable char _buf[16];
};