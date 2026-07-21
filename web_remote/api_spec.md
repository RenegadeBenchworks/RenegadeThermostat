# API & Data Schema — RenegadeThermostat

The remote UI uses two transport layers depending on mode.

---

## Firebase Proxy Mode (default — cloud-hosted UI)

All requests are routed through the Cloudflare Worker at `/firebase/<path>`, which appends the Firebase secret server-side.

### Firebase RTDB paths

#### `/thermostat/state.json` — device → Firebase (push every 30 s)
```json
{
  "roomTempF": 75.5,
  "setpointF": 70,
  "mode": "Off",
  "fault": false,
  "faultMsg": ""
}
```

#### `/thermostat/meters.json` — device → Firebase (push every 30 s)
```json
{
  "heatRunSec": 3600,
  "coolRunSec": 1200,
  "heatCycles": 12,
  "coolCycles": 4,
  "avgHeatSetptSec": 900,
  "avgCoolSetptSec": 600
}
```

#### `/thermostat/schedule.json` — device → Firebase (push on change)
```json
{
  "enabled": true,
  "days": [
    { "morning": {"h": 7, "min": 0, "sp": 70}, "day": {"h": 9, "min": 0, "sp": 72}, "night": {"h": 22, "min": 0, "sp": 68} },
    ...  // 7 days: Mon–Sun
  ]
}
```

#### `/thermostat/commands.json` — browser → Firebase (PATCH); device polls and clears
```json
{
  "setpoint": 72,
  "mode": "Heat",
  "resetFault": true,
  "schedule": { ... }   // full schedule object (optional)
}
```
The device applies any present fields and then deletes them from Firebase.

### Authentication

Write requests (`PATCH`, `PUT`, `POST`, `DELETE`) to `/firebase/...` require the header:
```
X-Auth-Token: <SITE_TOKEN>
```
The Worker returns `401 Unauthorized` if the token is missing or wrong. The UI clears the stored token on 401 and shows an error.

---

## Direct Device Mode (LAN — pass `?fb=0`)

The UI calls the device's embedded REST API directly.

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/state` | Current thermostat state |
| `GET` | `/api/meters` | Runtime/cycle counters |
| `GET` | `/api/schedule` | Current schedule |
| `POST` | `/api/setpoint` | Body: `{"setpoint": 72}` |
| `POST` | `/api/mode` | Body: `{"mode": "Heat"}` |
| `POST` | `/api/schedule` | Body: full schedule object |
| `POST` | `/api/resetfault` | No body required |

Authentication in LAN mode uses the `X-Auth-Token` header checked against enrolled device tokens (managed by `DeviceAuth`).

---

## Device enrollment endpoint

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/enroll?code=XXXXXX` | Validates code, returns page that saves token to localStorage for local device auth |
| `GET` | `/enroll-ext?code=XXXXXX` | Validates code, redirects to `WEB_EXTERNAL_URL/?token=SITE_TOKEN` for cloud UI auth |
