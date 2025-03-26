#include "SmartRelay.h"

void SmartRelay::begin(uint8_t pin, Inverter *inverter) {
    this->_pin = pin;
    this->_inverter = inverter;
    pinMode(this->_pin, OUTPUT);
    digitalWrite(this->_pin, LOW);
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
        if ((this->_inverter->minBatteryStateOfCharge() >= this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower() >= 100)) {
            this->_setPinOn();
            return;
        }
    }
    if (!this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge() >= this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower() >= this->minPowerMeterActivePowerSetting)) {
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
