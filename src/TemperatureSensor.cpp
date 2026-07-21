#include "TemperatureSensor.h"
#include <Arduino.h>

// ✅ Constructor: Initializes OneWire with specified pin
TemperatureSensor::TemperatureSensor(int pin) : ds(pin) {}

// ✅ Initialize the sensor and check if valid
bool TemperatureSensor::begin() {
    if (!ds.search(addr)) {
        Serial.println("No DS18B20 sensor found!");
        ds.reset_search();
        return false;
    }

    Serial.print("Found ROM: ");
    for (byte i = 0; i < 8; i++) {
        Serial.print(addr[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (OneWire::crc8(addr, 7) != addr[7]) {
        Serial.println("CRC check failed!");
        return false;
    }

    switch (addr[0]) {
        case 0x10:
            Serial.println("  Chip = DS18S20");
            type_s = 1;
            break;
        case 0x28:
            Serial.println("  Chip = DS18B20");
            type_s = 0;
            break;
        case 0x22:
            Serial.println("  Chip = DS1822");
            type_s = 0;
            break;
        default:
            Serial.println("Unknown DS18x20 sensor type.");
            return false;
    }
    
    return true;
}

// ✅ Read temperature in Celsius
float TemperatureSensor::getTemperatureC() {
    byte data[9];
    
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);  // Start temperature conversion

    delay(750); // Wait for conversion to complete

    ds.reset();
    ds.select(addr);
    ds.write(0xBE);  // Read Scratchpad

    for (byte i = 0; i < 9; i++) {
        data[i] = ds.read();
    }

    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
        raw = raw << 3; // 9-bit resolution default
        if (data[7] == 0x10) {
            raw = (raw & 0xFFF0) + 12 - data[6];
        }
    } else {
        byte cfg = (data[4] & 0x60);
        if (cfg == 0x00) raw &= ~7;  // 9-bit resolution
        else if (cfg == 0x20) raw &= ~3;  // 10-bit
        else if (cfg == 0x40) raw &= ~1;  // 11-bit
    }

    return (float)raw / 16.0;  // Convert to Celsius
}

// ✅ Convert temperature to Fahrenheit
float TemperatureSensor::getTemperatureF() {
    return getTemperatureC() * 1.8 + 32.0;
}
