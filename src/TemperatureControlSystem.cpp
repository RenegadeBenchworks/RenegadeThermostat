#include "TemperatureControlSystem.h"
#include "SerialLogger.h"

namespace {
constexpr int kModeOff  = 0;
constexpr int kModeHeat = 1;
constexpr int kModeCool = 2;
constexpr int kModeFan  = 3;
constexpr int kModeCount = 4;

// Relay board is active-HIGH: drive pin HIGH to energise, LOW to de-energise.
constexpr uint8_t kRelayOn  = HIGH;
constexpr uint8_t kRelayOff = LOW;

// When an air conditioner is in operation, its compressor circulates refrigerant
// under high pressure.  Once off, it will take two to three minutes for this
// high pressure to equalise.  The air conditioning compressor is unable to start
// against high pressure.  Therefore, once the air conditioner is turned off, it
// is important to leave it off for ~3 min. before restarting.
//
// This delay is only applied after the compressor has actually run at least once
// (_coolCompressorHasRun == true).  If the user cycles modes without the
// compressor ever energising, the normal 20-second anti-short-cycle delay is used.
constexpr unsigned long kCoolDelayAfterOff = 180000UL; // 3 minutes in ms

const char* fanSpeedName(FanSpeed speed) {
    switch (speed) {
        case FanSpeed::High: return "HIGH";
        case FanSpeed::Low:  return "LOW";
        case FanSpeed::Auto:
        default:             return "AUTO";
    }
}
}

// Off Mode
void OffMode::execute(TemperatureController* ctrl) {
    Logger.Info("System is in Off mode.");
    if (ctrl) ctrl->deactivateAllOutputs();
}

// Heating Mode
void HeatingMode::execute(TemperatureController* ctrl) {
    Logger.Info("System is in Heating mode.");
    if (ctrl) ctrl->activateHeatOutput();
}

// Cooling Mode
void CoolingMode::execute(TemperatureController* ctrl) {
    Logger.Info("System is in Cooling mode.");
    if (ctrl) ctrl->activateCoolOutput();
}

// Fan-only Mode
void FanMode::execute(TemperatureController* ctrl) {
    Logger.Info("System is in Fan mode.");
    if (ctrl) ctrl->deactivateAllOutputs();
}

// ---------------- Mode Context Implementation ----------------

ModeContext::ModeContext(TemperatureController* ctrl) : controller(ctrl) {
    modes[0] = new OffMode();
    modes[1] = new HeatingMode();
    modes[2] = new CoolingMode();
    modes[3] = new FanMode();
    modeIndex = kModeOff;
    mode = modes[kModeOff];
}
ModeContext::~ModeContext() {
    delete modes[0];
    delete modes[1];
    delete modes[2];
    delete modes[3];
}

void ModeContext::changeMode() {
    int oldModeIndex = modeIndex;
    modeIndex = (modeIndex + 1) % kModeCount;
    mode = modes[modeIndex];

    if (oldModeIndex != modeIndex && controller != nullptr) {
        controller->setLastOffTime(millis());
    }

    executeMode();
}

void ModeContext::executeMode() {
    mode->execute(controller);
}

const char* ModeContext::getModeName() {
    switch (modeIndex) {
        case kModeOff:  return "Off";
        case kModeHeat: return "Heat";
        case kModeCool: return "Cool";
        case kModeFan:  return "Fan";
        default: return "UNKNOWN";
    }
}
void ModeContext::setTempController(TemperatureController* tempCtrl) {
    controller = tempCtrl;
}

