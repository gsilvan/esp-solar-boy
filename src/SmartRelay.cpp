#include "SmartRelay.h"

void SmartRelay::begin(uint8_t pin, Inverter *inverter) {
    this->_pin = pin;
    this->_inverter = inverter;
    pinMode(this->_pin, OUTPUT);
    digitalWrite(this->_pin, LOW);
}

void SmartRelay::update() {
    if (this->isPinOn && (millis() - this->_lastEnableTime < this->_switchCycleMillis())) {
        // If relay is on and our switch cycle is running we do nothing in this update loop
        return;
    }
    if (this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge() >= this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower() >= 100)) {
            this->_setPinOn();
            return;
        }
    } else {
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
}

void SmartRelay::_setPinOff() {
    this->isPinOn = false;
    digitalWrite(this->_pin, LOW);
}
