#include <numeric>
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
    if (currentMillis - this->_lastUpdate >= this->INVERTER_UPDATE_INTERVAL) {
        if (!this->_modbus.isConnected(this->ipAddress)) {
            Serial.println("Inverter is disconnected!");
            this->_modbus.connect(this->ipAddress, this->port);
        } else {
            this->_modbus.readHreg(this->ipAddress, 37004, (uint16_t *) &this->_batteryStateOfCharge, 1, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37001, (uint16_t *) &this->_batteryChargePower, 2, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 32064, (uint16_t *) &this->_plantPower, 2, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37100, (uint16_t *) &this->_meterStatus, 1, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 37113, (uint16_t *) &this->_powerMeterActivePower, 2, nullptr, 1);
            this->_modbus.readHreg(this->ipAddress, 32000, (uint16_t *) &this->_state1, 1, nullptr, 1);
        }
        this->_modbus.task();
        this->_lastUpdate = millis();
        this->printy();
        return true;
    }
    this->_updateHistory(); // We invoke update the history in update loops where no value is fetched from inverter.
    return false;
}

uint16_t Inverter::getBatteryStateOfCharge() const {
    return this->_batteryStateOfCharge / 10;
}

int32_t Inverter::getBatteryChargePower() const {
    return (this->_batteryChargePower >> 16) | (this->_batteryChargePower << 16);
}

int32_t Inverter::getPlantPower() const {
    return (this->_plantPower >> 16) | (this->_plantPower << 16);
}

void Inverter::printy() {
    Serial.printf("Battery: %d\n", this->getBatteryStateOfCharge());
    Serial.printf("Battery charge power: %d\n", this->getBatteryChargePower());
    Serial.printf("Plant power: %d\n", this->getPlantPower());
    Serial.printf("Power meter status: %s\n", this->getMeterStatus().c_str());
    Serial.printf("Power meter active power: %d\n", this->getPowerMeterActivePower());
    Serial.printf("State1: %s\n", this->getState1().c_str());
    Serial.printf("State1 RAW: %d\n", this->_state1);
    this->_printDeque(&this->_powerMeterActivePowerHistory);
    this->_printDeque(&this->_batteryStateOfChargeHistory);
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

String Inverter::getState1() const {
    switch (this->_state1) {
        case 0b0000000001:
            return "standby";
        case 0b0000000010:
            return "grid-connected";
        case 0b0000000100:
            return "grid-connected normally";
        case 0b0000001000:
            return "grid connection with derating due to power rationing";
        case 0b0000010000:
            return "grid connection with derating due to internal causes of the solar inverter";
        case 0b0000100000:
            return "normal stop";
        case 0b0001000000:
            return "stop due to faults";
        case 0b0010000000:
            return "stop due to power rationing";
        case 0b0100000000:
            return "shutdown";
        case 0b1000000000:
            return "spot check";
        default:
            return "";
    }
}

void Inverter::_addToDeque(int value, std::deque<int> *dq) {
    if (dq->size() >= this->DEQUE_SIZE) {
        dq->pop_front();
    }
    dq->push_back(value);
}

void Inverter::_printDeque(std::deque<int> *dq) {
    for (auto val: *dq) {
        Serial.print(val);
        Serial.print(" ");
    }
    Serial.println();
}

void Inverter::_updateHistory() {
    if (millis() - this->_lastHistoryUpdate >= this->HISTORY_UPDATE_INTERVAL) {
        this->_addToDeque(this->getPowerMeterActivePower(), &this->_powerMeterActivePowerHistory);
        this->_addToDeque(this->getBatteryStateOfCharge(), &this->_batteryStateOfChargeHistory);
        this->_lastHistoryUpdate = millis();
    }
}

int Inverter::minBatteryStateOfCharge() {
    if (this->_batteryStateOfChargeHistory.empty()) {
        return 0;
    }
    return *std::min_element(this->_batteryStateOfChargeHistory.begin(), this->_batteryStateOfChargeHistory.end());
}

int Inverter::meanPowerMeterActivePower() {
    if (this->_powerMeterActivePowerHistory.empty()) {
        return 0;
    }
    double input_sum = std::accumulate(this->_powerMeterActivePowerHistory.begin(),
                                       this->_powerMeterActivePowerHistory.end(), 0);
    return (int) (input_sum / this->_powerMeterActivePowerHistory.size());
}