void ModeContext::setModeFromAzure(const String& modeStr) {
    int newModeIndex = -1;

    if (modeStr.equalsIgnoreCase("OFF")) {
        newModeIndex = kModeOff;
    } else if (modeStr.equalsIgnoreCase("HEAT")) {
        newModeIndex = kModeHeat;
    } else if (modeStr.equalsIgnoreCase("COOL")) {
        newModeIndex = kModeCool;
    } else if (modeStr.equalsIgnoreCase("FAN")) {
        newModeIndex = kModeFan;
    } else {
        Logger.Error("Invalid mode received from Azure: " + modeStr);
        return;
    }

    if (newModeIndex == modeIndex) {
        return;
    }

    modeIndex = newModeIndex;
    mode = modes[modeIndex];

    if (controller != nullptr) {
        controller->setLastOffTime(millis());
    }

    Logger.Info("Mode set from Azure: " + String(getModeName()));
    executeMode();
}


TemperatureController::TemperatureController(int initialTemp, int minT, int maxT,
                                             int heatPin, int coolPin,
                                             int heatFbPin, int coolFbPin,
                                             int highFanRelayPin, int lowFanRelayPin,
                                             ModeContext* context)
    : temperatureSetting(initialTemp), currentTemperature(initialTemp),
      minTemp(minT), maxTemp(maxT),
      heatOutputPin(heatPin), coolOutputPin(coolPin),
      heatFeedbackPin(heatFbPin), coolFeedbackPin(coolFbPin),
      highFanPin(highFanRelayPin), lowFanPin(lowFanRelayPin),
      modeContext(context) {
        unsigned long now = millis();
        lastHeatOffMs = (now >= delayAfterOff) ? (now - delayAfterOff) : 0;
        lastCoolOffMs = (now >= delayAfterOff) ? (now - delayAfterOff) : 0;
    pinMode(heatOutputPin, OUTPUT);
    pinMode(coolOutputPin, OUTPUT);
    pinMode(highFanPin, OUTPUT);
    pinMode(lowFanPin,  OUTPUT);
    digitalWrite(heatOutputPin, kRelayOff);
    digitalWrite(coolOutputPin, kRelayOff);
    digitalWrite(highFanPin,    kRelayOff);
    digitalWrite(lowFanPin,     kRelayOff);
    if (heatFeedbackPin >= 0) pinMode(heatFeedbackPin, INPUT_PULLUP);
    if (coolFeedbackPin >= 0) pinMode(coolFeedbackPin, INPUT_PULLUP);
}

void TemperatureController::increaseTemp() {
    if (temperatureSetting < maxTemp) {
        temperatureSetting++;
    }
}

void TemperatureController::decreaseTemp() {
    if (temperatureSetting > minTemp) {
        temperatureSetting--;
    }
}

