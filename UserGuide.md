# RenegadeThermostat — User Guide

This guide explains how to use your RenegadeThermostat from day to day. No technical knowledge is required.

---

## What You See on the Main Screen

When the thermostat powers on it shows the **Home screen**. The screen is divided into a few areas:

```
┌─────────────────────────────────────────────────────────┐
│  WiFi ●  192.168.1.42              [Settings]           │  ← top bar
│  [Heating]                                              │  ← current mode banner
├──────────┬────────────────────────────┬─────────────────┤
│    ▲     │        Inside              │   [ Cool  ]     │
│   +1°    │         72°F              │   [ Heat  ]     │  ← mode selector
│          │       Set  70°            │   [  Fan  ]     │
│    ▼     │                            │   [  Off  ]     │
│   -1°    │                            │                 │
├──────────┴──────────────┬─────────────┴─────────────────┤
│  🌤  Partly Cloudy       │  ☀ Sunrise  6:42 AM          │
│         58°F             │  🌅 Sunset   7:58 PM          │
│   H: 65   L: 44          │                               │
└─────────────────────────┴───────────────────────────────┘
```

| Area | What it shows |
|---|---|
| **Top bar** | Wi-Fi connection status and the device's IP address |
| **Mode banner** | What the system is currently set to: Heating, Cooling, or Off |
| **▲ / ▼ buttons** | Raise or lower the target temperature by 1°F per tap |
| **Center card** | Current room temperature and the target (set) temperature |
| **Mode selector** | Four buttons — Cool, Heat, Fan, Off — tap one to switch |
| **Bottom left** | Outside weather from the internet |
| **Bottom right** | Today's sunrise and sunset times |

---

## Setting the Temperature

1. Tap **▲** (up arrow, left side) to raise the target temperature.
2. Tap **▼** (down arrow, left side) to lower it.
3. The center card updates immediately to show the new target.

The thermostat will automatically turn on heat or cooling as needed to reach that temperature.

---

## Choosing a Mode

Tap one of the four buttons on the right side of the screen:

| Button | What it does |
|---|---|
| **Cool** | Turns on air conditioning when the room is too warm |
| **Heat** | Turns on the heater when the room is too cold |
| **Fan** | Runs the fan only — no heating or cooling |
| **Off** | Everything off — no heating, cooling, or fan |

The selected button lights up with a color (blue fill for Cool, orange fill for Heat, **cyan text** for Fan — the button stays dark, only the label turns cyan, gray fill for Off), and the mode banner at the top updates to match.

> **Tip:** The thermostat always starts in **Off** mode after a power cycle. Choose your mode after startup.

### Fan Speed

When **Fan** mode is active, the fan runs at **Low** speed by default. Tap the **Fan** button again to toggle between **Low** and **High** speed. The current speed is shown as a small label inside the Fan button.

Fan speed also applies when the **AC compressor is running** in Cool mode — the fan relay switches between high and low speed to match your selection.

### AC Compressor Restart Delay

After the air conditioning compressor has run, the thermostat enforces a **~3-minute wait** before it can restart. This protects the compressor:

> When an air conditioner is in operation, its compressor circulates refrigerant under high pressure. Once off, it will take two to three minutes for this high pressure to equalize. The air conditioning compressor is unable to start against high pressure. Therefore, once the air conditioner is turned off, it is important to leave it off for ~3 min. before restarting.

If you switch between modes without the compressor ever actually running (for example, switching from Heat to Cool and back while the temperature is already at the setpoint), only the normal 20-second anti-short-cycle delay applies.

---

## Understanding the Top Banner

The banner just below the status bar tells you what the system is doing right now:

| Banner text | Meaning |
|---|---|
| **Off** | Mode is set to Off — no HVAC running |
| **Heating** | Heat mode is selected |
| **Cooling** | Cool mode is selected |
| **Fan On** | Fan mode is active (fan running, no heating or cooling) |

---

## Fault Alerts

If something goes wrong (for example, a relay did not respond), a **red bar** appears on the home screen with a short description of the problem. While a fault is active:

- The heater and air conditioner are both turned off for safety.
- You cannot resume until the fault is cleared.

**To clear a fault:**
1. Tap any mode button on the home screen, **or**
2. Go to **Settings → System** and tap **Reset Fault**.

If the fault comes back repeatedly, check that the heater or AC unit is powered and responding.

---

## Settings Screen

Tap **Settings** in the top-right corner of the home screen to open the settings menu.

| Setting | What it does |
|---|---|
| **WiFi SSID** | Change the Wi-Fi network name |
| **WiFi Password** | Change the Wi-Fi password |
| **Reconnect WiFi** | Force a reconnection attempt right now |
| **System** | Open the System/diagnostics page (see below) |
| **Calibrate Touch** | Re-calibrate the touchscreen if taps feel off |
| **Web: External** | Switch the web interface to the externally hosted version (see Web Interface section) |
| **Add Device** | Open the device authorization screen — generate a QR code to authorize a browser for remote control |

Tap the back arrow or home area to return to the main screen.

