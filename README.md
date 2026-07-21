# RenegadeThermostat

Touchscreen thermostat firmware for the Arduino Nano ESP32 (ESP32-S3). It controls independent heat and cool relays, reads indoor temperature from a DS18B20 sensor, shows live local weather on a 480×320 TFT display, and syncs state to Firebase for remote monitoring and control through a Cloudflare-hosted web app.

## Introduction

This project is designed for RV/camper and off-grid style use where reliability and quick serviceability matter.

Key objectives:
- Provide a simple touch-first thermostat UI with local operation.
- Add practical safety behavior for HVAC hardware protection.
- Keep service diagnostics accessible from the in-device System page.
- Enable remote monitoring and control via Firebase + Cloudflare without exposing the device directly to the internet.

Recent system/safety behavior includes:
- Stuck-system fault protection: if heat or cool runs for 30 minutes with no measurable temperature change, the active relay is shut off and a fault is raised.
- Latched fault behavior: when faulted, HVAC outputs remain off until user reset.
- Fault reset from UI: reset is available on Home mode tap and dedicated System page button.
- Anti-short-cycle relay protection for both heat and cool.
- Delay before re-activation after mode changes (Off/Heat/Cool) to prevent rapid compressor/furnace cycling.
- Runtime/stat persistence in NVS (`Preferences`) so service metrics survive power loss.

## Remote Access

The thermostat syncs its state to a **Firebase Realtime Database** every 30 seconds and polls for pending commands. A **Cloudflare Worker** at `https://your-thermostat-ui.example.workers.dev` proxies all Firebase requests so credentials never touch the browser.

### Authorizing a browser for write access

1. On the touchscreen, navigate to **Settings → Add Device**.
2. Scan the QR code with your phone or browser.
3. The device validates the one-time code and redirects to the external web app with a permanent auth token stored in `localStorage`.
4. The browser is now authorized for write access. Controls remain read-only if no valid token is present.

To revoke access from a browser visit `https://your-thermostat-ui.example.workers.dev/?token=forget`.

### Firebase configuration (`src/iot_configs.h`)

```cpp
#define FIREBASE_ENABLED           1
#define FIREBASE_HOST              "<project>-default-rtdb.firebaseio.com"   // no https://, no trailing slash
#define FIREBASE_SECRET            "<legacy database secret>"
#define FIREBASE_SYNC_INTERVAL_MS  30000UL
```

### Cloudflare Worker secrets (set once via dashboard or `wrangler secret put`)

| Secret | Description |
|--------|-------------|
| `FIREBASE_SECRET` | Legacy Firebase Database Secret |
| `SITE_TOKEN` | Shared token that authorizes browser write access |

`FIREBASE_URL` is stored in `wrangler.toml` as a plain var (not sensitive).

## Getting Started

### Installation Process

1. Install VS Code and the PlatformIO IDE extension, or install PlatformIO Core CLI.
2. Clone this repository.
3. Open the repository folder in VS Code.
4. Connect your Arduino Nano ESP32 board via USB.

### Software Dependencies

Dependencies are declared in [platformio.ini](platformio.ini) and fetched automatically by PlatformIO.

Primary libraries include:
- Adafruit GFX
- Adafruit ST7796S/ST77xx display drivers
- XPT2046 touch controller
- OneWire (DS18B20)
- ArduinoJson
- ArduinoHttpClient

### Configuration

Main configuration is in [src/iot_configs.h](src/iot_configs.h):
- WiFi SSID/password
- OpenWeatherMap API key and location coordinates
- Firebase host, secret, and sync interval
- External web UI URL (`WEB_EXTERNAL_URL`)
- `SITE_TOKEN` — must match the Cloudflare Worker secret of the same name

For full setup details (hardware wiring, API setup, touch calibration, theme/fonts, splash, and test notes), see [SETUP.md](SETUP.md).

### Latest Releases