void TemperatureController::updateTemperatureSettingFromAzure(int newTemp) {
    temperatureSetting = newTemp;
}
void TemperatureController::updateCurrentTemperature(int currentTemp) {
    currentTemperature = currentTemp;
    unsigned long now = millis();
    int currentMode = modeContext->getCurrentModeIndex();
    const int heatOffThreshold = temperatureSetting + 1;
    const int heatOnThreshold = temperatureSetting - hysteresis;
    const int coolOffThreshold = temperatureSetting - hysteresis;
    const int coolOnThreshold = temperatureSetting + hysteresis;

    // Periodic debug: log every 10 s so you can see why a relay isn't engaging.
    static unsigned long lastDbgMs = 0;
    if (now - lastDbgMs >= 10000UL) {
        lastDbgMs = now;
        char dbg[120];
        snprintf(dbg, sizeof(dbg),
            "HVAC dbg | mode=%d temp=%d setpt=%d coolOn>%d heatOn<%d "
            "coolRelay=%d heatRelay=%d fbCool=%d fbHeat=%d",
            currentMode, currentTemp, temperatureSetting,
            coolOnThreshold, heatOnThreshold,
            (int)coolRelayState, (int)heatOutputState,
            coolFeedbackPin >= 0 ? digitalRead(coolFeedbackPin) : -1,
            heatFeedbackPin >= 0 ? digitalRead(heatFeedbackPin) : -1);
        Logger.Info(dbg);
    }

    auto setHeat = [&](bool on, bool reachedSetpoint = false) {
        if (heatOutputState == on) return;
        if (on && (now - lastHeatOffMs < delayAfterOff)) {
            static unsigned long lastAntiShortHeatLogMs = 0;
            if (now - lastAntiShortHeatLogMs >= 5000UL) {
                lastAntiShortHeatLogMs = now;
                char buf[64];
                snprintf(buf, sizeof(buf), "Heat blocked: anti-short-cycle %lus remain",
                    (delayAfterOff - (now - lastHeatOffMs)) / 1000UL);
                Logger.Info(buf);
            }
            return;
        }
        digitalWrite(heatOutputPin, on ? kRelayOn : kRelayOff);
        if (on) {
            heatRunStartMs    = now;
            heatCycleStartMs  = now;
            heatCycleStartTemp = currentTemp;
            stuckStartTempHeat = currentTemp;
            // Log feedback pin immediately so we know if the fault check can ever fire.
            if (heatFeedbackPin >= 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Heat relay ON: fbPin=%d (HIGH=fault-ok, LOW=will-not-fault)",
                    digitalRead(heatFeedbackPin));
                Logger.Info(buf);
            } else {
                Logger.Info("Heat relay ON");
            }
        } else if (heatRunStartMs != 0) {
            unsigned long dur = now - heatRunStartMs;
            heatRunMs += dur;
            heatRunStartMs = 0;
            // Avg time to setpoint (only count cycles that actually finished)
            if (reachedSetpoint && heatCycleStartMs != 0) {
                heatSetptTotalMs += (now - heatCycleStartMs);
                heatSetptCount++;
            }
            heatCycleStartMs = 0;
            // Avg ms per degree
            uint32_t degChanged = (uint32_t)abs(currentTemp - heatCycleStartTemp);
            if (degChanged > 0) {
                heatDegTotalMs += dur;
                heatDegTotal   += degChanged;
            }
        }
        if (!on) {
            lastHeatOffMs = now;
        }
        heatOutputState = on;
        if (on) { /* already logged above with fbPin */ } else { Logger.Info("Heat relay OFF"); }
    };

    auto setCool = [&](bool on, bool reachedSetpoint = false) {
        if (coolRelayState == on) return;
        if (on) {
            unsigned long coolDelay = _coolCompressorHasRun ? kCoolDelayAfterOff : delayAfterOff;
            if (now - lastCoolOffMs < coolDelay) return;
        }
        digitalWrite(coolOutputPin, on ? kRelayOn : kRelayOff);
        if (on) {
            _coolCompressorHasRun = true; // compressor now running; 3-min delay applies on next off
            coolRelayState = true;
            // Apply fan outputs for the current fan mode.
            applyFanOutputs();
            coolRunStartMs    = now;
            coolCycleStartMs  = now;
            coolCycleStartTemp = currentTemp;
            stuckStartTempCool = currentTemp;
        } else if (coolRunStartMs != 0) {
            unsigned long dur = now - coolRunStartMs;
            coolRunMs += dur;
            coolRunStartMs = 0;
            // Avg time to setpoint (only count cycles that actually finished)
            if (reachedSetpoint && coolCycleStartMs != 0) {
                coolSetptTotalMs += (now - coolCycleStartMs);
                coolSetptCount++;
            }
            coolCycleStartMs = 0;
            // Avg ms per degree
            uint32_t degChanged = (uint32_t)abs(currentTemp - coolCycleStartTemp);
            if (degChanged > 0) {
                coolDegTotalMs += dur;
                coolDegTotal   += degChanged;
            }
        }
        if (!on) {
            coolRelayState = false;
            applyFanOutputs();
            lastCoolOffMs = now;
        }
        Logger.Info(on ? (String("Cool relay ON (fan ") + fanSpeedName(_fanSpeed) + ")").c_str()
                       : "Cool relay OFF");
    };

    // Fault is latched: keep both outputs off until clearFault() is called.
    if (_faultActive) {
        setHeat(false);
        setCool(false);
        // In fault state, force blower relays off regardless of selected fan mode.
        digitalWrite(highFanPin, kRelayOff);
        digitalWrite(lowFanPin,  kRelayOff);
        return;
    }

    // Heating logic with anti-short-cycle delay and mutual exclusion.
    if (currentMode == kModeHeat) {
        setCool(false);

        if (heatOutputState && currentTemp >= heatOffThreshold) {
            setHeat(false, /*reachedSetpoint=*/true);
        } else if (!heatOutputState && currentTemp < heatOnThreshold) {
            setHeat(true);
        } else if (!heatOutputState) {
            // Relay is off and temp is not cold enough to engage — log so the user can see why.
            static unsigned long lastHeatStandbyLogMs = 0;
            if (now - lastHeatStandbyLogMs >= 10000UL) {
                lastHeatStandbyLogMs = now;
                char buf[72];
                snprintf(buf, sizeof(buf),
                    "Heat standby: temp=%d threshold=%d (need temp<%d to engage)",
                    currentTemp, heatOnThreshold, heatOnThreshold);
                Logger.Info(buf);
            }
        }
    }

    // Cooling logic with mutual exclusion.
    if (currentMode == kModeCool) {
        setHeat(false);

        if (coolRelayState && currentTemp <= coolOffThreshold) {
            setCool(false, /*reachedSetpoint=*/true);
        } else if (!coolRelayState && currentTemp > coolOnThreshold) {
            setCool(true);
        }
    }

    // Off mode always forces both relays off.
    if (currentMode == kModeOff) {
        setHeat(false);
        setCool(false);
    }

    // Relay feedback check: if a feedback pin is configured and the relay has
    // been commanded ON for 30 s without the feedback pin going LOW (closed),
    // the relay likely did not close — raise a fault immediately.
    constexpr unsigned long kNoResponseMs = 30UL * 1000UL;
    if (!_faultActive) {
        if (heatFeedbackPin >= 0 &&
            heatOutputState && heatRunStartMs != 0 &&
            (now - heatRunStartMs) >= kNoResponseMs &&
            digitalRead(heatFeedbackPin) == HIGH) {  // HIGH = not closed
            _faultActive = true;
            snprintf(_faultMsg, sizeof(_faultMsg), "Heat relay did not close");
            heatRunStartMs = 0; // relay never confirmed closed — don't count this time
            setHeat(false);
            Logger.Info("FAULT: Heat relay did not close (feedback pin HIGH after 30s)");
        }
        // Diagnostic: log why heat fault hasn't fired (runs once per relay-on cycle, after 31s)
        else if (heatFeedbackPin >= 0 && heatOutputState && heatRunStartMs != 0 &&
                 (now - heatRunStartMs) >= (kNoResponseMs + 1000UL)) {
            static unsigned long lastHeatFaultDbgMs = 0;
            if (now - lastHeatFaultDbgMs >= 10000UL) {
                lastHeatFaultDbgMs = now;
                char buf[80];
                snprintf(buf, sizeof(buf),
                    "Heat fault NOT fired: onSec=%lu fbPin=%d",
                    (now - heatRunStartMs) / 1000UL,
                    digitalRead(heatFeedbackPin));
                Logger.Info(buf);
            }
        }
        if (!_faultActive &&
            coolFeedbackPin >= 0 &&
            coolRelayState && coolRunStartMs != 0 &&
            (now - coolRunStartMs) >= kNoResponseMs &&
            digitalRead(coolFeedbackPin) == HIGH) {  // HIGH = not closed
            _faultActive = true;
            snprintf(_faultMsg, sizeof(_faultMsg), "AC relay did not close");
            coolRunStartMs = 0; // relay never confirmed closed — don't count this time
            setCool(false);
            Logger.Info("FAULT: Cool relay did not close (feedback pin HIGH after 30s)");
        }
    }

    // Stuck-system protection:
    // - Heating keeps the hard-fault behavior.
    // - Cooling does NOT hard-fault/shut down on low delta because in extreme
    //   ambient conditions (very hot days), continuous AC runtime is expected.
    constexpr unsigned long kStuckMs = 30UL * 60UL * 1000UL;
    if (!_faultActive) {
        if (heatOutputState && heatRunStartMs != 0 &&
            (now - heatRunStartMs) >= kStuckMs &&
            abs(currentTemp - stuckStartTempHeat) < 2) {
            _faultActive = true;
            snprintf(_faultMsg, sizeof(_faultMsg), "Heat on 30min, no temp change");
            setHeat(false);
            Logger.Info("FAULT: Heat stuck");
        }
        if (coolRelayState && coolRunStartMs != 0 &&
            (now - coolRunStartMs) >= kStuckMs &&
            abs(currentTemp - stuckStartTempCool) < 2) {
            static unsigned long lastCoolStallWarnMs = 0;
            if (now - lastCoolStallWarnMs >= 60000UL) {
                lastCoolStallWarnMs = now;
                Logger.Info("WARN: Cool runtime high with low temp delta; keeping AC running");
            }
        }
    }
}

