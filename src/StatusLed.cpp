#include "StatusLed.h"

StatusLed::StatusLed(uint8_t pin) : _ledPin(pin) {
    pinMode(this->_ledPin, OUTPUT);
    digitalWrite(this->_ledPin, LOW);
}

void StatusLed::attachInverter(Inverter *inverter) {
    this->_inverter = inverter;
}

void StatusLed::update() {
    if (!this->_inverter) {
        return;
    }
    if (!this->_inverter->isConnected) {
        this->_turnOn();
        return;
    }
    this->_turnOff();
}

void StatusLed::_turnOn() {
    digitalWrite(this->_ledPin, HIGH);
    this->_isOn = true;
}

void StatusLed::_turnOff() {
    digitalWrite(this->_ledPin, LOW);
    this->_isOn = false;
}
