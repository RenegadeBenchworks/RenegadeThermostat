#pragma once

// FirebaseSync — cloud relay via Firebase Realtime Database REST API.
//
// The device pushes its current state every FIREBASE_SYNC_INTERVAL_MS and polls
// for pending commands written by the remote web UI.  All communication is
// outbound HTTPS so no port-forwarding or TLS cert on the device is required.
//
// Enable and configure in iot_configs.h:
//   #define FIREBASE_ENABLED 1
//   #define FIREBASE_HOST    "your-project-default-rtdb.firebaseio.com"
//   #define FIREBASE_SECRET  "your-database-secret"

#include "iot_configs.h"

#if FIREBASE_ENABLED

namespace FirebaseSync {

  // Called in loop() on a timer while WiFi is connected.
  // Pushes current state and polls + applies any pending commands.
  void tick();

  // Call after locally saving a schedule change so the next tick pushes the
  // updated schedule to /thermostat/schedule.json in Firebase.
  void notifyScheduleChanged();

} // namespace FirebaseSync

#endif // FIREBASE_ENABLED
