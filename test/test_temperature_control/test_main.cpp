#include <Arduino.h>
#include <unity.h>

// Compile the production sources directly into this test binary.
#include "../../src/SerialLogger.cpp"
#include "../../src/TemperatureControlSystem.cpp"

static ModeContext modeContext(nullptr);
static TemperatureController controller(70, 50, 90, 3, 13, &modeContext);

void setUp(void) {
    modeContext.setTempController(&controller);
    modeContext.setModeFromAzure("OFF");
    controller.setLastOffTime(millis());
    controller.setHeatRunMs(0);
    controller.setCoolRunMs(0);
    controller.updateTemperatureSettingFromAzure(70);
    controller.clearFault();
}

void tearDown(void) {
}

void test_mode_context_cycles_off_heat_cool_off(void) {
    TEST_ASSERT_EQUAL_STRING("Off", modeContext.getModeName());

    modeContext.changeMode();
    TEST_ASSERT_EQUAL_STRING("Heat", modeContext.getModeName());

    modeContext.changeMode();
    TEST_ASSERT_EQUAL_STRING("Cool", modeContext.getModeName());

    modeContext.changeMode();
    TEST_ASSERT_EQUAL_STRING("Off", modeContext.getModeName());
}

void test_set_mode_from_azure_is_case_insensitive_and_validated(void) {
    modeContext.setModeFromAzure("heat");
    TEST_ASSERT_EQUAL_STRING("Heat", modeContext.getModeName());

    modeContext.setModeFromAzure("COOL");
    TEST_ASSERT_EQUAL_STRING("Cool", modeContext.getModeName());

    modeContext.setModeFromAzure("invalid-mode");
    TEST_ASSERT_EQUAL_STRING("Cool", modeContext.getModeName());
}

void test_setpoint_stays_within_bounds(void) {
    controller.updateTemperatureSettingFromAzure(70);

    for (int i = 0; i < 40; ++i) {
        controller.increaseTemp();
    }
    TEST_ASSERT_EQUAL_INT(90, controller.getTemperatureSetting());

    for (int i = 0; i < 80; ++i) {
        controller.decreaseTemp();
    }
    TEST_ASSERT_EQUAL_INT(50, controller.getTemperatureSetting());
}

void test_heat_runtime_accumulates_when_heat_runs(void) {
    modeContext.setModeFromAzure("HEAT");
    controller.updateTemperatureSettingFromAzure(70);

    // Allow immediate heat turn-on by simulating enough off time.
    controller.setLastOffTime(millis() - 61000UL);
    controller.updateCurrentTemperature(66);
    delay(25);
    controller.setLastOffTime(millis());

    TEST_ASSERT_GREATER_THAN_UINT32(0, controller.getHeatRunMs());
}

void test_cool_runtime_accumulates_when_cool_runs(void) {
    modeContext.setModeFromAzure("COOL");
    controller.updateTemperatureSettingFromAzure(70);

    controller.updateCurrentTemperature(75);
    delay(25);
    controller.setLastOffTime(millis());

    TEST_ASSERT_GREATER_THAN_UINT32(0, controller.getCoolRunMs());
}

void test_runtime_setters_roundtrip(void) {
    controller.setHeatRunMs(1234);
    controller.setCoolRunMs(5678);

    TEST_ASSERT_EQUAL_UINT32(1234, controller.getHeatRunMs());
    TEST_ASSERT_EQUAL_UINT32(5678, controller.getCoolRunMs());
}

void setup() {
    delay(2000);
    UNITY_BEGIN();

    RUN_TEST(test_mode_context_cycles_off_heat_cool_off);
    RUN_TEST(test_set_mode_from_azure_is_case_insensitive_and_validated);
    RUN_TEST(test_setpoint_stays_within_bounds);
    RUN_TEST(test_heat_runtime_accumulates_when_heat_runs);
    RUN_TEST(test_cool_runtime_accumulates_when_cool_runs);
    RUN_TEST(test_runtime_setters_roundtrip);

    UNITY_END();
}

void loop() {
}
