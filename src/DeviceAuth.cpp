#include "DeviceAuth.h"
#include <Preferences.h>
#include <esp_random.h>

// ── Private state ──────────────────────────────────────────────────────────
static int   s_count                                               = 0;
static char  s_tokens[DeviceAuth::kMaxDevices][DeviceAuth::kTokenLen + 1] = {};
static char  s_pendingCode[DeviceAuth::kCodeLen + 1]               = {};
static unsigned long s_codeGeneratedMs                             = 0;
static bool  s_enrollmentComplete                                  = false;

// ── Public implementation ──────────────────────────────────────────────────

void DeviceAuth::begin() {
    s_count = 0;
    memset(s_tokens, 0, sizeof(s_tokens));
    Preferences p;
    if (!p.begin("devauth", true)) return;

    // Rebuild from token slots so enrollment survives partial writes where
    // the count key may be stale after an unexpected reset.
    char key[6];
    for (int i = 0; i < kMaxDevices; i++) {
        snprintf(key, sizeof(key), "tok%d", i);
        String tok = p.getString(key, "");
        if ((int)tok.length() != kTokenLen) continue;
        strlcpy(s_tokens[s_count], tok.c_str(), kTokenLen + 1);
        s_count++;
    }
    p.end();
}

bool DeviceAuth::isAuthorized(const String& token) {
    if ((int)token.length() != kTokenLen) return false;
    for (int i = 0; i < s_count; i++) {
        if (strncmp(s_tokens[i], token.c_str(), kTokenLen) == 0) return true;
    }
    return false;
}

bool DeviceAuth::isEnabled()      { return s_count > 0; }
int  DeviceAuth::getDeviceCount() { return s_count; }
bool DeviceAuth::isFull()         { return s_count >= kMaxDevices; }

void DeviceAuth::generateEnrollCode() {
    // Unambiguous charset: no O/0/1/I to reduce scan errors.
    static const char kAlpha[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < kCodeLen; i++) {
        s_pendingCode[i] = kAlpha[esp_random() % 32];
    }
    s_pendingCode[kCodeLen] = '\0';
    s_codeGeneratedMs   = millis();
    s_enrollmentComplete = false;
}

const char* DeviceAuth::getPendingCode() { return s_pendingCode; }

bool DeviceAuth::codeExpiredOrUsed() {
    if (s_pendingCode[0] == '\0') return true;
    return (millis() - s_codeGeneratedMs) > kCodeExpiryMs;
}

String DeviceAuth::completeEnrollment(const char* code) {
    if (!code || strlen(code) != (size_t)kCodeLen) return "";
    if (s_pendingCode[0] == '\0') return "";
    if ((millis() - s_codeGeneratedMs) > kCodeExpiryMs) return "";
    if (strncmp(s_pendingCode, code, kCodeLen) != 0) return "";
    if (isFull()) return "";

    // Generate a 32-char hex token from ESP32 hardware RNG.
    char token[kTokenLen + 1];
    for (int i = 0; i < kTokenLen; i += 8) {
        snprintf(token + i, 9, "%08x", (unsigned int)esp_random());
    }
    token[kTokenLen] = '\0';

    strlcpy(s_tokens[s_count], token, kTokenLen + 1);
    s_count++;
    s_pendingCode[0]     = '\0'; // single-use
    s_enrollmentComplete = true;

    // Persist a full snapshot to keep keys/count consistent.
    Preferences p;
    if (p.begin("devauth", false)) {
        char key[6];
        p.putUChar("cnt", (uint8_t)s_count);
        for (int i = 0; i < kMaxDevices; i++) {
            snprintf(key, sizeof(key), "tok%d", i);
            if (i < s_count) p.putString(key, s_tokens[i]);
            else             p.remove(key);
        }
        p.end();
    }
    return String(token);
}

bool DeviceAuth::enrollmentJustCompleted() {
    if (s_enrollmentComplete) {
        s_enrollmentComplete = false;
        return true;
    }
    return false;
}

void DeviceAuth::clearAllDevices() {
    s_count = 0;
    memset(s_tokens, 0, sizeof(s_tokens));
    s_pendingCode[0] = '\0';
    Preferences p;
    if (p.begin("devauth", false)) { p.clear(); p.end(); }
}
