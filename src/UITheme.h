// FILE: src/UITheme.h
// ============================================================
// UITheme — centralised visual styling for all pages.
//
// HOW TO CHANGE COLORS
//   All colors are RGB565 (16-bit). Use the converter at:
//   https://rickkas7.github.io/rgb565/
//   or compute manually: R(5 bits) G(6 bits) B(5 bits)
//   Format: 0xRRRRGGGGGBBBBB (5-6-5 packed into uint16_t)
//
// HOW TO CHANGE FONTS
//   This app uses the built-in Adafruit GFX raster font scaled
//   via setTextSize(). Scale 1 = 6×8 px per character.
//   Scale 2 = 12×16 px, scale 3 = 18×24 px, etc.
//
//   textSize below sets the DEFAULT scale used by menus,
//   keyboards, and info pages. The home page uses its own
//   per-element sizes (1, 2, 3, or 6) hardcoded in
//   src/ThermostatHomePage.h — edit setTextSize() calls there
//   to change individual element sizes.
//
//   To use a proportional/custom GFX font instead:
//     1. Add the .h font file to src/ (many available at
//        github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts)
//     2. #include the font header in the relevant page file.
//     3. Call ctx.display.setFont(&YourFont) before drawing text.
//     4. Call ctx.display.setFont(nullptr) to revert to built-in.
// ============================================================
#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

struct UITheme {
  // ----------------------------------------------------------
  // Colors (RGB565 — 16-bit)
  // ----------------------------------------------------------
  uint16_t bg        = 0x0000;  // Background — black
  uint16_t fg        = 0xFFFF;  // Primary text / foreground — white
  uint16_t accent    = 0x07E0;  // Active / highlighted elements — green
  uint16_t muted     = 0x7BEF;  // Secondary text, borders, dividers — grey
  uint16_t highlight = 0x39E7;  // Selected state, info panels — blue-grey
  uint16_t danger    = 0xF800;  // Fault indicators, error states — red

  // ----------------------------------------------------------
  // Typography
  // ----------------------------------------------------------
  // Default Adafruit GFX text scale used by menus / keyboards.
  // 1 = 6×8 px per char. Each step multiplies size linearly.
  // Home page element sizes are set per-element in ThermostatHomePage.h.
  uint8_t  textSize  = 2;

  // Custom font — set to a GFXfont pointer to use a proportional font,
  // or leave as nullptr to use the built-in Adafruit GFX raster font.
  //
  // Example (after adding the .h to src/):
  //   #include <Fonts/FreeSans9pt7b.h>
  //   ctx.theme.font = &FreeSans9pt7b;
  //
  // Note: custom fonts ignore setTextSize(). Use setTextSize(1) with them.
  // Browse fonts at: github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts
  const GFXfont* font = nullptr;

  // Helper — applies the theme font to a display.
  // Called automatically at the start of every page render.
  void applyFont(Adafruit_GFX& d) const { d.setFont(font); }

  // ----------------------------------------------------------
  // Layout
  // ----------------------------------------------------------
  uint8_t  pad       = 8;   // Edge padding in pixels (applied inside page borders)
  uint8_t  lineH     = 20;  // Line height in pixels for multi-line text layouts
  uint8_t  statusH   = 18;  // Height of the top status bar in pixels

  // ----------------------------------------------------------
  // Animation
  // ----------------------------------------------------------
  uint16_t transitionMs = 200;  // Page slide-transition duration in milliseconds
};