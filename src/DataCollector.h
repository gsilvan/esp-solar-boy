#ifndef ESP_SOLAR_BOY_DATACOLLECTOR_H
#define ESP_SOLAR_BOY_DATACOLLECTOR_H

#include "Inverter.h"
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>


class DataCollector {
public:
    DataCollector();

    ~DataCollector() = default;

    void setup(Inverter *inverter, const String &deviceId, uint64_t updateInterval = 60000);

    void update();

    void setIsEnabled(bool value);

    void setUrl(const String &url);

    bool isEnabled = false;
    String url = "";

private:
    Preferences _preferences;
    Inverter *_inverter = nullptr;
    WiFiClient wifiClient;
    HTTPClient httpClient;
    String _deviceId = "";
    uint64_t _updateInterval = 0;
    uint64_t _lastUpdate = 0;

    JsonDocument generateJson();
};


#endif //ESP_SOLAR_BOY_DATACOLLECTOR_H