- Main development branch: `main`
- Remote repository: `origin` at `https://github.com/RenegadeBenchworks/RenegadeThermostat.git`

### API References

External APIs/services used:
- OpenWeatherMap One Call API 3.0
- Firebase Realtime Database (REST)
- Cloudflare Workers (proxy + static hosting)

Internal code entry points:
- UI/bootstrap: [src/main.cpp](src/main.cpp)
- HVAC control and safety logic: [src/TemperatureControlSystem.cpp](src/TemperatureControlSystem.cpp)
- Firebase sync: [src/FirebaseSync.cpp](src/FirebaseSync.cpp)
- Device enrollment / auth: [src/DeviceAuth.cpp](src/DeviceAuth.cpp), [src/DeviceEnrollPage.h](src/DeviceEnrollPage.h)

### System Page Quick Guide

Open path on device:
1. Home screen → tap Settings
2. In Settings, tap System

What you can do on the System page:
- View firmware build date
- View total heat runtime and cool runtime
- View average time to reach setpoint (heat and cool)
- View average time to change room temperature by 1°F (heat and cool)
- Hold-to-reset runtime/stat counters per channel (left card = heat, right card = cool)
- If fault is active, tap Reset Fault to clear fault latch and resume operation
- Tap **Add Device** to show the QR enrollment page for authorizing a remote browser

Service notes:
- Runtime and average stats are persisted to flash and survive power loss
- Fault protection trips when heat or cool runs for 30 minutes without temperature change
- Relay short-cycling protection enforces minimum off-time before re-enable

Screenshot placeholder:
- Add image at: assets/system-page.png
- Then include in this section with:

```markdown
![System Page](assets/system-page.png)
```

## Web Interface

The thermostat exposes a browser-based UI and a REST API over Wi-Fi. Two hosting modes are supported and switchable from the Settings screen on the device.

### Self-Hosted UI (default)

When connected to Wi-Fi, the device serves a full thermostat control page at:

```
http://<device-ip>/
```

The IP address is shown on the **System** page (build stripe, right of the build date).

The page lets you:
- See the current room temperature and active mode.
- Adjust the setpoint with a slider.
- Switch between Off / Heat / Cool modes.
- View any active fault message (red alert bar).

The page polls `/api/state` every 5 seconds and requires no app or installation.

### Externally Hosted UI

`web_remote/index.html` is a standalone file you can deploy to any static host (Cloudflare Pages, GitHub Pages, etc.). It reads a `?api=` query parameter to know which device to talk to.

Example URL after deploying:
```
https://your-thermostat-ui.pages.dev/?api=http://192.168.1.42
```

The remote page automatically reads that parameter, so a single deployed file can control any device on your network. If `?api=` is absent it falls back to `/api` (useful for local testing).

#### Deploying to Cloudflare Pages (or GitHub Pages)

1. Copy `web_remote/index.html` to the root of a new repository (or a `public/` folder for Cloudflare).
2. Connect the repo to Cloudflare Pages / GitHub Pages.
3. After the first build, note the published URL (e.g. `https://your-thermostat-ui.pages.dev`).
4. Set that URL in [src/iot_configs.h](src/iot_configs.h):
   ```c
   #define WEB_EXTERNAL_URL "https://your-thermostat-ui.pages.dev"
   ```
5. Rebuild and flash the firmware.

### Switching Between Self-Hosted and External

In **Settings → Web: External** (the bottom toggle on the Settings page):

| Toggle state | Behaviour when you open the device IP in a browser |
|---|---|
| Off (default) | Device serves the built-in self-hosted page directly. |
| On | Device issues a `302` redirect to `WEB_EXTERNAL_URL?api=http://<device-ip>`. |

The toggle has no effect if `WEB_EXTERNAL_URL` still contains the placeholder string (`https://your-thermostat-ui.pages.dev`). Set a real URL first.

The setting is persisted in NVS and survives reboots.

### REST API

