#include "Sun2000Inverter.h"

Sun2000Inverter::Sun2000Inverter() = default;

Sun2000Inverter::~Sun2000Inverter() = default;

void Sun2000Inverter::begin(IPAddress ipAddress, uint16_t port) {
    this->ipAddress = ipAddress;
    this->port = port;
    this->modbus.client();
}

void Sun2000Inverter::update() {
    if (millis() - this->lastUpdate > this->updateInterval) {
        Serial.println("Update this boii!!!!");
        this->updateBatteryCharge();
        this->lastUpdate = millis();
    }
}

uint8_t Sun2000Inverter::getBatteryCharge() {
    return this->batteryCharge;
}

void Sun2000Inverter::updateBatteryCharge() {
    uint16_t SOC = 37004;
    uint16_t value;
    this->modbus.readHreg(this->ipAddress, SOC, &value, 1, nullptr, 1);
    delay(100);
    this->modbus.task();
    this->batteryCharge = value;
}
