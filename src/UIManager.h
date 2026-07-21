// FILE: src/UIManager.h
#pragma once
#include "UIPage.h"
#include "UITransition.h"
#include <Adafruit_SPITFT.h>

template <size_t STACK_SIZE = 6>
class UIManager {
public:
  explicit UIManager(UIContext& c) : ctx(c) {}

  UIPage* current() { return (sp >= 0) ? stack[sp] : nullptr; }

  void begin(UIPage* root, uint32_t nowMs) {
    (void)nowMs;
    sp = 0;
    stack[0] = root;
    stack[0]->onEnter(ctx);
    ctx.invalidateAll();
    stack[0]->renderFull(ctx);
    ctx.dirty.clear();
  }

  void push(UIPage* page, uint32_t nowMs, UITransitionType t = UITransitionType::WipeLeft) {
    if (sp + 1 >= (int)STACK_SIZE) return;

    stack[sp]->onExit(ctx);
    sp++;
    stack[sp] = page;
    stack[sp]->onEnter(ctx);

    ctx.invalidateAll();
    stack[sp]->renderFull(ctx);
    ctx.dirty.clear();

    trans.begin(t, nowMs, ctx.theme.transitionMs);
  }

  void pop(uint32_t nowMs, UITransitionType t = UITransitionType::WipeRight) {
    if (sp <= 0) return;

    stack[sp]->onExit(ctx);
    sp--;
    stack[sp]->onEnter(ctx);

    ctx.invalidateAll();
    stack[sp]->renderFull(ctx);
    ctx.dirty.clear();

    trans.begin(t, nowMs, ctx.theme.transitionMs);
  }

  void replace(UIPage* page, uint32_t nowMs, UITransitionType t = UITransitionType::WipeLeft) {
    if (sp < 0) return;

    stack[sp]->onExit(ctx);
    stack[sp] = page;
    stack[sp]->onEnter(ctx);

    ctx.invalidateAll();
    stack[sp]->renderFull(ctx);
    ctx.dirty.clear();

    trans.begin(t, nowMs, ctx.theme.transitionMs);
  }

  void tick(uint32_t nowMs, UIEvent ev = UIEvent::None) {
  if (sp < 0) return;
  UIPage* p = stack[sp];

  p->update(ctx, nowMs);

  if (ev != UIEvent::None) {
    if (p->handle(ctx, ev)) {
      // If page didn't specify a rect, redraw all once
      if (!ctx.dirty.valid) ctx.invalidateAll();
    }
  }

  // Redraw only when dirty
  if (ctx.dirty.valid) {
    p->renderDirty(ctx, ctx.dirty);
    ctx.dirty.clear();
  }

  // Transition overlay draws without invalidating.
  if (trans.active) {
    trans.drawOverlay(ctx, nowMs);
  }
}
 
  UIContext& context() { return ctx; }

private:
  UIContext& ctx;
  UIPage* stack[STACK_SIZE]{};
  int sp = -1;
  UITransition trans;
};