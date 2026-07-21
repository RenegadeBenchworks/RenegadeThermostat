# ESP32 Touch UI Framework

A reusable UI framework for building touchscreen interfaces on ESP32
devices using TFT displays.

Designed originally for a thermostat UI but flexible enough for many
embedded devices such as: - Thermostats - Weather stations - RV control
panels - Appliance displays - IoT dashboards

------------------------------------------------------------------------

## Features

-   Page-based UI architecture
-   Touch button framework
-   Screen navigation manager
-   RGB565 bitmap icon system
-   Grid layout helpers
-   Minimal RAM usage
-   ESP32 optimized rendering
-   Easily extensible widgets

------------------------------------------------------------------------

## Architecture Overview

Application State\
↓\
UI Manager\
↓\
Pages\
↓\
Buttons\
↓\
Renderer\
↓\
Display

------------------------------------------------------------------------

## Core Components

### UIManager

Controls screen navigation and manages the page stack.

Example:

``` cpp
ctx.ui.push(&settingsPage);
ctx.ui.pop();
```

------------------------------------------------------------------------

### UIPage

Each screen is implemented as a Page class.

Example:

``` cpp
class ThermostatHomePage : public UIPage {

public:

  const char* title() const override {
    return "Home";
  }

  void renderFull(UIContext& ctx) override;

  bool handle(UIContext& ctx, UIEvent ev) override;

};
```

------------------------------------------------------------------------

### Buttons

Buttons define:

-   position
-   size
-   icon
-   callback

Example:

``` cpp
UIButton gear;

gear.id = "settings";
gear.x = 420;
gear.y = 10;
gear.w = 40;
gear.h = 40;

gear.image565 = gear_icon_32;

gear.onPress = [](UIContext& ctx, void*) {
  ctx.ui.push(&settingsPage);
};
```

------------------------------------------------------------------------

### Icons

Icons are stored as RGB565 bitmaps in flash memory.

Example:

``` cpp
const uint16_t gear_icon_32[1024] PROGMEM;
```

Draw:

``` cpp
tft.drawRGBBitmap(x, y, gear_icon_32, 32, 32);
```

------------------------------------------------------------------------

## Layout System

Typical layout zones:

    +--------------------------------+
    | Status Bar                     |
    +--------------------------------+
    | Header                         |
    +--------------------------------+
    |                                |
    |         Main Content           |
    |                                |
    +--------------------------------+
    | Controls                       |
    +--------------------------------+
    | Secondary Info                 |
    +--------------------------------+

------------------------------------------------------------------------

## Hardware Support

Displays: - ST7796 - ILI9488 - ST7789

Touch controllers: - XPT2046

Example display:

480x320 SPI TFT + XPT2046 Touch

------------------------------------------------------------------------

## Quick Start

Clone the repository:

    git clone https://github.com/yourname/esp32-touch-ui

Install dependencies:

-   Adafruit GFX Library
-   Adafruit ST7796S_kbv
-   XPT2046_Touchscreen
-   ArduinoJson

Build using PlatformIO:

    pio run
    pio run --target upload

------------------------------------------------------------------------

## Credits

Graphics Library: Adafruit GFX Library\
https://github.com/adafruit/Adafruit-GFX-Library

Display Driver: Adafruit ST7796S_kbv\
https://github.com/prenticedavid/Adafruit_ST7796S_kbv

Touch Driver: XPT2046_Touchscreen\
https://github.com/PaulStoffregen/XPT2046_Touchscreen

------------------------------------------------------------------------

## License

MIT License

------------------------------------------------------------------------

## Author

Created by Renegade Benchworks