int TemperatureController::getTemperatureSetting() {
    return temperatureSetting;
}

void TemperatureController::setLastOffTime(unsigned long time) {
    lastHeatOffMs = time;
    lastCoolOffMs = time;
    Logger.Info("Stop all");
    // Accumulate any in-progress runtime before forcing off.
    if (heatOutputState && heatRunStartMs != 0) {
        heatRunMs += (time - heatRunStartMs);
        heatRunStartMs = 0;
    }
    if (coolRelayState && coolRunStartMs != 0) {
        coolRunMs += (time - coolRunStartMs);
        coolRunStartMs = 0;
    }
    digitalWrite(heatOutputPin, kRelayOff);  // Ensure off
    digitalWrite(coolOutputPin, kRelayOff);
    heatOutputState = false;
    coolRelayState = false;
    applyFanOutputs();
}

// ---------------- Direct Relay Control (with software interlock) ----------------

void TemperatureController::activateHeatOutput() {
    unsigned long now = millis();

    if (_faultActive) {
        Logger.Info("Heat activation blocked: fault active");
        return;
    }

    // Software interlock: ensure cool relay is de-energised before heat turns on.
    if (coolRelayState) {
        if (coolRunStartMs != 0) {
            coolRunMs += now - coolRunStartMs;
            coolRunStartMs = 0;
        }
        digitalWrite(coolOutputPin, kRelayOff);
        coolRelayState = false;
        lastCoolOffMs = now;
        Logger.Info("Interlock: Cool OFF before Heat activation");
    }

    if (heatOutputState) return; // already on

    if (now - lastHeatOffMs < delayAfterOff) {
        char buf[72];
        snprintf(buf, sizeof(buf), "Heat activation blocked (anti-short-cycle): %lus remain",
                 (delayAfterOff - (now - lastHeatOffMs)) / 1000UL);
        Logger.Info(buf);
        return;
    }

    digitalWrite(heatOutputPin, kRelayOn);  // active-high: HIGH energises
    heatOutputState   = true;
    heatRunStartMs    = now;
    heatCycleStartMs  = now;
    heatCycleStartTemp = currentTemperature;
    stuckStartTempHeat = currentTemperature;
    Logger.Info("Heat relay ON");
}

