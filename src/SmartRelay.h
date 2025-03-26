#include <Arduino.h>
#include "inverter.h"

#ifndef ESP_SOLAR_BOY_SMARTRELAY_H
#define ESP_SOLAR_BOY_SMARTRELAY_H


class SmartRelay {
public:
    SmartRelay() = default;

    ~SmartRelay() = default;

    void begin(uint8_t pin, Inverter *inverter);

    void update();

    bool isPinOn = false;
    bool isPinEnabledSetting = false;
    uint16_t minBatteryChargeSetting = 95;
    uint32_t minPowerMeterActivePowerSetting = 500;
    uint8_t monitoringWindowMinutesSetting = 1;
    uint8_t switchCycleMinutesSetting = 1;
private:
    uint8_t _pin;
    Inverter *_inverter;
    uint64_t _lastEnableTime = 0;

    uint64_t _switchCycleMillis() const;

    void _setPinOn();

    void _setPinOff();
};


#endif //ESP_SOLAR_BOY_SMARTRELAY_H
