// Example: simple ESP32 WebServer handlers to expose thermostat API
// NOT compiled into project automatically. Copy into src/ and integrate as needed.

#include <WebServer.h>
#include <ArduinoJson.h>
#include "AppState.h"

extern ThermostatState appState; // from main.cpp

WebServer server(80);

void handleState() {
  StaticJsonDocument<256> doc;
  doc["roomTempF"] = appState.roomTempF;
  doc["setpointF"] = appState.setpointF;
  doc["mode"]     = (appState.mode == HvacMode::Heat) ? "Heat" : (appState.mode == HvacMode::Cool) ? "Cool" : "Off";
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleSetpoint() {
  if (server.method() != HTTP_POST) { server.send(405); return; }
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, server.arg(0));
  if (err) { server.send(400, "text/plain", "bad json"); return; }
  if (!doc.containsKey("setpoint")) { server.send(400, "text/plain", "no setpoint"); return; }
  appState.setpointF = doc["setpoint"].as<float>();
  server.send(200, "text/plain", "ok");
}

void initWebApi() {
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/setpoint", HTTP_POST, handleSetpoint);
  server.begin();
}

// Call initWebApi() from setup() after network is connected.
