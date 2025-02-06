#include "inverter.h"

Inverter::Inverter() = default;

Inverter::~Inverter() = default;

void Inverter::begin(IPAddress ipAddress, in_port_t port) {
    this->ipAddress = ipAddress;
    this->port = port;
    this->_modbus.client();
}

bool Inverter::update() {
    uint64_t currentMillis = millis();
    if (currentMillis - this->_lastUpdate >= this->_updateInterval) {
        if (!this->_modbus.isConnected(this->ipAddress)) {
            Serial.println("Inverter is disconnected!");
            this->_modbus.connect(this->ipAddress, this->port);
        } else {
            this->_modbus.readHreg(this->ipAddress, 37004, (uint16_t *) &this->_batteryStateOfCharge, 1, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37001, (uint16_t *) &this->_batteryChargePower, 2, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 32064, (uint16_t *) &this->_gridPower, 2, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37100, (uint16_t *) &this->_meterStatus, 1, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37113, (uint16_t *) &this->_powerMeterActivePower, 2, nullptr, 1);
            this->_lastUpdate = millis();
        }
        this->_modbus.task();
        this->printy();
        return true;
    }
    return false;
}

uint16_t Inverter::getBatteryStateOfCharge() const {
    return this->_batteryStateOfCharge / 10;
}

int32_t Inverter::getBatteryChargePower() const {
    return (this->_batteryChargePower >> 16) | (this->_batteryChargePower << 16);
}

int32_t Inverter::getGridPower() const {
    return (this->_gridPower >> 16) | (this->_gridPower << 16);
}

void Inverter::printy() const {
    Serial.printf("Battery: %d\n", this->getBatteryStateOfCharge());
    Serial.printf("Battery charge power: %d\n", this->getBatteryChargePower());
    Serial.printf("Grid power: %d\n", this->getGridPower());
    Serial.printf("Power meter status: %s\nd", this->getMeterStatus().c_str());
    Serial.printf("Power meter active power: %d\n", this->getPowerMeterActivePower());
}

int32_t Inverter::getPowerMeterActivePower() const {
    return (this->_powerMeterActivePower >> 16) | (this->_powerMeterActivePower << 16);
}

String Inverter::getMeterStatus() const {
    switch (this->_meterStatus) {
        case 0:
            return "offline";
        case 1:
            return "normal";
        default:
            return "";
    }
}