All endpoints support `OPTIONS` preflight (CORS) so the remote page can call them from any origin.

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/state` | Returns current state as JSON. |
| `POST` | `/api/setpoint` | Sets the target temperature. Body: `{"setpointF": 72}` |
| `POST` | `/api/mode` | Sets HVAC mode. Body: `{"mode": "Heat"}` — values: `"Heat"`, `"Cool"`, `"Off"` |
| `POST` | `/api/resetfault` | Clears the active fault latch. No request body needed. |
| `GET` | `/api/schedule` | Returns the full weekly schedule as JSON. |
| `POST` | `/api/schedule` | Saves the weekly schedule. Requires `X-Auth-Token` header. |

`GET /api/state` response fields:

```json
{
  "roomTempF": 71.2,
  "setpointF": 72,
  "mode": "Heat",
  "wifiState": "Connected",
  "lastUpdateMs": 1234567,
  "fault": false,
  "faultMsg": ""
}
```

When `fault` is `true`, `faultMsg` contains the reason (e.g. `"Heat relay did not close"`). Both web UIs display this as a red alert bar below the SET button.

`GET /api/schedule` response shape:

```json
{
  "enabled": true,
  "days": [
    { "morning": {"h":7,"min":0,"sp":70}, "day": {"h":9,"min":0,"sp":72}, "night": {"h":22,"min":0,"sp":68} },
    ...
  ]
}
```

`days` is a 7-element array, index 0 = Monday through index 6 = Sunday. Each day has three period objects (`morning`, `day`, `night`), each with `h` (0–23), `min` (0–59), and `sp` (setpoint °F).

### Temperature Scheduling

The thermostat supports a weekly temperature schedule with three independently configurable periods per day:

| Period | Default start | Default setpoint |
|---|---|---|
| Morning | 07:00 | 70 °F |
| Day | 09:00 | 72 °F |
| Night | 22:00 | 68 °F |

**How it works:**

- When the schedule is enabled, the firmware checks the current time every 60 seconds.
- At each period boundary, the setpoint is automatically updated to the scheduled value.
- **Manual overrides are preserved within a period.** If you manually change the setpoint during the Day period, that change is kept until the Night period begins — the schedule only applies at transition points.
- The previous day's Night setpoint is used for the time before Morning starts each day.
- Scheduling requires a valid NTP time sync. The device syncs automatically after each Wi-Fi connection. The schedule indicator is inactive until NTP succeeds (typically within a few seconds of connecting).

**Configuring the schedule:**

Open the **Schedule** tab on either web interface:
- Toggle **Enable Schedule** on or off.
- For each day (Mon–Sun), set the start time and target temperature for Morning, Day, and Night.
- Click **SAVE SCHEDULE**. Changes are persisted to flash and survive power loss.

The schedule is stored in NVS under namespace `"sched"` / key `"json"` using `Preferences`.

## UI Icons Reference

This section maps every icon on the main screen to the file and method where it is drawn.

### `+` / `−` Stepper Buttons (left column, up/down)

- Drawing code: `drawStepperIcons()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Current implementation is vector line drawing (not bitmap-based).

### Settings Area (top-right)

- The visible UI element is text only: `d.print("Settings")` in `renderFull()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h).
- The previous gear icon has been removed from the current main-screen implementation.

### Cool / Heat / Off Mode Icons (right column chips)

- Drawing code: `drawCoolIcon()` and `drawHeatIcon()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Called from: `redrawModeChip()` in the same file.

### Sunrise / Sunset Icons (bottom card, right side)

- Current drawing code uses bitmaps:
   - `WeatherIconRenderer::drawSunrise()`
   - `WeatherIconRenderer::drawSunset()`