void TemperatureController::activateCoolOutput() {
    unsigned long now = millis();

    if (_faultActive) {
        Logger.Info("Cool activation blocked: fault active");
        return;
    }

    // Software interlock: ensure heat relay is de-energised before cool turns on.
    if (heatOutputState) {
        if (heatRunStartMs != 0) {
            heatRunMs += now - heatRunStartMs;
            heatRunStartMs = 0;
        }
        digitalWrite(heatOutputPin, kRelayOff);
        heatOutputState = false;
        lastHeatOffMs = now;
        Logger.Info("Interlock: Heat OFF before Cool activation");
    }

    if (coolRelayState) return; // already on

    unsigned long coolDelay = _coolCompressorHasRun ? kCoolDelayAfterOff : delayAfterOff;
    if (now - lastCoolOffMs < coolDelay) {
        char buf[80];
        snprintf(buf, sizeof(buf), "Cool activation blocked (anti-short-cycle): %lus remain",
                 (coolDelay - (now - lastCoolOffMs)) / 1000UL);
        Logger.Info(buf);
        return;
    }

    digitalWrite(coolOutputPin, kRelayOn);  // active-high: HIGH energises
    _coolCompressorHasRun = true;           // compressor now running; 3-min delay applies on next off
    coolRelayState    = true;
    // Apply fan outputs for the current fan mode.
    applyFanOutputs();
    coolRunStartMs    = now;
    coolCycleStartMs  = now;
    coolCycleStartTemp = currentTemperature;
    stuckStartTempCool = currentTemperature;
    Logger.Info((String("Cool relay ON (fan ") + fanSpeedName(_fanSpeed) + ")").c_str());
}

