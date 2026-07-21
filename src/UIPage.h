// FILE: src/UIPage.h
#pragma once
#include <Arduino.h>
#include "UIContext.h"
#include "UIEvent.h"
#include "UIRect.h"

// New render model:
// - renderFull(): draw the whole page
// - renderDirty(): optionally redraw only a dirty region (default calls renderFull)
class UIPage {
public:
  virtual ~UIPage() = default;

  virtual const char* title() const = 0;

  virtual void onEnter(UIContext&) {}
  virtual void onExit(UIContext&) {}

  virtual void update(UIContext&, uint32_t /*nowMs*/) {}

  // Return true if handled
  virtual bool handle(UIContext&, UIEvent) { return false; }

  // Required
  virtual void renderFull(UIContext& ctx) = 0;

  // Optional optimization
  virtual void renderDirty(UIContext& ctx, const UIRect& /*dirty*/) {
    renderFull(ctx);
  }
};