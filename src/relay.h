#include <Arduino.h>
#include <deque>
#include <vector>
#include <numeric>
#include <string>
#include <ESP8266WebServer.h>
#include "inverter.h"

#ifndef ESP_SOLAR_BOY_RELAYMANAGER_H
#define ESP_SOLAR_BOY_RELAYMANAGER_H

class Relay {
public:
    Relay(uint8_t pin);

    ~Relay() = default;

    void update();

private:
    uint8_t _pin;
    bool _isOn = false;
    uint64_t _lastEnableTime = 0;
    uint64_t _cycleTime = 60000;
    uint64_t _monitoringWindow = 0;
};

class RelayManager {
public:

    void update();

    void addRelay(Relay &r);

    void setup(Inverter *inverter, ESP8266WebServer &webServer);

private:
    const uint64_t _maxBufferLength = 30;
    const uint64_t _bufferUpdateInterval = 10000;
    uint64_t _lastBufferUpdate = 0;
    Inverter *_inverter;
    std::deque<uint16_t> _batteryStateOfCharge;
    std::deque<int32_t> _powerMeterActivePower;
    std::vector<Relay> _relays;

    uint16_t _getMeanBatteryStateOfCharge();

    int32_t _getMeanPowerMeterActivePower();

    void _addBatteryValue(uint16_t value);

    void _addActivePowerValue(int32_t value);

    void _printy();
};

#endif //ESP_SOLAR_BOY_RELAYMANAGER_H
