#!/usr/bin/env python3
"""Extract RGB565 C arrays from weatherBitmaps.cpp and save as PNG files.
   Black pixels (0x0000) are treated as transparent.
   Output: assets/<name>.png
"""

import re, os, sys
from PIL import Image

SRC   = os.path.join(os.path.dirname(__file__), "..", "src", "weatherBitmaps.cpp")
OUTDIR = os.path.join(os.path.dirname(__file__), "..", "assets")

FACES = {
    "freezy_face_45x45": ("freezy_face_45x45", 45, 45),
    "melt_face_45x45":   ("melt_face_45x45",   45, 45),
    "sun_hot_45x45":     ("sun_hot_45x45",      45, 45),
    "snowflake_45x45":   ("snowflake_45x45",    45, 45),
    "hoodie_45x45":      ("hoodie_45x45",        45, 45),
}

def rgb565_to_rgba(val, transparent_black=True):
    r = ((val >> 11) & 0x1F) << 3
    g = ((val >> 5)  & 0x3F) << 2
    b = ( val        & 0x1F) << 3
    a = 0 if (transparent_black and val == 0x0000) else 255
    return (r, g, b, a)

with open(SRC, "r") as f:
    src = f.read()

# Remove C++ comments
src = re.sub(r'//[^\n]*', '', src)
src = re.sub(r'/\*.*?\*/', '', src, flags=re.DOTALL)

for varname, (slug, w, h) in FACES.items():
    pattern = rf'const\s+uint16_t\s+{re.escape(varname)}\s*\[.*?\]\s*(?:PROGMEM\s*)?\s*=\s*\{{(.*?)\}}\s*;'
    m = re.search(pattern, src, re.DOTALL)
    if not m:
        print(f"  WARNING: {varname} not found", file=sys.stderr)
        continue
    raw = m.group(1)
    vals = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', raw)]
    if len(vals) != w * h:
        print(f"  WARNING: {varname} expected {w*h} values, got {len(vals)}", file=sys.stderr)
        continue
    img = Image.new("RGBA", (w, h))
    img.putdata([rgb565_to_rgba(v) for v in vals])
    # Scale up 3x for readability in the HTML guide
    img = img.resize((w * 3, h * 3), Image.NEAREST)
    out = os.path.join(OUTDIR, f"{slug}.png")
    img.save(out)
    print(f"  Saved {out}")

print("Done.")
