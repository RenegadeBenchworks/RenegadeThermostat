#include "LocationWeather.h"

LocationWeather::LocationWeather(const char* host, const char* key)
    : weatherHost(host), weatherKey(key),
      temperature(0), feelsLike(0),
      tempMin(0), tempMax(0),
      humidity(0), pressure(0), windSpeed(0)
{
    description[0] = '\0';
    sunriseTime[0] = '\0';
    sunsetTime[0]  = '\0';

    weatherClient = new HttpClient(wifiClient, weatherHost, 443);
}

LocationWeather::~LocationWeather() {
    delete weatherClient;
}

bool LocationWeather::update(float latitude, float longitude) {
    wifiClient.setInsecure();
    // Keep blocking network calls shorter than the task-WDT window.
    wifiClient.setTimeout(8000);
    char path[256];

    snprintf(path, sizeof(path),
        "/data/3.0/onecall?lat=%.6f&lon=%.6f"
        "&exclude=minutely,hourly,alerts"
        "&units=imperial"
        "&appid=%s",
        latitude, longitude, weatherKey);

    weatherClient->beginRequest();
    weatherClient->get(path);
    weatherClient->endRequest();

    int statusCode = weatherClient->responseStatusCode();
   
    if (statusCode != 200) {
        Serial.print("Weather HTTP error: ");
        Serial.println(statusCode);
        return false;
    }

    // JSON FILTER (huge RAM saver)
    StaticJsonDocument<256> filter;
    filter["current"]["temp"] = true;
    filter["current"]["feels_like"] = true;
    filter["current"]["humidity"] = true;
    filter["current"]["pressure"] = true;
    filter["current"]["wind_speed"] = true;
    filter["current"]["weather"][0]["description"] = true;
    filter["current"]["sunrise"] = true;
    filter["current"]["sunset"] = true;
    filter["timezone_offset"] = true;
    filter["daily"][0]["temp"]["min"] = true;
    filter["daily"][0]["temp"]["max"] = true;

    StaticJsonDocument<768> doc;

    DeserializationError error =
        deserializeJson(doc, weatherClient->responseBody(),
                        DeserializationOption::Filter(filter));
 
    if (error) {
        Serial.print("JSON error: ");
        Serial.println(error.c_str());
        return false;
    }

    // Current conditions
    temperature = doc["current"]["temp"] | 0.0;
    feelsLike   = doc["current"]["feels_like"] | 0.0;
    humidity    = doc["current"]["humidity"] | 0;
    pressure    = doc["current"]["pressure"] | 0;
    windSpeed   = doc["current"]["wind_speed"] | 0.0;

    const char* desc = doc["current"]["weather"][0]["description"] | "";
    strncpy(description, desc, sizeof(description) - 1);
    description[sizeof(description) - 1] = '\0';

    // Daily forecast (today)
    tempMin = doc["daily"][0]["temp"]["min"] | 0.0;
    tempMax = doc["daily"][0]["temp"]["max"] | 0.0;

    // Sunrise / Sunset
    long sunriseUTC = doc["current"]["sunrise"] | 0;
    long sunsetUTC  = doc["current"]["sunset"]  | 0;
    long offset     = doc["timezone_offset"]    | 0;

    sunriseUTC += offset;
    sunsetUTC  += offset;

    formatTime(sunriseUTC, sunriseTime);
    formatTime(sunsetUTC,  sunsetTime);

    return true;
}

void LocationWeather::formatTime(long unixTime, char* buffer) {

    int hour   = (unixTime % 86400L) / 3600L;
    int minute = (unixTime % 3600L) / 60L;

    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;

    snprintf(buffer, 8, "%d:%02d", hour, minute);
}