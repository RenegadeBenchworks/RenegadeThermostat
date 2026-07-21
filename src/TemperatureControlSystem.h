#ifndef TEMPERATURE_CONTROL_SYSTEM_H
#define TEMPERATURE_CONTROL_SYSTEM_H

#include <Arduino.h>
#include "AppState.h"

class TemperatureController;

// Abstract base class for all modes
class ModeStrategy {
public:
    virtual void execute(TemperatureController* ctrl) = 0;
    virtual ~ModeStrategy() {}  
};

// Off mode strategy
class OffMode : public ModeStrategy {
public:
    void execute(TemperatureController* ctrl) override;
};

// Heating mode strategy
class HeatingMode : public ModeStrategy {
public:
    void execute(TemperatureController* ctrl) override;
};

// Cooling mode strategy
class CoolingMode : public ModeStrategy {
public:
    void execute(TemperatureController* ctrl) override;
};

// Fan-only mode strategy
class FanMode : public ModeStrategy {
public:
    void execute(TemperatureController* ctrl) override;
};

// Context for mode switching
class ModeContext {
private:
    ModeStrategy* mode;
    int modeIndex = 0;
    ModeStrategy* modes[4];
    TemperatureController* controller;
   
public:
    ModeContext(TemperatureController* ctrl);
    ~ModeContext();
    void changeMode();
    void executeMode();
    const char* getModeName();
    void setModeFromAzure(const String& mode);
    int getCurrentModeIndex() const { return modeIndex; }
    void setTempController(TemperatureController* tempCtrl);
};

// Temperature Controller
class TemperatureController {
private:
    int temperatureSetting;
    int currentTemperature;
    const int minTemp;
    const int maxTemp;
    const int hysteresis = 2; // 2F band around setpoint for on/off decisions.
    bool heatOutputState = false;
    bool coolRelayState = false;
    // true once the cool relay has been energised at least once this session;
    // controls whether the 3-min pressure-equalization delay applies.
    bool _coolCompressorHasRun = false;
    // Auto: fan follows AC (high when cooling is active). High/Low: fan forced on.
    FanSpeed _fanSpeed = FanSpeed::Auto;
    unsigned long lastHeatOffMs = 0;
    unsigned long lastCoolOffMs = 0;
    const unsigned long delayAfterOff = 20000; // Minimum OFF time before a relay can re-enable.
    int heatOutputPin;
    int coolOutputPin;
    int heatFeedbackPin; // INPUT_PULLUP, reads LOW when relay is confirmed closed; -1 = no feedback
    int coolFeedbackPin;
    int highFanPin;  // A3 — energise for high fan speed
    int lowFanPin;   // A4 — energise for low  fan speed
    ModeContext* modeContext;
    void applyFanOutputs();

    // Runtime accumulators (milliseconds)
    unsigned long heatRunMs = 0;
    unsigned long coolRunMs = 0;
    unsigned long heatRunStartMs = 0; // 0 = not currently running
    unsigned long coolRunStartMs = 0;

    // Avg time to reach setpoint (only completed, threshold-triggered cycles)
    unsigned long heatCycleStartMs = 0;
    unsigned long coolCycleStartMs = 0;
    unsigned long heatSetptTotalMs = 0;
    unsigned long coolSetptTotalMs = 0;
    uint32_t      heatSetptCount   = 0;
    uint32_t      coolSetptCount   = 0;

    // Avg time per 1°F change (accumulated across all cycles)
    int           heatCycleStartTemp = 0;
    int           coolCycleStartTemp = 0;
    unsigned long heatDegTotalMs  = 0;
    unsigned long coolDegTotalMs  = 0;
    uint32_t      heatDegTotal    = 0; // total integer degrees changed
    uint32_t      coolDegTotal    = 0;

    // Stuck-system detection
    int stuckStartTempHeat = 0;
    int stuckStartTempCool = 0;

    // Fault state
    bool _faultActive = false;
    char _faultMsg[64] = {};

public:
    TemperatureController(int initialTemp, int minT, int maxT, int heatPin, int coolPin,
                          int heatFbPin, int coolFbPin,
                          int highFanPin, int lowFanPin,
                          ModeContext* context);
    void increaseTemp();
    void decreaseTemp();
    void updateCurrentTemperature(int currentTemp);
    void updateTemperatureSettingFromAzure(int newTemp);
    int getTemperatureSetting();
    void setLastOffTime(unsigned long time);
    bool isHeatOutputOn() const { return heatOutputState; }
    bool isCoolOutputOn() const { return coolRelayState; }
    // Returns true only when the relay is commanded ON and feedback confirms it closed.
    // Falls back to commanded state when no feedback pin is configured.
    bool isHeatRelayConfirmed() const {
        if (!heatOutputState) return false;
        if (heatFeedbackPin < 0) return heatOutputState;
        return digitalRead(heatFeedbackPin) == LOW;
    }
    bool isCoolRelayConfirmed() const {
        if (!coolRelayState) return false;
        if (coolFeedbackPin < 0) return coolRelayState;
        return digitalRead(coolFeedbackPin) == LOW;
    }

