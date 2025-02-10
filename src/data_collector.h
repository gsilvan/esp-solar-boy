//
// Created by silvan on 10.02.25.
//
#include "inverter.h"
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#ifndef ESP_SOLAR_BOY_DATA_COLLECTOR_H
#define ESP_SOLAR_BOY_DATA_COLLECTOR_H


class DataCollector {
public:
    DataCollector();

    ~DataCollector() = default;

    void
    setup(Inverter *inverter, String url, String deviceId, uint64_t updateInterval = 60000);

    void loop();

    String url = "";

private:
    Inverter *_inverter = nullptr;
    WiFiClient wifiClient;
    HTTPClient httpClient;
    String _deviceId = "";
    uint64_t _updateInterval = 0;
    uint64_t _lastUpdate = 0;

    JsonDocument generateJson();
};


#endif //ESP_SOLAR_BOY_DATA_COLLECTOR_H