---

## System (Diagnostics) Page

Go to **Settings → System** to see detailed information about the thermostat's health.

What you can see:
- **Firmware build date** and the device's current **IP address**
- **Heat runtime** — total hours the heater has run
- **Cool runtime** — total hours the AC has run
- **Average time to reach setpoint** (separately for heat and cool)
- **Average time to change temperature by 1°F** (separately for heat and cool)

What you can do:
- **Reset stats** — tap and hold the left card (heat) or right card (cool) to clear those counters
- **Reset Fault** — tap this button if a fault is active and you have resolved the underlying issue

---

## Web Interface — Control from Your Phone or Computer

You can control the thermostat from any device on the same Wi-Fi network — no app to install.

### Finding the Address

The device's IP address is shown in the **top bar** on the home screen (e.g. `192.168.1.42`) and on the **System** page.

### Opening the Web Page

On your phone, tablet, or computer — while on the same Wi-Fi network — open a browser and go to:

```
http://192.168.1.42
```

(Replace `192.168.1.42` with the address shown on your device.)

You will see a control page that mirrors the thermostat screen:

- **Current temperature** displayed at the top
- **Slider** to choose your target temperature
- **OFF / HEAT / COOL** buttons to pick the mode
- **SET THERMOSTAT** button — becomes active when you make a change; tap it to send the new settings to the device
- **Red alert bar** — appears if there is a fault, with a short description

The page refreshes automatically every 5 seconds so it always shows the latest readings.

---

## Externally Hosted Web Interface

If someone has set up a version of the control page on the internet (for example at a web address like `https://your-thermostat-ui.pages.dev`), you can access it from **anywhere** — not just your local Wi-Fi.

### Enabling It

1. On the thermostat, tap **Settings**.
2. Tap **Web: External** to toggle it **ON**.
3. The setting is saved automatically and survives restarts.

### Using It

When "Web: External" is ON, opening the device's IP address in a browser will automatically redirect you to the external page. That page will connect back to your thermostat over your local network.

> **Note:** The external page only works when your phone or computer is on the same Wi-Fi network as the thermostat, or if your network is set up to allow remote access. It just means the page files are loaded from the internet rather than from the device itself.

### Switching Back

Tap **Web: External** again in Settings to toggle it **OFF**. The device will go back to serving its own built-in page.

---

## Authorizing a Device for Web Control

If you are using the **externally hosted web interface**, each browser or device needs to be authorized before it can make changes. Without authorization the page opens in **Read Only** mode — you can see the current state but cannot send commands.

### Why Authorization Is Required

The external web page is accessible from anywhere on the internet. Authorization ensures that only devices you approve can control your thermostat.

### How to Authorize

1. On the thermostat, tap **Settings**.
2. Tap **Add Device**.
3. A screen appears showing a 6-character code and a **QR code**.
4. On your phone or computer, scan the QR code with your camera (or a QR reader app). This opens the external web page with your authorization token already set.
5. The web page will show a green **✓ Authorized** badge — you now have full control.

The code is valid for **5 minutes**. If it expires, go back to **Settings → Add Device** to generate a new one.

> **Note:** Each browser or app on each device needs its own authorization. If you use two different browsers on the same phone, authorize each one separately.

### Authorization Badge

| Badge | Meaning |
|---|---|
| **✓ Authorized** (green) | Full control — you can change the setpoint, mode, and schedule |
| **Read Only** (yellow) | View only — changes are not sent to the device |

### Removing Authorized Devices

Up to **8** devices can be authorized at the same time. To remove all authorizations at once:

1. Tap **Settings → Add Device**.
2. Press and **hold** the **Hold: Remove All** button for 1.5 seconds.
3. All authorized devices are removed. Any previously authorized browser will revert to Read Only mode.

To re-authorize a browser, repeat the QR scan process.

---

## Wi-Fi Tips

- The thermostat connects to Wi-Fi automatically on startup using the saved SSID and password.
- The top bar shows a Wi-Fi signal icon when connected, and turns red when disconnected.
- If the connection is lost, go to **Settings → Reconnect WiFi** to try again without restarting.
- Weather and the web interface both require a Wi-Fi connection. Heating and cooling still work without it.

---

## Quick Reference

| Task | How |
|---|---|
| Change target temperature | Tap **▲** or **▼** on the home screen |
| Switch to Heat / Cool / Fan / Off | Tap the mode button on the right side |
| Change fan speed (High / Low) | Tap the **Fan** button again while already in Fan mode |
| Open the web interface | Go to `http://<device IP>` in a browser |
| Clear a fault | Tap a mode button, or Settings → System → Reset Fault |
| Change Wi-Fi | Settings → WiFi SSID / WiFi Password |
| Force Wi-Fi reconnect | Settings → Reconnect WiFi |
| See runtime stats | Settings → System |
| Toggle external web page | Settings → Web: External |
| Authorize browser for remote control | Settings → Add Device → scan QR code |
| Remove all authorized devices | Settings → Add Device → hold "Remove All" 1.5 s |
