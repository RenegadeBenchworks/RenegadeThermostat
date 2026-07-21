#!/usr/bin/env python3
"""Embed the device face icons as base64 data-URIs into UserGuide.html."""
import base64, os

root   = os.path.join(os.path.dirname(__file__), "..")
assets = os.path.join(root, "assets")
guide  = os.path.join(root, "UserGuide.html")

def b64uri(name):
    data = open(os.path.join(assets, name + ".png"), "rb").read()
    return "data:image/png;base64," + base64.b64encode(data).decode()

def img_tag(uri, alt):
    return (f'<img src="{uri}" alt="{alt}" width="45" height="45"'
            f' aria-hidden="true" style="image-rendering:pixelated">')

icons = {
    "melt":      b64uri("melt_face_45x45"),
    "sun":       b64uri("sun_hot_45x45"),
    "hoodie":    b64uri("hoodie_45x45"),
    "snowflake": b64uri("snowflake_45x45"),
    "freezy":    b64uri("freezy_face_45x45"),
}

replacements = [
    ('<div class="icon-emoji">\U0001f975</div>',
     '<div class="icon-emoji">' + img_tag(icons["melt"],      "Melty face icon")  + '</div>'),
    ('<div class="icon-emoji">\u2600\ufe0f</div>',
     '<div class="icon-emoji">' + img_tag(icons["sun"],       "Sunny face icon")  + '</div>'),
    ('<div class="icon-emoji">\U0001f9e5</div>',
     '<div class="icon-emoji">' + img_tag(icons["hoodie"],    "Hoodie icon")      + '</div>'),
    ('<div class="icon-emoji">\u2744\ufe0f</div>',
     '<div class="icon-emoji">' + img_tag(icons["snowflake"], "Snowflake icon")   + '</div>'),
    ('<div class="icon-emoji">\U0001f976</div>',
     '<div class="icon-emoji">' + img_tag(icons["freezy"],    "Freezy face icon") + '</div>'),
]

html = open(guide, "r", encoding="utf-8").read()
for old, new in replacements:
    if old in html:
        html = html.replace(old, new, 1)
        print(f"  Replaced emoji for: {new[35:60]}...")
    else:
        print(f"  NOT FOUND: {repr(old)}")

open(guide, "w", encoding="utf-8").write(html)
print("Done.")