void TemperatureController::deactivateAllOutputs() {
    unsigned long now = millis();
    bool anyWasOn = false;

    if (heatOutputState) {
        if (heatRunStartMs != 0) {
            heatRunMs += now - heatRunStartMs;
            heatRunStartMs = 0;
        }
        digitalWrite(heatOutputPin, kRelayOff);
        heatOutputState = false;
        lastHeatOffMs = now;
        anyWasOn = true;
    }
    if (coolRelayState) {
        if (coolRunStartMs != 0) {
            coolRunMs += now - coolRunStartMs;
            coolRunStartMs = 0;
        }
        digitalWrite(coolOutputPin, kRelayOff);
        coolRelayState = false;
        lastCoolOffMs = now;
        anyWasOn = true;
    }
    applyFanOutputs();
    if (anyWasOn) Logger.Info("All outputs deactivated");
}

void TemperatureController::applyFanOutputs() {
    if (_faultActive) {
        digitalWrite(highFanPin, kRelayOff);
        digitalWrite(lowFanPin,  kRelayOff);
        return;
    }

    switch (_fanSpeed) {
        case FanSpeed::High:
            digitalWrite(highFanPin, kRelayOn);
            digitalWrite(lowFanPin,  kRelayOff);
            break;
        case FanSpeed::Low:
            digitalWrite(highFanPin, kRelayOff);
            digitalWrite(lowFanPin,  kRelayOn);
            break;
        case FanSpeed::Auto:
        default:
            // Auto: fan runs high only while AC is actively running.
            digitalWrite(highFanPin, coolRelayState ? kRelayOn : kRelayOff);
            digitalWrite(lowFanPin,  kRelayOff);
            break;
    }
}

void TemperatureController::setFanSpeed(FanSpeed speed) {
    _fanSpeed = speed;
    applyFanOutputs();
    Logger.Info((String("Fan mode set ") + fanSpeedName(_fanSpeed)).c_str());
}

unsigned long TemperatureController::getCoolDelayRemaining() const {
    if (!_coolCompressorHasRun) return 0; // compressor has never run; delay never applies
    if (coolRelayState) return 0;          // compressor is currently running
    unsigned long now = millis();
    unsigned long elapsed = now - lastCoolOffMs;
    if (elapsed >= kCoolDelayAfterOff) return 0;
    return kCoolDelayAfterOff - elapsed;
}

// ---------------- Utility Functions Implementation ----------------

bool readButton(int pin) {
    static int lastState = HIGH;
    int reading = digitalRead(pin);
    if (reading == LOW && lastState == HIGH) {  
        delay(1);  //debounce
        lastState = LOW;
        return true;
    } else if (reading == HIGH) {
        lastState = HIGH;
    }
    return false;
}
