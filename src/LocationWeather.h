#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <WiFiClientSecure.h>

class LocationWeather {
public:
    LocationWeather(const char* weatherHost, const char* weatherKey);
    ~LocationWeather();

    bool update(float latitude, float longitude);

    float temperature;
    float feelsLike;
    float tempMin;
    float tempMax;
    int   humidity;
    int   pressure;
    float windSpeed;

    char description[32];
    char sunriseTime[8];
    char sunsetTime[8];

private:
    void formatTime(long unixTime, char* buffer);

    const char* weatherHost;
    const char* weatherKey;

    WiFiClientSecure wifiClient;
    HttpClient* weatherClient;
};