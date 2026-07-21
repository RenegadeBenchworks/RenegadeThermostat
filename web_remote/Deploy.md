# Deploying the Remote Web UI to Cloudflare Pages

`web_remote/index.html` is a self-contained static page. It calls the device's REST API
directly from the browser using the `?api=` query-string parameter (e.g.
`https://your-site.pages.dev/?api=http://192.168.1.42`).

No build step, no framework, no server — just one HTML file.

---

## Prerequisites

- A [Cloudflare account](https://dash.cloudflare.com/sign-up) (free tier is sufficient)
- Git installed locally, **or** willingness to use the Cloudflare dashboard drag-and-drop uploader

---

## Option 1 — Drag-and-drop upload (quickest)

1. Log in to the [Cloudflare dashboard](https://dash.cloudflare.com/).
2. In the left sidebar, go to **Workers & Pages → Pages**.
3. Click **Create a project → Upload assets**.
4. Give the project a name (e.g. `thermostat-remote`).
5. Drag the `web_remote/` folder (or just `index.html`) into the upload area, then click
   **Deploy site**.
6. Cloudflare assigns a URL like `https://thermostat-remote.pages.dev`. Done.

To redeploy after changes, open the project in the dashboard → **Deployments → Upload assets**
and repeat step 5.

---

## Option 2 — Git-connected deployment (recommended for ongoing changes)

### 2a. Push to a GitHub/GitLab repo

If the project is not already on GitHub:

```bash
git init
git remote add origin https://github.com/<your-username>/<repo>.git
git add .
git commit -m "initial"
git push -u origin main
```

### 2b. Connect Cloudflare Pages to the repo

1. Cloudflare dashboard → **Workers & Pages → Create application → Pages → Connect to Git**.
2. Authorise Cloudflare to access your GitHub/GitLab account and select the repo.
3. In the **Build settings**:
   | Setting | Value |
   |---|---|
   | Build command | `exit 0` |
   | Deploy command | `exit 0` |
   | Build output directory | `web_remote` |
4. Click **Save and Deploy**.

Every `git push` to `main` now triggers an automatic redeploy.

---

## Accessing the deployed site

The page needs to know your device's IP to talk to the API. Pass it via the `?api=` parameter:

```
https://thermostat-remote.pages.dev/?api=http://192.168.1.42
```

Bookmark that URL on your phone. Replace `192.168.1.42` with whatever IP the thermostat
receives from your router (see **Settings → Device Info** on the thermostat screen, or check
your router's DHCP table).

> **Tip — assign a static IP:** Configure your router to give the ESP32's MAC address a fixed
> DHCP lease so the bookmark never breaks.

---

## The mixed-content browser warning

Because the deployed site is served over **HTTPS** but the device API is plain **HTTP**, modern
browsers enforce the *mixed content* policy:

| Browser | Default behaviour |
|---|---|
| Chrome / Edge | Blocks the HTTP request silently |
| Firefox | Blocks the HTTP request |
| Safari | Blocks the HTTP request |

### Workaround — allow mixed content for the Pages domain

This must be done once per browser per device.

**Chrome / Edge:**
1. Open `https://thermostat-remote.pages.dev/?api=http://192.168.1.42`.
2. Click the lock / info icon in the address bar → **Site settings**.
3. Find **Insecure content** → change from `Block` to `Allow`.
4. Reload the page.

**Firefox:**
1. Open the URL above.
2. Click the lock icon → **Connection not secure → More information**.
3. **Permissions** tab → **Load insecure content** → uncheck "Use default" → select **Allow**.

**Safari:** Safari does not expose a per-site mixed content override. Use the Chrome workaround
on a different browser, or use the device's built-in web UI directly (served from the device
over HTTP with no cross-origin issue).

### Permanent alternative — custom Cloudflare domain + HTTP-only device

If you own a domain managed by Cloudflare, you can point a subdomain at the Pages project
(e.g. `thermostat.yourdomain.com`) and enable **Cloudflare SSL/TLS → Full** mode. This does
not change the device-to-browser trust issue — the mixed content policy still applies — but it
gives you a cleaner URL.

The only way to fully eliminate the browser warning is to enable TLS on the device itself
(self-signed certificate). See the HTTPS discussion in the project notes if that becomes a
requirement.

---

## Custom domain (optional)

1. Cloudflare dashboard → your Pages project → **Custom domains → Set up a custom domain**.
2. Enter a subdomain you control (e.g. `thermostat.yourdomain.com`).
3. Cloudflare adds the DNS record automatically if the domain's nameservers are already
   pointed at Cloudflare.

---

## Updating the site

| Method | How to update |
|---|---|
| Drag-and-drop | Dashboard → project → **Deployments → Upload assets** |
| Git-connected | `git push` — Cloudflare redeploys automatically within ~30 s |

---

## File structure expected by Cloudflare Pages

```
web_remote/
├── index.html          ← the entire UI (no other files required)
└── ExternalSite.md     ← this file (ignored at runtime)
```

Cloudflare Pages serves `index.html` as the root of the site. No `_headers`, `_redirects`, or
`wrangler.toml` files are needed for this single-page app.

---

## Firebase Realtime Database relay (recommended for remote access)

Using Firebase as a relay solves the mixed-content / mixed-HTTP problem entirely. The device
pushes its state to Firebase over HTTPS every 30 seconds. The browser reads from Firebase
(HTTPS → HTTPS, no blocked requests) and writes commands that the device picks up on its next
poll.

### 1. Create a Firebase project

1. Go to [console.firebase.google.com](https://console.firebase.google.com) and click
   **Add project**.
2. Give it a name (e.g. `renegade-thermostat`) and follow the wizard (Analytics optional).
3. In the left sidebar, go to **Build → Realtime Database → Create Database**.
4. Choose a region, start in **locked mode** (you will set rules in a moment).

### 2. Set database security rules

In the Realtime Database console, click the **Rules** tab and replace the default with:

```json
{
  "rules": {
    "thermostat": {
      ".read":  "auth != null",
      ".write": "auth != null"
    }
  }
}
```

Click **Publish**. This allows access only to authenticated requests (i.e. requests that
include the database secret).

### 3. Get the database secret

1. Firebase Console → **Project Settings** (gear icon) → **Service Accounts** tab.
2. Scroll to **Database Secrets** → click **Show** next to the existing secret, or generate
   a new one.
3. Copy the secret — treat it like a password and do not commit it to a public repo.

Your database host is shown at the top of the **Data** tab:
`https://<project>-default-rtdb.firebaseio.com` — copy everything after `https://` and before
the trailing `/`.

### 4. Configure the firmware

Open `src/iot_configs.h` and update the Firebase section:

```cpp
#define FIREBASE_ENABLED 1
#define FIREBASE_HOST    "your-project-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET  "your-firebase-database-secret"
#define FIREBASE_SYNC_INTERVAL_MS 30000UL   // adjust if needed
```

Rebuild and upload. The device will now push to `/thermostat/state` and poll
`/thermostat/commands` every 30 seconds when WiFi is connected.

### 5. Store credentials in Cloudflare Pages (secret never touches the browser)

The site includes a Cloudflare Pages Function (`functions/firebase/[[path]].js`) that acts as
a server-side proxy. The browser calls `/firebase/…` on your own Pages domain; the function
appends the database secret and forwards to Firebase. The secret is stored only in Cloudflare.

1. Cloudflare dashboard → your Pages project → **Settings → Environment variables**.
2. Add two variables, setting **Type** to **Secret** for both:

   | Variable name | Value |
   |---|---|
   | `FIREBASE_URL` | `https://your-project-default-rtdb.firebaseio.com` |
   | `FIREBASE_SECRET` | your database secret |

3. Click **Save**.
4. Trigger a redeploy so the function is deployed with the new secrets:
   - **Git-connected:** `git push` — Cloudflare redeploys automatically.
   - **Drag-and-drop:** upload the `web_remote` folder again from
     **Deployments → Upload assets**, and also upload the `functions` folder
     (Cloudflare dashboard → **Functions** tab, or re-upload as a zip that includes both).

> **Note:** For drag-and-drop, zip the entire project root so both `web_remote/` and
> `functions/` are included. Cloudflare Pages detects the `functions/` directory automatically.

### 6. Open the web UI in Firebase mode

> **If you deployed the site before Firebase support was added**, redeploy first (see step 5).

Open the page with `?fb=1` — no credentials in the URL:

```
https://thermostat-remote.pages.dev/?fb=1
```

Bookmark this. The database secret is never exposed in the browser, URL bar, or history.

> **Local testing only:** If you need to test Firebase mode without Cloudflare (e.g. opening
> `index.html` directly from disk), you can still pass credentials explicitly:
> `?fburl=https://your-project-default-rtdb.firebaseio.com&fbkey=your-secret`

### How Firebase mode behaves

| Action | What happens |
|---|---|
| Setpoint / mode change | Command written to Firebase instantly; device applies on next poll (~0–30 s) |
| Fault reset | Same — command written, applied on next poll |
| Schedule save | Schedule written to commands; device applies + saves to NVS on next poll |
| State display | Reads from Firebase; reflects device's last push (up to 30 s old) |
| Auto-refresh interval | Every 10 s in Firebase mode (vs 5 s in direct mode) |

> **Tip:** After sending a setpoint or mode change, the UI updates optimistically. The displayed
> temperature will sync to the actual device reading on the next auto-refresh.
