

// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

// Wifi
#define IOT_CONFIG_WIFI_SSID "YOUR_WIFI_SSID"
#define IOT_CONFIG_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define IOT_CONFIG_WIFI_CONNECT_RETRY_MS 10000
#define CONFIG_WEATHER_URL "api.openweathermap.org"
#define CONFIG_WEATHER_KEY "YOUR_OPENWEATHER_API_KEY"

// External web UI URL. When "Web: External" is toggled ON in Settings, visiting
// the device IP in a browser redirects here with ?api=http://<device-ip> appended
// so the remote site knows which device to talk to.
// Set to your Cloudflare Pages / GitHub Pages URL (no trailing slash).
#define WEB_EXTERNAL_URL "https://your-thermostat-ui.example.workers.dev"

// Token that authorizes write access on the external hosted UI.
// Must match the SITE_TOKEN secret set in Cloudflare Workers.
// Scanning the QR on "Add Device" opens the external site with this token,
// permanently authorizing that browser for write access.
#define SITE_TOKEN "change-me-to-a-long-random-secret"

// ── Firebase Realtime Database cloud relay ────────────────────────────────────
// Set FIREBASE_ENABLED to 1 and fill in the host/secret to enable the device
// pushing its state to Firebase every FIREBASE_SYNC_INTERVAL_MS and polling for
// pending commands (setpoint, mode, fault reset, schedule).
//
// FIREBASE_HOST:   "<project>-default-rtdb.firebaseio.com"  (no https://, no slash)
//   Find it in Firebase Console → Realtime Database → Data tab.
// FIREBASE_SECRET: Legacy Database Secret.
//   Firebase Console → Project Settings → Service Accounts → Database Secrets.
// ─────────────────────────────────────────────────────────────────────────────
#define FIREBASE_ENABLED 0
#define FIREBASE_HOST    "your-project-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET  "your-firebase-database-secret"
#define FIREBASE_SYNC_INTERVAL_MS 60000UL


#define IOT_CONFIG_DAYLIGHT_SAVINGS true 
#define IOT_CONFIG_TIME_ZONE -7 
#define IOT_CONFIG_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1 
#define IOT_CONFIG_AZURE_SDK_CLIENT_USER_AGENT "c/1.5.0-beta.1(ard;nanorp2040connect)" 
#define IOT_CONFIG_SAS_TOKEN_EXPIRY_MINUTES 60