    // Runtime tracking
    unsigned long getHeatRunMs() const {
        return heatRunMs + (heatOutputState && heatRunStartMs ? millis() - heatRunStartMs : 0);
    }
    unsigned long getCoolRunMs() const {
        return coolRunMs + (coolRelayState && coolRunStartMs ? millis() - coolRunStartMs : 0);
    }
    void setHeatRunMs(unsigned long ms) { heatRunMs = ms; }
    void setCoolRunMs(unsigned long ms) { coolRunMs = ms; }

    // Full meter reset (zeroes accumulator; if relay is currently on, restarts
    // the in-flight timer from now so getHeatRunMs()/getCoolRunMs() read 0).
    void resetHeatRunMs() {
        heatRunMs = 0;
        if (heatOutputState) heatRunStartMs = millis();
    }
    void resetCoolRunMs() {
        coolRunMs = 0;
        if (coolRelayState) coolRunStartMs = millis();
    }

    // Avg time to reach setpoint (ms per completed cycle; 0 if no data yet).
    unsigned long getAvgHeatSetptMs() const {
        return heatSetptCount ? heatSetptTotalMs / heatSetptCount : 0UL;
    }
    unsigned long getAvgCoolSetptMs() const {
        return coolSetptCount ? coolSetptTotalMs / coolSetptCount : 0UL;
    }

    // Avg time (ms) per 1°F of temperature change; 0 if no data yet.
    unsigned long getAvgHeatMsPerDeg() const {
        return heatDegTotal ? heatDegTotalMs / heatDegTotal : 0UL;
    }
    unsigned long getAvgCoolMsPerDeg() const {
        return coolDegTotal ? coolDegTotalMs / coolDegTotal : 0UL;
    }

    // Raw accessors for NVS persistence (store/restore totals in seconds).
    unsigned long getHeatSetptTotalMs() const { return heatSetptTotalMs; }
    uint32_t      getHeatSetptCount()   const { return heatSetptCount; }
    unsigned long getCoolSetptTotalMs() const { return coolSetptTotalMs; }
    uint32_t      getCoolSetptCount()   const { return coolSetptCount; }
    unsigned long getHeatDegTotalMs()   const { return heatDegTotalMs; }
    uint32_t      getHeatDegTotal()     const { return heatDegTotal; }
    unsigned long getCoolDegTotalMs()   const { return coolDegTotalMs; }
    uint32_t      getCoolDegTotal()     const { return coolDegTotal; }

    void setHeatSetptStats(unsigned long totalMs, uint32_t count) {
        heatSetptTotalMs = totalMs; heatSetptCount = count;
    }
    void setCoolSetptStats(unsigned long totalMs, uint32_t count) {
        coolSetptTotalMs = totalMs; coolSetptCount = count;
    }
    void setHeatDegStats(unsigned long totalMs, uint32_t totalDeg) {
        heatDegTotalMs = totalMs; heatDegTotal = totalDeg;
    }
    void setCoolDegStats(unsigned long totalMs, uint32_t totalDeg) {
        coolDegTotalMs = totalMs; coolDegTotal = totalDeg;
    }

    // Reset stat accumulators (called together with resetHeat/CoolRunMs on
    // the about-page "Hold to Reset" action).
    void resetHeatStats() {
        heatSetptTotalMs = 0; heatSetptCount = 0;
        heatDegTotalMs   = 0; heatDegTotal   = 0;
    }
    void resetCoolStats() {
        coolSetptTotalMs = 0; coolSetptCount = 0;
        coolDegTotalMs   = 0; coolDegTotal   = 0;
    }

    // Direct relay control — called by mode execute() methods.
    // activateHeatOutput / activateCoolOutput include the software interlock
    // (opposite relay is de-energised first) and respect the anti-short-cycle
    // delay.  deactivateAllOutputs() immediately turns both relays off.
    void activateHeatOutput();
    void activateCoolOutput();
    void deactivateAllOutputs();
    // Update fan mode; applies highFanPin/lowFanPin immediately.
    void setFanSpeed(FanSpeed speed);
    // Returns ms remaining in the AC anti-short-cycle delay, or 0 if not active.
    unsigned long getCoolDelayRemaining() const;

    // Fault access
    bool hasFault() const { return _faultActive; }
    const char* getFaultMsg() const { return _faultMsg; }
    void clearFault() { _faultActive = false; _faultMsg[0] = '\0'; }
  
};

// Utility Functions
bool readButton(int pin);

#endif  // TEMPERATURE_CONTROL_SYSTEM_H