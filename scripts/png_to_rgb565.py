#!/usr/bin/env python3
"""Convert a PNG to an RGB565 C array for use with Adafruit GFX / TFT displays."""

import sys
from PIL import Image

SRC  = r"C:\Users\ajols\AppData\Roaming\Code\User\workspaceStorage\vscode-chat-images\image-1776612985433.png"
DST  = r"c:\Users\ajols\OneDrive\Documents\PlatformIO\Projects\ThermostatCamper\src\splashBitmap.h"
W, H = 480, 320          # target display size (fill)
VAR  = "splash_480x320"

img = Image.open(SRC).convert("RGB")

# Maintain aspect ratio, fit inside W x H, then paste on black background
img.thumbnail((W, H), Image.LANCZOS)
bw, bh = img.size
bg = Image.new("RGB", (W, H), (0, 0, 0))
ox = (W - bw) // 2
oy = (H - bh) // 2
bg.paste(img, (ox, oy))

pixels = list(bg.getdata())

def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

with open(DST, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <Arduino.h>\n\n")
    f.write(f"// Auto-generated RGB565 splash bitmap {W}x{H}\n")
    f.write(f"// Source: Renegade Benchworks logo\n")
    f.write(f"static constexpr uint16_t kSplashW = {W};\n")
    f.write(f"static constexpr uint16_t kSplashH = {H};\n\n")
    f.write(f"const uint16_t {VAR}[{W * H}] PROGMEM = {{\n")
    for i, (r, g, b) in enumerate(pixels):
        if i % 16 == 0:
            f.write("  ")
        f.write(f"0x{to_rgb565(r, g, b):04X}")
        if i < len(pixels) - 1:
            f.write(", ")
        if (i + 1) % 16 == 0:
            f.write("\n")
    f.write("\n};\n")

print(f"Written {W}x{H} = {W*H} pixels to {DST}")
print(f"Image placed at offset ({ox}, {oy}), size ({bw}x{bh})")
