// FILE: src/SplashPage.h
// Displays the Renegade Benchworks splash screen for a fixed duration on boot.
#pragma once
#include <Adafruit_SPITFT.h>
#include <pgmspace.h>
#include <string.h>
#include "UIContext.h"
#include "splashBitmap.h"

#ifndef THERMOSTAT_SPLASH_USE_FAST_RENDER
#define THERMOSTAT_SPLASH_USE_FAST_RENDER 1
#endif

class SplashPage {
public:
    // Draw the splash bitmap then block for durationMs milliseconds.
    static void show(UIContext& ctx, uint32_t durationMs = 2000) {
#if THERMOSTAT_SPLASH_USE_FAST_RENDER
        auto& d = static_cast<Adafruit_SPITFT&>(ctx.display);
        static uint16_t rowBuf[kSplashW];

        d.startWrite();
        d.setAddrWindow(0, 0, kSplashW, kSplashH);

        for (uint16_t row = 0; row < kSplashH; ++row) {
            const uint16_t* src = &splash_480x320[row * kSplashW];
            memcpy_P(rowBuf, src, kSplashW * sizeof(uint16_t));
            d.writePixels(rowBuf, kSplashW, true, false);
        }

        d.endWrite();
    #else
        Adafruit_GFX& d = ctx.display;
        d.fillScreen(0x0000);
        d.drawRGBBitmap(0, 0, splash_480x320, kSplashW, kSplashH);
    #endif
        delay(durationMs);
    }
};
