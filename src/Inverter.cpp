#include <numeric>
#include "Inverter.h"

Inverter::Inverter() :
        _powerMeterActivePowerHistory(DEQUE_SIZE, 0),
        _batteryStateOfChargeHistory(DEQUE_SIZE, 0) {
    this->_preferences.begin("inverter", false);
    this->setIpAddress(this->_preferences.getString("ip", ""));
    this->setPort(this->_preferences.getUShort("port", 502));
    this->setModbusUnit(this->_preferences.getUChar("unit", 1));
    this->_modbus.client();
};

Inverter::~Inverter() = default;

bool Inverter::update() {
    if (millis() - this->_lastUpdate >= this->INVERTER_UPDATE_INTERVAL) {
        if (!this->_modbus.isConnected(this->ipAddress)) {
            this->isConnected = false;
            Serial.println("Inverter is disconnected!");
            this->_modbus.connect(this->ipAddress, this->port);
        } else {
            this->isConnected = true;
            auto cb = [this](Modbus::ResultCode event, uint16_t transactionId, void *data) -> bool {
                // Modified example:
                // https://github.com/emelianov/modbus-esp8266/blob/master/examples/Callback/Transactional/Transactional.ino#L40-L48
                if (event != Modbus::EX_SUCCESS) {
                    Serial.printf("Modbus error [%d]: %02X\n", transactionId, event);
                }
                if (event == Modbus::EX_TIMEOUT) {
                    this->_modbus.disconnect(this->ipAddress);
                    this->_modbus.dropTransactions();
                }
                return true;
            };
            this->_modbus.readHreg(this->ipAddress, 37004, (uint16_t *) &this->_batteryStateOfChargeRaw, 1, cb, this->modbusUnit);
            this->_modbus.readHreg(this->ipAddress, 37001, (uint16_t *) &this->_batteryChargePowerRaw, 2, cb, this->modbusUnit);
            this->_modbus.readHreg(this->ipAddress, 32064, (uint16_t *) &this->_plantPowerRaw, 2, cb, this->modbusUnit);
            this->_modbus.readHreg(this->ipAddress, 37113, (uint16_t *) &this->_powerMeterActivePowerRaw, 2, cb, this->modbusUnit);
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
    return this->_batteryStateOfChargeRaw / 10;
}

int32_t Inverter::getBatteryChargePower() const {
    return (int32_t) (_batteryChargePowerRaw[0] << 16 | _batteryChargePowerRaw[1]);
}

int32_t Inverter::getPlantPower() const {
    return (int32_t) (_plantPowerRaw[0] << 16 | _plantPowerRaw[1]);
}

void Inverter::printy() {
    Serial.printf("Battery: %d\n", this->getBatteryStateOfCharge());
    Serial.printf("Battery charge power: %d\n", this->getBatteryChargePower());
    Serial.printf("Plant power: %d\n", this->getPlantPower());
    Serial.printf("Power meter active power: %d\n", this->getPowerMeterActivePower());
    Inverter::_printDeque(&this->_powerMeterActivePowerHistory);
    Inverter::_printDeque(&this->_batteryStateOfChargeHistory);
}

int32_t Inverter::getPowerMeterActivePower() const {
    return (int32_t) (this->_powerMeterActivePowerRaw[0] << 16 | this->_powerMeterActivePowerRaw[1]);
}

void Inverter::_addToDeque(int value, std::deque<int> *dq) const {
    if (dq->size() >= this->DEQUE_SIZE) {
        dq->pop_front();
    }
    dq->push_back(value);
}

void Inverter::_printDeque(std::deque<int> *dq) {
    if (!dq) {
        return;
    }
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

int Inverter::_minutesToN(unsigned int minutes) const {
    return 60000 * minutes / this->HISTORY_UPDATE_INTERVAL;
}

int Inverter::minBatteryStateOfCharge(int lastNMinutes) {
    if (this->_batteryStateOfChargeHistory.empty()) {
        return 0;
    }
    int indexOffset = this->_minutesToN(lastNMinutes);
    return *std::min_element(this->_batteryStateOfChargeHistory.end() - indexOffset, this->_batteryStateOfChargeHistory.end());
}

int Inverter::meanPowerMeterActivePower(int lastNMinutes) {
    if (this->_powerMeterActivePowerHistory.empty()) {
        return 0;
    }
    int indexOffset = this->_minutesToN(lastNMinutes);
    double input_sum = std::accumulate(this->_powerMeterActivePowerHistory.end() - indexOffset,
                                       this->_powerMeterActivePowerHistory.end(), 0);
    return (int) (input_sum / indexOffset);
}

void Inverter::setIpAddress(const IPAddress &ip) {
    this->ipAddress = ip;
    this->_preferences.putString("ip", this->ipAddress.toString());
}

void Inverter::setIpAddress(const String &ip) {
    IPAddress _tempIp;
    _tempIp.fromString(ip);
    this->setIpAddress(_tempIp);
}

void Inverter::setPort(in_port_t _port) {
    this->port = _port;
    this->_preferences.putUShort("port", this->port);
}

void Inverter::setPort(const String &_port) {
    this->setPort((in_port_t)_port.toInt());
}

void Inverter::setModbusUnit(uint8_t _unit) {
    this->modbusUnit = _unit;
    this->_preferences.putUChar("unit", _unit);
}

void Inverter::setModbusUnit(const String &_unit) {
    this->setModbusUnit((uint8_t) _unit.toInt());
}
