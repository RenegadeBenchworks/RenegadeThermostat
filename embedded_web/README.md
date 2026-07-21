Embedded (self-hosted) web UI

This folder contains guidance and an example for hosting the control UI directly on the device (Arduino/ESP32).

Approach:
- Serve the static `index.html` (a trimmed version of `web_remote/index.html`) from an HTTP server on the ESP.
- Implement the REST endpoints on the ESP:
  - `GET /api/state` — return JSON of `ThermostatState`
  - `POST /api/setpoint` — accept JSON {setpoint}
  - `POST /api/mode` — accept JSON {mode}

Example libraries for ESP32:
- `WebServer` (built-in) — simple blocking server for small payloads
- `ESPAsyncWebServer` — non-blocking, recommended for responsiveness

See `example_server.cpp` for a small starting point.
