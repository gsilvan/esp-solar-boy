#ifndef ESP_SOLAR_BOY_SMARTRELAY_H
#define ESP_SOLAR_BOY_SMARTRELAY_H
#include <Arduino.h>
#include <Preferences.h>
#include "inverter.h"

#define MIN_BATTERY_SETTING "minBattery"
#define MIN_POWER_SETTING "minPower"
#define MONITOR_WINDOW_SETTING "monWindow"
#define SWITCH_CYCLE_SETTING "switchCycle"

class SmartRelay {
public:
    explicit SmartRelay(uint8_t pin);

    ~SmartRelay() = default;

    void registerInverter(Inverter *inverter);

    void update();

    void setMinBatteryChargeSetting(uint16_t value);

    void setMinPowerMeterActivePowerSetting(uint32_t value);

    void setMonitoringWindowMinutesSetting(uint8_t value);

    void setSwitchCycleMinutesSetting(uint8_t value);

    const uint64_t UPDATE_INTERVAL = 1000;
    bool isPinOn = false;
    bool isPinAlwaysOn = false;
    bool isPinEnabledSetting = false;
    uint16_t minBatteryChargeSetting = 95;
    uint32_t minPowerMeterActivePowerSetting = 500;
    uint8_t monitoringWindowMinutesSetting = 1;
    uint8_t switchCycleMinutesSetting = 1;
private:
    uint8_t _pin;
    Inverter *_inverter;
    uint64_t _lastEnableTime = 0;
    uint64_t _lastUpdateTime = 0;
    Preferences _preferences;
    String _preferencesNamespace;

    uint64_t _switchCycleMillis() const;

    void _setPinOn();

    void _setPinOff();
};


#endif //ESP_SOLAR_BOY_SMARTRELAY_H
