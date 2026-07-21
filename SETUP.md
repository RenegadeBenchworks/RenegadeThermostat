# RenegadeThermostat — Setup & Configuration Guide

A touchscreen thermostat for the Arduino Nano ESP32 (ESP32-S3) that controls heat/cool relays, reads an indoor DS18B20 temperature sensor, and fetches live weather from OpenWeatherMap.

---

## Table of Contents

1. [Hardware](#1-hardware)
2. [Software Prerequisites](#2-software-prerequisites)
3. [Project Structure](#3-project-structure)
4. [WiFi Configuration](#4-wifi-configuration)
5. [OpenWeatherMap API](#5-openweathermap-api)
6. [Timezone & Daylight Saving](#6-timezone--daylight-saving)
7. [Weather Refresh Rate](#7-weather-refresh-rate)
8. [Room Temperature Polling](#8-room-temperature-polling)
9. [Thermostat Setpoint Limits & Hysteresis](#9-thermostat-setpoint-limits--hysteresis)
10. [Pin Assignments](#10-pin-assignments)
11. [UI Theme — Colors & Fonts](#11-ui-theme--colors--fonts)
12. [Weather Icons](#12-weather-icons)
13. [Status Bar](#13-status-bar)
14. [System Page Quick Guide](#14-system-page-quick-guide)
15. [Boot Splash Screen](#15-boot-splash-screen)
16. [Building & Flashing](#16-building--flashing)
17. [Running Unit Tests](#17-running-unit-tests)

---

## 1. Hardware

| Component | Notes |
|-----------|-------|
| Arduino Nano ESP32 (ESP32-S3) | Main MCU |
| ST7796S 480×320 TFT display | SPI, Adafruit ST7796S_kbv driver |
| XPT2046 resistive touch panel | SPI, attached to same bus |
| DS18B20 temperature sensor | 1-Wire, OneWire library |
| Heat relay | Active-high digital output |
| Cool relay | Active-high digital output |
| PIR / presence sensor | Digital input (active-high configurable) |
| Backlight | PWM on analog output pin |

---

## 2. Software Prerequisites

- [PlatformIO IDE](https://platformio.org/) (VS Code extension or CLI)
- All library dependencies are declared in `platformio.ini` and fetched automatically on first build

---

## 3. Project Structure

```
src/
  iot_configs.h            ← All user-configurable settings (WiFi, API keys, timing)
  main.cpp                 ← Application entry point, pin definitions
  AppState.h               ← Shared state structs (ThermostatState, WeatherData, HvacMode)
  TemperatureControlSystem ← Heat/cool relay control, mode strategy, fault detection
  TemperatureSensor        ← DS18B20 1-Wire reader
  LocationWeather          ← OpenWeatherMap HTTPS client
  ThermostatHomePage.h     ← Main touchscreen home page
  UITheme.h                ← All colors, font sizes, padding
  WeatherIconRenderer.h    ← Icon drawing helpers
  weatherBitmaps.h/.cpp    ← PROGMEM RGB565 bitmap data for icons
test/
  test_temperature_control/
    test_main.cpp          ← Unity on-device unit tests
```

---

## 4. WiFi Configuration

Edit `src/iot_configs.h`:

```cpp
#define IOT_CONFIG_WIFI_SSID     "YourNetworkName"
#define IOT_CONFIG_WIFI_PASSWORD "YourPassword"
```

The reconnect retry interval (milliseconds):

```cpp
#define IOT_CONFIG_WIFI_CONNECT_RETRY_MS 10000   // default: 10 s
```

WiFi credentials can also be configured at runtime through the on-screen **Settings → WiFi** page, where they are persisted to flash via the `Preferences` library.

---

## 5. OpenWeatherMap API

The app uses the **OpenWeatherMap One Call API 3.0** (HTTPS).

### Steps

1. Register at [openweathermap.org](https://openweathermap.org/) and create a free API key.
2. Subscribe to the *One Call API 3.0* plan (free tier: 1 000 calls/day).
3. Edit `src/iot_configs.h`:

```cpp
#define CONFIG_WEATHER_URL "api.openweathermap.org"   // do not change
#define CONFIG_WEATHER_KEY "your_api_key_here"
```

4. In `src/main.cpp`, set the latitude and longitude of the location you want weather for:

```cpp
// Search for the LocationWeather update call, e.g.:
weather.update(33.4484f, -112.0740f);  // Phoenix, AZ example
```

> Units are **imperial** (°F, mph). To switch to metric, change `&units=imperial` to `&units=metric` in `src/LocalWeather.cpp` and update any display labels accordingly.

---

## 6. Timezone & Daylight Saving

```cpp
#define IOT_CONFIG_TIME_ZONE                    -7   // UTC offset in hours (e.g. -7 = MST)
#define IOT_CONFIG_DAYLIGHT_SAVINGS             true
#define IOT_CONFIG_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF  1  // hours to add when DST is active
```

`main.cpp` also has matching defines (`PST_TIME_ZONE`, `PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF`) used for the NTP configuration — update both if you change timezone.

---

## 7. Weather Refresh Rate

Defined in `src/main.cpp`:

```cpp
static const uint32_t WEATHER_UPDATE_FREQUENCY_MS = 60UL * 60UL * 1000UL;  // 1 hour
static const uint32_t WEATHER_RETRY_FREQUENCY_MS  = 60UL * 1000UL;          // 1 minute on failure
```

Adjust as needed. With a free OpenWeatherMap key (1 000 calls/day) a 1-hour interval uses only 24 calls/day.

---

## 8. Room Temperature Polling

Defined in `src/main.cpp`:

```cpp
static const uint32_t TEMP_SENSOR_POLL_MS = 5000UL;  // read sensor every 5 seconds
```

The DS18B20 is sampled on this interval. To change how frequently the sensor is read, edit that constant.

### Display update threshold

The UI only redraws the current temperature when the displayed **integer** value changes (i.e. it rounds to a different whole degree). This prevents unnecessary screen updates when the raw float drifts sub-degree:

```cpp
// In main.cpp loop:
int displayedRoomTemp = (int)lroundf(sampledRoomTempF);
if (!hasDisplayedRoomTemp || displayedRoomTemp != lastDisplayedRoomTemp) {
    appState.roomTempF = sampledRoomTempF;
    tempController.updateCurrentTemperature(displayedRoomTemp);
    // ... update display ...
}
```

| Behaviour | Value |
|-----------|-------|
| Sensor poll interval | 5 s (`TEMP_SENSOR_POLL_MS`) |
| Display update trigger | Integer °F value changes |

---

## 9. Thermostat Setpoint Limits & Hysteresis

Defined in `src/TemperatureControlSystem.cpp` (constructor call in `src/main.cpp`):

```cpp
// main.cpp — TemperatureController constructor:
// TemperatureController(initialTemp, minTemp, maxTemp, heatPin, coolPin, modeContext)
TemperatureController tempCtrl(72, 50, 90, HEAT_PIN, COOL_PIN, &modeCtx);
//                                     ^^  ^^
//                               min 50°F  max 90°F
```

The **hysteresis band** (prevents relay chatter) is set inside `TemperatureControlSystem.h`:

```cpp
const int hysteresis = 2; // ±2°F dead band around setpoint
```

The **anti-short-cycle delay** (minimum OFF time before either heat or cool can re-energise):

```cpp
const unsigned long delayAfterOff = 20000; // 20 seconds
```

---

## 10. Pin Assignments

All pin definitions are at the top of `src/main.cpp`:

```cpp
// TFT display (SPI)
#define TFT_CS    10
#define TFT_DC     8
#define TFT_RST    9

// Touch controller (SPI, shared bus)
#define TOUCH_CS   D2
#define TOUCH_IRQ  D5

// Peripherals
#define BACKLIGHT_PIN  A0          // PWM backlight control
#define PRESENCE_PIN   A1          // PIR / occupancy sensor input
#define TEMP_SENSOR_PIN GPIO_NUM_10  // DS18B20 1-Wire data line

// Relay output active level (1 = active-high, 0 = active-low)
#define PRESENCE_ACTIVE_HIGH 1
```

---

## 11. UI Theme — Colors & Fonts

All visual styling is centralised in `src/UITheme.h`:

```cpp
struct UITheme {
  // Colors (RGB565)
  uint16_t bg        = 0x0000;  // Background — black
  uint16_t fg        = 0xFFFF;  // Primary text / foreground — white
  uint16_t accent    = 0x07E0;  // Active / highlighted elements — green
  uint16_t muted     = 0x7BEF;  // Secondary text, borders, dividers — grey
  uint16_t highlight = 0x39E7;  // Selected state, info panels — blue-grey
  uint16_t danger    = 0xF800;  // Fault indicators, error states — red

  // Typography
  uint8_t  textSize  = 2;             // Default text scale (1 = 6×8 px per char)
  const GFXfont* font = nullptr;      // Custom font pointer (nullptr = built-in)

  // Layout
  uint8_t  pad       = 8;    // Edge padding in pixels
  uint8_t  lineH     = 20;   // Line height in pixels
  uint8_t  statusH   = 18;   // Status bar height in pixels

  // Animation
  uint16_t transitionMs = 200;  // Page slide-transition duration in ms
};
```

Colors use **RGB565** format. A convenient converter: [rickkas7.github.io/rgb565](https://rickkas7.github.io/rgb565/).

The theme instance is stored in `UIContext` and passed to every page and renderer. To change a color or font application-wide, edit the defaults above — every page automatically picks them up.

### Fonts

The app uses the built-in **Adafruit GFX** raster font by default, scaled via `textSize`. Each page calls `th.applyFont(d)` at the start of its render path, so setting `UITheme::font` is all that's needed to change the font globally.

#### Switching to a custom font (app-wide)

1. Download a font `.h` file from [github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts](https://github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts) and add it to `src/`.
2. Include it in `src/main.cpp`:
   ```cpp
   #include <Fonts/FreeSans9pt7b.h>
   ```
3. Set the font on the theme after `uiCtx` is created in `setup()`:
   ```cpp
   uiCtx.theme.font = &FreeSans9pt7b;
   ```
4. Custom fonts ignore `textSize` — use `setTextSize(1)` alongside them. Per-element size overrides in `ThermostatHomePage.h` may need adjusting.

#### Reverting to the built-in font

```cpp
uiCtx.theme.font = nullptr;
```

#### Per-page font overrides

To use a different font on one page only, call `setFont()` directly inside that page's `renderFull()` after `th.applyFont(d)`:

```cpp
d.setFont(&MySpecialFont);  // override for this page
// ... draw text ...
d.setFont(th.font);         // restore theme font when done
```

#### Text size reference

| `setTextSize()` | Character size | Typical use |
|---|---|---|
| 1 | 6×8 px | Status bar, labels, small info |
| 2 | 12×16 px | Menus, keyboard, default UI |
| 3 | 18×24 px | Setpoint temperature |
| 6 | 36×48 px | Large room-temperature readout |

---

## 12. Weather Icons

Icons are stored as **RGB565 PROGMEM bitmaps** in `src/weatherBitmaps.h` / `src/weatherBitmaps.cpp` and rendered by `src/WeatherIconRenderer.h`.

### Available bitmaps

| Symbol | Size | Description |
|--------|------|-------------|
| `thermometer_blue_35px` | 15×35 | Cold thermometer |
| `thermometer_red_35px` | 15×35 | Warm thermometer |
| `shiver_face_45x45` | 45×45 | "Freezing" comfort face (≤32 °F) |
| `melt_face_45x45` | 45×45 | "Sweltering" comfort face (≥85 °F) |
| `sunrise_47x30_rgb565` | 47×30 | Sunrise icon |
| `sunset_54x30_rgb565` | 54×30 | Sunset icon |

### Replacing an icon

1. Prepare your image in the correct pixel dimensions.
2. Convert to RGB565 using [image2cpp](https://javl.github.io/image2cpp/) — set color format to **RGB565**, output to **C array**.
3. Replace the array data in `src/weatherBitmaps.cpp` (keep the same variable name and `PROGMEM` attribute).
4. Update the width/height constants in `WeatherIconRenderer.h` if the new image has different dimensions.

### Adding a new icon

1. Add the `extern` declaration to `src/weatherBitmaps.h`.
2. Add the array definition to `src/weatherBitmaps.cpp`.
3. Add a `static void drawMyIcon(UIContext& ctx, int x, int y)` helper to `WeatherIconRenderer.h` following the pattern of the existing helpers.

---

## 13. Status Bar

The status bar at the top of every page is rendered by `src/UIStatusBar.h`. It shows:

- **Left text** — e.g. time or page title
- **Right text** — e.g. IP address or build number
- **WiFi indicator** — three-bar icon (green = connected, red outline = disconnected)

The status bar height is controlled by `UITheme::statusH` (default 18 px).

On the home page, the status-bar WiFi icon is hidden because the home page draws its own larger WiFi indicator. This is controlled by setting `status.showWifi = false` before calling `UIStatusBar::draw()` in the home page draw paths.

---

## 14. System Page Quick Guide

The previous "About" service page has been renamed to **System**.

### Navigation

1. From Home, tap the gear icon to open **Settings**.
2. Tap **System**.

### What the System page shows

- Firmware build date
- Total Heat runtime and AC runtime
- Average time to reach setpoint (Heat and Cool)
- Average time to change indoor temperature by 1 degree F (Heat and Cool)

### Service actions on the page

- **Hold to Reset** under each runtime card:
  - Left card resets Heat runtime and Heat averages
  - Right card resets Cool runtime and Cool averages
- **Reset Fault** button (visible only while fault is active):
  - Clears the latched HVAC fault
  - Allows normal HVAC operation to resume

### Safety behavior tied to System diagnostics

- Stuck-system fault protection trips when heat or cool runs for 30 minutes with no measurable temperature change.
- Fault is latched (outputs are forced off) until reset.
- Runtime/stat counters are persisted to NVS (`Preferences`) and survive power loss.

---

## 15. Boot Splash Screen

On boot/power-up, the firmware shows a full-screen Renegade Benchworks logo splash for 2 seconds.

### Files involved

- `src/splashBitmap.h` — generated RGB565 bitmap array (`480x320`) in `PROGMEM`
- `src/SplashPage.h` — splash renderer
- `scripts/png_to_rgb565.py` — conversion helper used to generate `splashBitmap.h`

### Render modes (compile-time toggle)

The splash renderer supports two modes via a compile-time define in `src/SplashPage.h`:

```cpp
#ifndef THERMOSTAT_SPLASH_USE_FAST_RENDER
#define THERMOSTAT_SPLASH_USE_FAST_RENDER 1
#endif
```

- `1` (default): fast row-by-row bulk SPI transfer (`startWrite`, `setAddrWindow`, `writePixels`)
- `0`: legacy `drawRGBBitmap` render path

To override in `platformio.ini`:

```ini
[env:arduino_nano_esp32]
build_flags =
  -DTHERMOSTAT_SPLASH_USE_FAST_RENDER=1
```

### Duration

Splash duration is configured in `src/main.cpp` where the boot flow calls:

```cpp
SplashPage::show(uiCtx, 2000);
```

Change `2000` to any value in milliseconds.

---

## 16. Building & Flashing

```bash
# Build only
platformio run -e arduino_nano_esp32

# Build and upload
platformio run -e arduino_nano_esp32 --target upload

# Open serial monitor (115200 baud)
platformio device monitor --baud 115200

# Build, upload, and open monitor in one step
platformio run -e arduino_nano_esp32 --target upload --target monitor
```

> **OneDrive users:** If your project is stored under OneDrive, the `.pio` temp directory must be redirected outside of OneDrive to avoid sync interference. The `platformio.ini` in this project already includes:
> ```ini
> [platformio]
> build_dir  = C:/Users/<you>/.platformio/build/RenegadeThermostat
> libdeps_dir = C:/Users/<you>/.platformio/libdeps/RenegadeThermostat
> ```
> Update the paths to match your Windows username.

---

## 17. Running Unit Tests

Six Unity-based on-device tests cover the thermostat control logic:

| Test | What it checks |
|------|----------------|
| `test_mode_context_cycles_off_heat_cool_off` | Mode cycle order: Off → Heat → Cool → Off |
| `test_set_mode_from_azure_is_case_insensitive_and_validated` | Mode string parsing is case-insensitive; invalid modes are rejected |
| `test_setpoint_stays_within_bounds` | `increaseTemp` / `decreaseTemp` clamps at 90 °F / 50 °F |
| `test_heat_runtime_accumulates_when_heat_runs` | Heat runtime > 0 after relay energises |
| `test_cool_runtime_accumulates_when_cool_runs` | Cool runtime > 0 after relay energises |
| `test_runtime_setters_roundtrip` | `setHeatRunMs` / `setCoolRunMs` round-trip correctly |

```bash
# Close any serial monitor that has the COM port open, then:
platformio test -e arduino_nano_esp32
```

Results are printed over UART at 115200 baud and summarised in the terminal.
