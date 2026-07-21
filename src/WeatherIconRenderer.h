#pragma once
#include <Arduino.h>
#include <pgmspace.h>
#include "UIContext.h"
#include "weatherbitmaps.h"

class WeatherIconRenderer {
public:
  static constexpr int THERMO_W = 15;
  static constexpr int THERMO_H = 35;

  static constexpr int FACE_W = 45;
  static constexpr int FACE_H = 45;

  static constexpr int SUNRISE_W = 47;
  static constexpr int SUNRISE_H = 30;

  static constexpr int SUNSET_W  = 54;
  static constexpr int SUNSET_H  = 30;

  // Generic PROGMEM RGB565 renderer
  // Draws one row at a time, reading pixels safely from flash.
  static void drawRGBBitmapProgmem(UIContext& ctx,
                                   int16_t x,
                                   int16_t y,
                                   const uint16_t* bitmap,
                                   int16_t w,
                                   int16_t h,
                                   bool swapBytes = true) {
    auto& d = ctx.display;

    // Largest bitmap currently used is 54 pixels wide
    uint16_t rowBuf[64];

    if (w > 64) return; // safety guard

    for (int16_t row = 0; row < h; ++row) {
      for (int16_t col = 0; col < w; ++col) {
        uint16_t px = pgm_read_word(&bitmap[row * w + col]);
        rowBuf[col] = swapBytes ? swap565(px) : px;
      }
      d.drawRGBBitmap(x, y + row, rowBuf, w, 1);
    }
  }

  static void drawOutdoorThermometer(UIContext& ctx, int x, int y, float tempF) {
    if (tempF < 40.0f) {
      drawRGBBitmapProgmem(ctx, x, y, thermometer_blue_35px, THERMO_W, THERMO_H,false);
    } else {
      drawRGBBitmapProgmem(ctx, x, y, thermometer_red_35px, THERMO_W, THERMO_H,false);
    }
  }

  static void drawComfortFace(UIContext& ctx, int x, int y, float tempF) {
    if (tempF <= 32.0f) {
      drawRGBBitmapProgmem(ctx, x, y, freezy_face_45x45, FACE_W, FACE_H,false);
      return;
    }

    if (tempF >= 85.0f) {
      drawRGBBitmapProgmem(ctx, x, y, melt_face_45x45, FACE_W, FACE_H,false);
      return;
    }
  }

  // Choose a weather bitmap from description keywords, then fall back by temperature.
  static void drawCurrentWeatherIcon(UIContext& ctx, int x, int y, const char* description, float tempF) {
    String desc = description ? String(description) : String("");
    desc.toLowerCase();

    // Group 6xx: Snow — also catches "rain and snow" combos
    if (desc.indexOf("snow") >= 0 || desc.indexOf("sleet") >= 0 ||
        desc.indexOf("blizzard") >= 0 || desc.indexOf("flurr") >= 0) {
      drawRGBBitmapProgmem(ctx, x, y, snowflake_45x45, FACE_W, FACE_H,false);
      return;
    }

    // Group 800: Clear sky
    if (desc.indexOf("clear") >= 0 || desc.indexOf("sun") >= 0) {
      drawRGBBitmapProgmem(ctx, x, y, sun_hot_45x45, FACE_W, FACE_H,false);
      return;
    }

    // Groups 2xx/3xx/5xx/7xx/80x: Rain, storms, clouds, atmosphere
    if (desc.indexOf("rain") >= 0 || desc.indexOf("drizzle") >= 0 ||
        desc.indexOf("storm") >= 0 || desc.indexOf("thunder") >= 0 ||
        desc.indexOf("shower") >= 0 || desc.indexOf("cloud") >= 0 ||
        desc.indexOf("mist") >= 0  || desc.indexOf("fog") >= 0  ||
        desc.indexOf("smoke") >= 0 || desc.indexOf("haze") >= 0  ||
        desc.indexOf("dust") >= 0  || desc.indexOf("sand") >= 0  ||
        desc.indexOf("ash") >= 0   || desc.indexOf("squall") >= 0 ||
        desc.indexOf("tornado") >= 0) {
      drawRGBBitmapProgmem(ctx, x, y, hoodie_45x45, FACE_W, FACE_H,false);
     // return;
    }

    if (tempF <= 32.0f) {
      drawRGBBitmapProgmem(ctx, x, y, freezy_face_45x45, FACE_W, FACE_H,false);
      return;
    }

      if (tempF >= 70.0f && tempF < 85.0f) {
      drawRGBBitmapProgmem(ctx, x, y, sun_hot_45x45, FACE_W, FACE_H,false);
      return;
    }
    if (tempF >= 85.0f) {
      drawRGBBitmapProgmem(ctx, x, y, melt_face_45x45, FACE_W, FACE_H,false);
      return;
    }

    if (tempF >= 32.0f && tempF <= 70.0f) {
          drawRGBBitmapProgmem(ctx, x, y, hoodie_45x45, FACE_W, FACE_H,false);
      return;
    }

  }

static void drawSunrise(UIContext& ctx, int x, int y) {
  drawRGBBitmapProgmem(ctx, x, y, sunrise_47x30_rgb565, SUNRISE_W, SUNRISE_H, false);
}

static void drawSunset(UIContext& ctx, int x, int y) {
  drawRGBBitmapProgmem(ctx, x, y, sunset_54x30_rgb565, SUNSET_W, SUNSET_H, false);
}

  static void drawPlusIcon(UIContext& ctx, int x, int y, uint16_t color) {
    auto& d = ctx.display;
    d.drawRoundRect(x, y, 40, 28, 5, color);
    d.drawFastHLine(x + 10, y + 14, 20, color);
    d.drawFastVLine(x + 20, y + 6, 16, color);
  }

  static void drawMinusIcon(UIContext& ctx, int x, int y, uint16_t color) {
    auto& d = ctx.display;
    d.drawRoundRect(x, y, 40, 28, 5, color);
    d.drawFastHLine(x + 10, y + 14, 20, color);
  }

  static uint16_t swap565(uint16_t c) {
  return (uint16_t)((c << 8) | (c >> 8));
}

};