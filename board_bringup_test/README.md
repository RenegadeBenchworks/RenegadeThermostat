# Board Bring-Up Test (Nano ESP32)

This standalone PlatformIO project verifies:
- ST7796S TFT display over SPI
- XPT2046 touch controller over SPI
- DS18B20 temperature sensor on D7
- Relay outputs (heat, cool, high fan, low fan)

## Pin Map
- TFT CS: D10
- TFT DC: D8
- TFT RST: D9
- Touch CS: A7
- Touch IRQ: A4
- Temp sensor (DS18B20 data): D7
- Heat relay: D4
- Cool relay: D5
- High fan relay: D3
- Low fan relay: D2
- Heat relay feedback: A3 (active LOW)
- Cool relay feedback: A2 (active LOW)

## Usage
1. Open this folder as a PlatformIO project:
   - board_bringup_test
2. Build/upload for `arduino_nano_esp32`.
3. Open serial monitor at 115200.
4. Use serial commands shown in the menu.

## Serial Commands
- `m` show menu
- `d` redraw display test screen
- `u` reinitialize TFT (hardware reset + init) and redraw display test screen
- `y` run TFT solid color fill test
- `j` run TFT mode sweep diagnostic (MADCTL/COLMOD/inversion)
- `t` read temperature now
- `s` scan candidate pins for DS18B20 bus/device and print first ROM
- `x` read touch now
- `v` toggle live touch stream (raw x/y/z while pressed)
- `q` run QA sequence and print PASS/FAIL summary
- `1` toggle HEAT relay
- `2` toggle COOL relay
- `3` toggle HIGH FAN relay
- `4` toggle LOW FAN relay
- `f` print relay command + feedback status
- `g` print relay GPIO command vs output-level snapshot
- `n` print relay pin mapping (Arduino Dn to raw GPIO)
- `o` toggle relay polarity (active-high/active-low) and re-apply outputs
- `k` force-probe COOL relay pin D5 (raw HIGH then LOW)
- `p` timed HEAT/COOL feedback PASS/FAIL test
- `a` all relays ON
- `z` all relays OFF
- `b` run relay chase test

Relay status output includes feedback validation for heat/cool:
- `fb:CLOSED` means aux feedback contact is closed (pin LOW)
- `fb:OPEN` means aux feedback contact is open (pin HIGH)
- `OK` means command and feedback agree
- `MISMATCH` means command and feedback do not agree

Timed test behavior (`p`):
- Turns HEAT on and expects heat feedback to close, then turns HEAT off and expects feedback to open
- Turns COOL on and expects cool feedback to close, then turns COOL off and expects feedback to open
- Prints per-relay PASS/FAIL plus overall PASS/FAIL

QA sequence (`q`) behavior:
- Redraws the display test screen (visual check)
- Runs DS18B20 auto check
- Runs timed relay-feedback test
- Waits up to 5 seconds for a touch press
- Prints a single summary with PASS/FAIL lines and overall auto-check result

## Notes
- Default relay polarity is active-high (`relayActiveHigh = true` in src/main.cpp), matching transistor-base relay drivers.
- Push-pull output drive is enabled by default (`RELAY_ACTIVE_LOW_OPEN_DRAIN = false`).
- SPI bus is explicitly initialized on Nano headers `D13(SCK)`, `D12(MISO)`, `D11(MOSI)` for TFT + touch.
- Use serial command `o` to toggle active-high/active-low at runtime for quick validation.
- DS18B20 needs a 4.7k pull-up from data to 3.3V.
- On Nano ESP32 with pin remap enabled, OneWire is initialized with the raw GPIO for the D7 header pin.
- Use command `s` if the sensor appears on an unexpected pin label to confirm the actual connected net.
