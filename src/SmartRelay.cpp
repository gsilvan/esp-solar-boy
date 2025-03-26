#include "SmartRelay.h"

SmartRelay::SmartRelay(uint8_t pin) {
    this->_pin = pin;
    pinMode(this->_pin, OUTPUT);
    digitalWrite(this->_pin, LOW);
    this->_inverter = nullptr;
    this->_preferencesNamespace = String("smart-relay") + String(this->_pin);

    _preferences.begin(_preferencesNamespace.c_str(), false);

    minBatteryChargeSetting = _preferences.getUShort(MIN_BATTERY_SETTING, 90);
    minPowerMeterActivePowerSetting = _preferences.getULong(MIN_POWER_SETTING, 300);
    monitoringWindowMinutesSetting = _preferences.getUChar(MONITOR_WINDOW_SETTING, 1);
    switchCycleMinutesSetting = _preferences.getUChar(SWITCH_CYCLE_SETTING, 1);
}

void SmartRelay::setup(Inverter *inverter) {
    this->_inverter = inverter;
}

void SmartRelay::update() {
    if (millis() - this->_lastUpdateTime < this->UPDATE_INTERVAL) {
        return;
    }
    if (this->isPinAlwaysOn) {
        this->_setPinOn();
        return;
    }
    if (!this->isPinEnabledSetting) {
        this->_setPinOff();
        return;
    }
    if (!this->_inverter->isConnected) {
        this->_setPinOff();
        return;
    }
    if (this->isPinOn && (millis() - this->_lastEnableTime < this->_switchCycleMillis())) {
        // If relay is on and our switch cycle is running we do nothing in this update loop
        this->_lastUpdateTime = millis();
        return;
    }
    if (this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= 100)) {
            this->_setPinOn();
            return;
        }
    }
    if (!this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= (int) this->minPowerMeterActivePowerSetting)) {
            this->_setPinOn();
            return;
        }
    }
    this->_setPinOff();
}

uint64_t SmartRelay::_switchCycleMillis() const {
    return this->switchCycleMinutesSetting * 60 * 1000;
}

void SmartRelay::_setPinOn() {
    this->isPinOn = true;
    digitalWrite(this->_pin, HIGH);
    this->_lastEnableTime = millis();
    this->_lastUpdateTime = millis();
    Serial.println("Set pin ON!");
}

void SmartRelay::_setPinOff() {
    this->isPinOn = false;
    digitalWrite(this->_pin, LOW);
    this->_lastEnableTime = 0;
    this->_lastUpdateTime = millis();
    Serial.println("Set pin OFF!");
}

void SmartRelay::setMinBatteryChargeSetting(uint16_t value) {
    this->minBatteryChargeSetting = value;
    this->_preferences.putUShort(MIN_BATTERY_SETTING, value);
}

void SmartRelay::setMinPowerMeterActivePowerSetting(uint32_t value) {
    this->minPowerMeterActivePowerSetting = value;
    this->_preferences.putUInt(MIN_POWER_SETTING, value);
}

void SmartRelay::setMonitoringWindowMinutesSetting(uint8_t value) {
    this->monitoringWindowMinutesSetting = value;
    this->_preferences.putUChar(MONITOR_WINDOW_SETTING, value);
}

void SmartRelay::setSwitchCycleMinutesSetting(uint8_t value) {
    this->switchCycleMinutesSetting = value;
    this->_preferences.putUChar(SWITCH_CYCLE_SETTING, value);
}
