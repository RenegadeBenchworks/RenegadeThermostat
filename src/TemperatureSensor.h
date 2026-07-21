#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <OneWire.h>

class TemperatureSensor {
private:
    OneWire ds;   // OneWire instance for communication
    byte addr[8]; // Sensor address
    byte type_s;  // Sensor type (DS18B20, DS18S20, etc.)

public:
    // Constructor: Accepts pin number
    TemperatureSensor(int pin);

    // Methods
    bool begin();  // Initialize and find the sensor
    float getTemperatureC();  // Get temperature in Celsius
    float getTemperatureF();  // Get temperature in Fahrenheit
};

#endif