- Called from `redrawSunRow()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Bitmap symbols are:
   - `sunrise_47x30_rgb565`
   - `sunset_54x30_rgb565`
- Bitmap data lives in [src/weatherBitmaps.cpp](src/weatherBitmaps.cpp) and declarations are in [src/weatherBitmaps.h](src/weatherBitmaps.h).

### Weather Cloud / Thermometer / Comfort Face (bottom card, left side)

- Vector cloud: `drawWeatherGlyph()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h), called from `redrawWeatherRow()`.
- Bitmap outdoor thermometer: `WeatherIconRenderer::drawOutdoorThermometer()` in [src/WeatherIconRenderer.h](src/WeatherIconRenderer.h)
- Bitmap comfort faces (shiver / melt): `WeatherIconRenderer::drawComfortFace()` in [src/WeatherIconRenderer.h](src/WeatherIconRenderer.h)
- All bitmaps are stored as RGB565 pixel arrays in [src/weatherBitmaps.h](src/weatherBitmaps.h) / [src/weatherBitmaps.cpp](src/weatherBitmaps.cpp).
- Use [scripts/png_to_rgb565.py](scripts/png_to_rgb565.py) to convert a new PNG to a compatible array.

### Wi-Fi Bars (top status bar, left side)

- Drawing code: inline bar-chart in `UIStatusBar::draw()` in [src/UIStatusBar.h](src/UIStatusBar.h)
- Colour is `th.accent` when connected, `th.danger` when disconnected.

### Changing Bitmap Icons

1. Prepare a PNG at the correct pixel dimensions.
2. Run the conversion script:
   ```powershell
   python scripts/png_to_rgb565.py your_icon.png
   ```
3. Replace the corresponding array in [src/weatherBitmaps.h](src/weatherBitmaps.h) / [src/weatherBitmaps.cpp](src/weatherBitmaps.cpp).
4. Update the `W`/`H` constants in `WeatherIconRenderer` if the dimensions changed.

## Home Screen Position Guide

All home-screen placement is controlled in [src/ThermostatHomePage.h](src/ThermostatHomePage.h). The display is 480x320, and coordinates are pixel-based with origin at top-left.

General workflow for moving any item:
1. Find the drawing method below.
2. Update `x`/`y` values in `setCursor`, `fillRect`, `drawRoundRect`, or icon draw calls.
3. If text or icon trails remain, increase or move the corresponding `fillRect` clear area in the same method.
4. Build and test with:
    - `& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" run`
    - `& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" run --target upload --environment arduino_nano_esp32`

### Top Status Bar

- Wi-Fi icon position: [src/UIStatusBar.h](src/UIStatusBar.h)
   - `iconX = th.pad`
   - `iconY = 4`
- Left status text position: [src/UIStatusBar.h](src/UIStatusBar.h)
   - `textX = iconX + 22` (when Wi-Fi is shown)
   - `setCursor(textX, 4)`
- Right status text position: [src/UIStatusBar.h](src/UIStatusBar.h)
   - auto-aligned from measured text width (`rx`)

### Header and Main Cards

