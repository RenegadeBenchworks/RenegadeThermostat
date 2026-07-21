#pragma once
#include <Arduino.h>

// Lightweight device authorization for the embedded web server.
//
// Flow:
//   1. "Add Device" on System page → DeviceEnrollPage generates a one-time 6-char code.
//   2. A QR code encodes  http://<IP>/enroll?code=XXXXXX
//   3. User scans → browser hits that URL → server calls completeEnrollment().
//   4. Server returns HTML that stores the 32-char auth token in localStorage.
//   5. All control API calls include header  X-Auth-Token: <token>.
//   6. Auth is only enforced once at least one device is enrolled.

class DeviceAuth {
public:
    static constexpr int kMaxDevices    = 8;
    static constexpr int kTokenLen      = 32;   // 16 hex bytes
    static constexpr int kCodeLen       = 6;    // one-time pairing code
    static constexpr unsigned long kCodeExpiryMs = 5UL * 60UL * 1000UL; // 5 min

    // Call once in setup() to load persisted tokens from NVS.
    static void begin();

    // Returns true if a token matches any enrolled device.
    static bool isAuthorized(const String& token);

    // Returns true when at least one device is enrolled (auth enforced).
    static bool isEnabled();

    static int  getDeviceCount();
    static bool isFull();

    // ── One-time enrollment code ──────────────────────────────────────────
    // Call when DeviceEnrollPage becomes visible to create a fresh code.
    static void generateEnrollCode();

    // The current pairing code (null-terminated, kCodeLen chars).
    static const char* getPendingCode();

    // True if the code has been consumed or the 5-min window has passed.
    static bool codeExpiredOrUsed();

    // Validates code, generates a permanent token, persists to NVS.
    // Returns the new token string, or "" on failure.
    static String completeEnrollment(const char* code);

    // One-shot flag: returns true once after a successful enrollment.
    static bool enrollmentJustCompleted();

    // Remove all enrolled devices from NVS.
    static void clearAllDevices();
};
