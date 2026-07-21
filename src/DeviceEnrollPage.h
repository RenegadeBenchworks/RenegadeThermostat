#pragma once
#include "UIPage.h"
#include "UIStatusBar.h"
#include "DeviceAuth.h"
#include "iot_configs.h"
#include <WiFi.h>
#include <qrcode.h>

extern bool webExternalMode; // defined in main.cpp

// Displays a QR code the user scans to authorize their browser.
// The QR code encodes:  http://<device-IP>/enroll?code=XXXXXX
// Scanning opens the browser, the server validates the code and issues
// a persistent auth token stored in localStorage.
class DeviceEnrollPage : public UIPage {
public:
    const char* title() const override { return "Add Device"; }

    void setStatusProvider(void (*fn)(UIStatusState&)) { _sp = fn; }

    // Call this just before pushing the page so a fresh code is ready.
    void activate() {
        DeviceAuth::generateEnrollCode();
        _enrolled = false;
    }

    bool handle(UIContext& ctx, UIEvent ev) override {
        if (ev == UIEvent::Back) {
            ctx.invalidateRect(0, 0, ctx.width, ctx.theme.statusH);
            return true;
        }
        // Enrollment completed by the web server → show success screen.
        if (!_enrolled && DeviceAuth::enrollmentJustCompleted()) {
            _enrolled = true;
            renderFull(ctx);
            return false;
        }
        // Auto-refresh the QR code when the 5-min code window expires.
        static unsigned long lastRefreshMs = 0;
        unsigned long now = millis();
        if (!_enrolled && DeviceAuth::codeExpiredOrUsed()
                && (now - lastRefreshMs) > 5000UL) {
            lastRefreshMs = now;
            DeviceAuth::generateEnrollCode();
            renderFull(ctx);
        }
        return false;
    }

    void renderFull(UIContext& ctx) override {
        auto& d = ctx.display;
        const auto& th = ctx.theme;

        d.fillScreen(th.bg);
        th.applyFont(d);

        if (_sp) _sp(_st);
        UIStatusBar::draw(ctx, _st);

        // Back button (same geometry as InfoPage)
        const int bx = th.pad;
        const int by = th.statusH + th.pad;
        d.drawRect(bx, by, 38, 24, th.accent);
        d.setTextSize(2);
        d.setTextColor(th.accent);
        d.setCursor(bx + 10, by + 5);
        d.print("<");

        // Title
        d.setTextSize(th.textSize);
        d.setTextColor(th.accent);
        d.setCursor(th.pad + 52, th.statusH + th.pad + 2);
        d.print("Add Device");

        if (_enrolled) {
            drawSuccess(ctx);
        } else {
            drawQR(ctx);
        }
        d.setTextWrap(false);
    }

    // Hit-test for back button — matches InfoPage / WeatherInfoPage geometry.
    static bool hitBack(const UIContext& ctx, int16_t x, int16_t y) {
        const int bx = 8;
        const int by = ctx.theme.statusH + ctx.theme.pad;
        return (x >= bx && x < bx + 38 && y >= by && y < by + 24)
            || (y <= 60 && x <= 220);
    }

    // Hit-test for the "Remove All" button (bottom-right of right column).
    static bool hitRemoveAll(const UIContext& ctx, int16_t x, int16_t y) {
        int bx, by, bw, bh;
        getRemoveAllRect(ctx, bx, by, bw, bh);
        return (x >= bx && x < bx + bw && y >= by && y < by + bh);
    }

    // Redraws the Remove All button with a progress fill (0.0–1.0).
    // Call from main.cpp while the user holds the button.
    void drawRemoveAllProgress(UIContext& ctx, float progress) {
        int bx, by, bw, bh;
        getRemoveAllRect(ctx, bx, by, bw, bh);
        drawRemoveAllButton(ctx, bx, by, bw, bh, progress);
    }

private:
    void (*_sp)(UIStatusState&) = nullptr;
    UIStatusState _st{};
    bool _enrolled = false;

