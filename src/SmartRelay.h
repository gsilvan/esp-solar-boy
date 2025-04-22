#ifndef ESP_SOLAR_BOY_SMARTRELAY_H
#define ESP_SOLAR_BOY_SMARTRELAY_H
#include <Arduino.h>
#include <Preferences.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "Inverter.h"
#include "template.h"

#define PIN_ENABLED_SETTING "pinEnabled"
#define PIN_ALWAYS_ON_SETTING "pinAlwaysOn"
#define MIN_BATTERY_SETTING "minBattery"
#define MIN_PLANT_POWER_SETTING "minPlantPower"
#define TURN_ON_ACTIVE_POWER_SETTING "onActivePower"
#define MONITOR_WINDOW_SETTING "monWindow"
#define SWITCH_CYCLE_SETTING "switchCycle"
#define NAME_SETTING "name"

class SmartRelay {
public:
    explicit SmartRelay(uint8_t pin);

    ~SmartRelay() = default;

    void setup(Inverter *inverter, AsyncWebServer *httpServer);

    void update();

    void setIsPinEnabledSetting(bool value);

    void setIsPinAlwaysOnSetting(bool value);

    void setMinBatteryChargeSetting(uint16_t value);

    void setMinPlantPowerSetting(uint32_t value);

    void setTurnOnPowerMeterActivePowerSetting(int32_t value);

    void setMonitoringWindowMinutesSetting(uint8_t value);

    void setSwitchCycleMinutesSetting(uint8_t value);

    void setName(String value);

    const uint64_t UPDATE_INTERVAL = 1000;
    bool isPinOn = false;
    bool isPinAlwaysOnSetting = false;
    bool isPinEnabledSetting = false;
    uint16_t minBatteryChargeSetting = 95;
    uint32_t minPlantPowerSetting = 0;
    int32_t turnOnPowerMeterActivePowerSetting = 300;
    uint8_t monitoringWindowMinutesSetting = 1;
    uint8_t switchCycleMinutesSetting = 1;
    String name;
private:
    uint8_t _pin;
    Inverter *_inverter;
    AsyncWebServer *_httpServer;
    uint64_t _lastEnableTime = 0;
    uint64_t _lastUpdateTime = 0;
    Preferences _preferences;
    String _preferencesNamespace;
    String _settingsRoute;
    String _indicatorRoute;
    String _labelRoute;

    uint64_t _switchCycleMillis() const;

    void _setPinOn();

    void _setPinOff();

    String _generateSettingsRoute();

    String _generateIndicatorRoute();

    String _generateLabelRoute();

    String _generateHTML();

    String _generateIndicatorHTML();

    void _registerHttpRoutes();
};


#endif //ESP_SOLAR_BOY_SMARTRELAY_H
