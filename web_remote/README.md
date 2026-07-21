# Remote Web UI

This folder contains `index.html` — the source of the Cloudflare-hosted remote control UI for RenegadeThermostat.

## How it works

The UI operates in **Firebase proxy mode** by default:

1. The device (ESP32) pushes state, meters, and schedule to Firebase Realtime Database every 30 s and polls for commands.
2. The browser loads `index.html` from your deployed Cloudflare Worker URL.
3. All Firebase reads/writes are routed through the Worker at `/firebase/...` — credentials never touch the browser.
4. Write operations (setpoint, mode, schedule, fault reset) require an auth token stored in `localStorage`.

## Deployment

Deploy this folder to your own Pages/Workers project. To update it:

1. Edit `index.html` here (this file is the source of truth).
2. Copy it to your deployment repo:
   ```powershell
   Copy-Item web_remote/index.html C:/path/to/your/deploy-repo/index.html -Force
   ```
3. Commit and deploy:
   ```powershell
   cd C:/path/to/your/deploy-repo
   git add index.html ; git commit -m "Update UI" ; git push
   npx wrangler deploy
   ```

## Authorizing a browser for write access

1. On the thermostat touchscreen, navigate to **Settings → Add Device**.
2. Scan the QR code — this hits `/enroll-ext?code=XXXXXX` on the device.
3. The device validates the one-time code and redirects the browser to `/?token=<SITE_TOKEN>`.
4. The token is saved to `localStorage`. The browser is now authorized for write access permanently.
5. To revoke: visit `/?token=forget`.

## URL parameters

| Parameter | Description |
|-----------|-------------|
| `?fb=0` | Disable Firebase proxy, fall back to same-origin `/api` (local device mode) |
| `?token=<value>` | Save auth token to localStorage and strip from URL |
| `?token=forget` | Remove auth token from localStorage |

## Cloudflare Worker secrets required

| Secret | Set via |
|--------|--------|
| `FIREBASE_SECRET` | `wrangler secret put FIREBASE_SECRET` or dashboard |
| `SITE_TOKEN` | `wrangler secret put SITE_TOKEN` or dashboard |

`FIREBASE_URL` is stored in `wrangler.toml` as a plain `[vars]` entry.

See `api_spec.md` for the full Firebase data schema.