    static uint8_t qrVersionForMode() {
        return webExternalMode ? 15 : 3;
    }

    static int qrModulePxForMode() {
        return webExternalMode ? 2 : 4;
    }

    static int qrPixelWidthForMode() {
        const int modules = 17 + 4 * (int)qrVersionForMode();
        return modules * qrModulePxForMode();
    }

    static String urlEncode(const String& in) {
        static const char kHex[] = "0123456789ABCDEF";
        String out;
        out.reserve(in.length() * 3);
        for (size_t i = 0; i < in.length(); i++) {
            char c = in[i];
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                out += c;
            } else {
                out += '%';
                out += kHex[((uint8_t)c >> 4) & 0x0F];
                out += kHex[(uint8_t)c & 0x0F];
            }
        }
        return out;
    }

    static void getRemoveAllRect(const UIContext& ctx, int& x, int& y, int& w, int& h) {
        const int contentY = ctx.theme.statusH + 50;
        const int rx = 14 + qrPixelWidthForMode() + 14;
        w = ctx.width - rx - 14;
        h = 22;
        x = rx;
        y = contentY + 4 + 136; // below "Waiting for browser..."
    }

    static void drawRemoveAllButton(UIContext& ctx, int x, int y, int w, int h, float progress) {
        auto& d = ctx.display;
        const auto& th = ctx.theme;
        progress = constrain(progress, 0.0f, 1.0f);
        int fillW = (int)(progress * (w - 2));
        d.fillRoundRect(x, y, w, h, 4, th.bg);
        if (fillW > 0) d.fillRoundRect(x + 1, y + 1, fillW, h - 2, 4, th.danger);
        d.drawRoundRect(x, y, w, h, 4, th.danger);
        uint16_t textColor = (progress >= 0.5f) ? 0xFFFF : th.danger;
        d.setTextSize(1);
        d.setTextColor(textColor);
        d.setTextWrap(false);
        const char* label = "Hold: Remove All";
        int16_t tx1, ty1; uint16_t tw, tht;
        d.getTextBounds(label, 0, 0, &tx1, &ty1, &tw, &tht);
        d.setCursor(x + (w - (int)tw) / 2, y + (h - 8) / 2);
        d.print(label);
    }

    // ── Success screen ──────────────────────────────────────────────────
    static void drawSuccess(UIContext& ctx) {
        auto& d = ctx.display;
        const auto& th = ctx.theme;
        const int cy = ctx.height / 2;

        d.setTextSize(2);
        d.setTextColor(th.accent);
        d.setTextWrap(true);
        d.setCursor(36, cy - 28);
        d.print("Device added!");

        d.setTextSize(1);
        d.setTextColor(th.fg);
        d.setCursor(36, cy + 10);
        d.print("Your browser is now authorized.");
        d.setCursor(36, cy + 26);
        d.print("Tap < to return.");
        d.setTextWrap(false);
    }

    // ── QR code + instructions ──────────────────────────────────────────
    static void drawQR(UIContext& ctx) {
        auto& d = ctx.display;
        const auto& th = ctx.theme;

        const int contentY = ctx.theme.statusH + 50;

        if (WiFi.status() != WL_CONNECTED) {
            d.setTextSize(1);
            d.setTextColor(th.danger);
            d.setTextWrap(true);
            d.setCursor(14, contentY + 20);
            d.print("WiFi not connected.");
            d.setCursor(14, contentY + 36);
            d.print("Connect to WiFi first.");
            d.setTextWrap(false);
            return;
        }

        if (DeviceAuth::isFull()) {
            d.setTextSize(1);
            d.setTextColor(th.danger);
            d.setCursor(14, contentY + 20);
            d.print("Max devices (8) enrolled.");
            return;
        }

        // Build enrollment URL.
        // External mode encodes the external site URL in the QR and passes a
        // local pair endpoint as `pair=`. The web app then redirects to the
        // local endpoint to validate the one-time code.
        String ip = WiFi.localIP().toString();
        String url;
        String enrollUrl = "http://" + ip + "/enroll-ext?code=" + DeviceAuth::getPendingCode();
        if (webExternalMode) {
            String ext = String(WEB_EXTERNAL_URL);
            const bool hasExternalUrl =
                ext.length() > 0 &&
                strncmp(ext.c_str(), "https://your-thermostat", 23) != 0;
            if (hasExternalUrl) {
                url = ext;
                url += (url.indexOf('?') >= 0) ? "&" : "?";
                url += "pair=" + urlEncode(enrollUrl);
            } else {
                // Fallback keeps pairing functional when external URL is unset.
                url = enrollUrl;
            }
        } else {
            url = "http://" + ip + "/enroll?code=" + DeviceAuth::getPendingCode();
        }

        // External-mode payloads can be long; keep buffer sized for version 15.
        static uint8_t qrData[1200];
        QRCode qrc;
        qrcode_initText(&qrc, qrData, qrVersionForMode(), ECC_LOW, url.c_str());

        const int kMod = qrModulePxForMode();
        const int qrPx = qrc.size * kMod;
        const int qrX  = 14;
        // Centre vertically in the content area.
        const int qrY  = contentY + (ctx.height - contentY - qrPx) / 2;

        // White quiet zone
        d.fillRect(qrX - 4, qrY - 4, qrPx + 8, qrPx + 8, 0xFFFF);

        for (uint8_t row = 0; row < qrc.size; row++) {
            for (uint8_t col = 0; col < qrc.size; col++) {
                uint16_t color = qrcode_getModule(&qrc, col, row) ? 0x0000 : 0xFFFF;
                d.fillRect(qrX + col * kMod, qrY + row * kMod, kMod, kMod, color);
            }
        }

        // Right-side instructions column.
        const int rx = qrX + qrPx + 14;
        const int ry = contentY + 4;

        d.setTextWrap(true);
        d.setTextSize(1);

        d.setTextColor(th.fg);
        d.setCursor(rx, ry);
        if (webExternalMode) {
            d.print("Scan QR to open the");
            d.setCursor(rx, ry + 12);
            d.print("remote app (authorized):");
            d.setTextColor(th.muted);
            d.setCursor(rx, ry + 28);
            d.print("Redirects to remote app");
        } else {
            d.print("Scan QR to authorize");
            d.setCursor(rx, ry + 12);
            d.print("this device (local):");
            d.setTextColor(th.muted);
            d.setCursor(rx, ry + 28);
            d.printf("http://%s", ip.c_str());
        }

        // Pairing code in large text.
        d.setTextColor(th.muted);
        d.setTextSize(1);
        d.setCursor(rx, ry + 52);
        d.print("Pairing code:");

        d.setTextSize(2);
        d.setTextColor(th.accent);
        d.setCursor(rx, ry + 64);
        d.print(DeviceAuth::getPendingCode());

        // Device count.
        char buf[32];
        snprintf(buf, sizeof(buf), "Devices: %d / %d",
                 DeviceAuth::getDeviceCount(), DeviceAuth::kMaxDevices);
        d.setTextSize(1);
        d.setTextColor(th.muted);
        d.setCursor(rx, ry + 100);
        d.print(buf);

        // Status.
        d.setTextColor(th.fg);
        d.setCursor(rx, ry + 116);
        d.print("Waiting for browser...");

        // Remove All button — only shown when at least one device is enrolled.
        if (DeviceAuth::getDeviceCount() > 0) {
            int bx, by, bw, bh;
            getRemoveAllRect(ctx, bx, by, bw, bh);
            drawRemoveAllButton(ctx, bx, by, bw, bh, 0.0f);
        }

        d.setTextWrap(false);
    }
};