- Settings label: [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
   - `setCursor(ctx.width - 72, th.statusH + 8)`
- Top frame rectangles: [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
   - Plus card: `(14, 56, 84, 72)`
   - Minus card: `(14, 134, 84, 72)`
   - Center temp card: `(106, 56, 220, 152)`
   - Mode rows: `(334, 56, 132, 44)`, `(334, 108, 132, 44)`, `(334, 160, 132, 44)`

### Center Temperature Area

- Method: `redrawCurrentTemp()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Clear area: `fillRect(116, 70, 200, 74, th.bg)`
- "Inside" label: `setCursor(168, 70)`
- Large current temp text anchor: `setCursor(startX, 102)` where `startX` is auto-centered around `195`
- Degree circle: drawn relative to `startX` and measured text width

## **Typography / Fonts**

- **Apply theme font:** the page applies the theme font at the start of full render via `th.applyFont(d)` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h#L84).
- **Current (inside) temperature:** the "Inside" label uses `setTextSize(2)` and the large numeric temperature uses `setTextSize(6)` in `redrawCurrentTemp()` ([src/ThermostatHomePage.h](src/ThermostatHomePage.h#L274) and [src/ThermostatHomePage.h](src/ThermostatHomePage.h#L281)).
- **Outside temperature:** the outside temperature value is drawn with `setTextSize(5)` in `redrawWeatherRow()` ([src/ThermostatHomePage.h](src/ThermostatHomePage.h#L374)).
- **Change the font face:** add a GFX font header to `src/`, set `ctx.theme.font = &YourFont;` (or call `ctx.display.setFont(&YourFont)` before drawing), and remember custom GFX fonts ignore `setTextSize()` scaling.
- **Change sizes:** edit the `setTextSize(...)` calls in `src/ThermostatHomePage.h` at the locations above to adjust sizes for those elements.

### Setpoint Area

- Method: `redrawSetpoint()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Clear area: `fillRect(126, 172, 180, 20, th.bg)`
- Setpoint text anchor: `setCursor(startX, 176)` where `startX` is auto-centered around `216`
- Degree circle: drawn relative to measured text width

### Mode Labels and Icons

- Method: `redrawModeChip()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Cool row text/icon:
   - text `setCursor(372, 74)`
   - icon `drawCoolIcon(..., 346, 78, ...)`
- Heat row text/icon:
   - text `setCursor(372, 126)`
   - icon `drawHeatIcon(..., 346, 130, ...)`
- Off row text/icon:
   - text `setCursor(372, 178)`
   - icon `drawCircle(346, 187, ...)`

### Run Status Banner

- Method: `redrawRunStatus()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Banner rectangle constants:
   - `x = 68`, `y = 26`, `w = 178`, `h = 24`
- Text is horizontally centered with `getTextBounds`, then drawn at `y + 8`

### Weather Card (Left Side)

- Method: `redrawWeatherRow()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Clear area: `fillRect(16, 230, 228, 76, 0xFFFF)`
- Weather glyph anchor: `drawWeatherGlyph(d, 28, 268, ...)`
- Description text: `setCursor(84, 233)`
- Outside temp text: `setCursor(108, 256)`
- High/low text: `setCursor(58, 290)`

### Sunrise and Sunset Times (Main Screen)

- Method: `redrawSunRow()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Clear area: `fillRect(278, 230, 172, 78, 0xFFFF)`
- Sunrise bitmap icon: `WeatherIconRenderer::drawSunrise(ctx, 252, 232)`
- Sunset bitmap icon: `WeatherIconRenderer::drawSunset(ctx, 248, 268)`
- Sunrise label and time:
   - label `setCursor(312, 232)`
   - time `setCursor(310, 250)`
- Sunset label and time:
   - label `setCursor(312, 268)`
   - time `setCursor(310, 286)`

If you only want to move the sunrise/sunset times, edit those four `setCursor` values in `redrawSunRow()`.

### Fault Banner

- Method: `redrawFaultBanner()` in [src/ThermostatHomePage.h](src/ThermostatHomePage.h)
- Clear area: `fillRect(106, 210, 360, 12, th.bg)`
- Banner rectangle: `fillRoundRect(106, 210, 360, 12, 4, kRed)`
- Text anchor: `setCursor(112, 212)`

## Build and Test

### Build

From repo root:

```powershell
& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" run
```

Or inside VS Code: run the PlatformIO Build task.

### Upload

```powershell
& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" run --target upload --environment arduino_nano_esp32
```

### Unit Tests

```powershell
& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" test --environment arduino_nano_esp32
```

If using explicit serial ports for on-device tests:

```powershell
& "C:\Users\$env:USERNAME\.platformio\penv\Scripts\platformio.exe" test -e arduino_nano_esp32 --upload-port COM4 --test-port COM4
```

## Contribute

Contributions are welcome.

Suggested workflow:
1. Create a branch from `main`.
2. Make focused changes with clear commit messages.
3. Build locally and run tests where possible.
4. Open a pull request with a concise summary and validation notes.

When changing safety logic, include the expected HVAC behavior (timing, lockout conditions, and recovery path) in the PR description.