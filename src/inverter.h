#include <deque>
#include <Arduino.h>
#include <IPAddress.h>
#include <ModbusIP_ESP8266.h>


#ifndef ESP_SOLAR_BOY_INVERTER_H
#define ESP_SOLAR_BOY_INVERTER_H

class Inverter {
public:
    Inverter();

    ~Inverter();

    void begin(IPAddress ipAddress, in_port_t port = 502);

    bool update();

    [[nodiscard]] uint16_t getBatteryStateOfCharge() const;

    [[nodiscard]] int32_t getBatteryChargePower() const;

    [[nodiscard]] int32_t getPlantPower() const;

    [[nodiscard]] int32_t getPowerMeterActivePower() const;

    [[nodiscard]] String getMeterStatus() const;

    [[nodiscard]] String getState1() const;

    void printy();

    int minBatteryStateOfCharge();

    int meanPowerMeterActivePower();

    IPAddress ipAddress;
    in_port_t port = 502;
    const uint16_t DEQUE_SIZE = 360;
    const uint64_t INVERTER_UPDATE_INTERVAL = 10000;
    const uint64_t HISTORY_UPDATE_INTERVAL = 10000;
private:
    uint64_t _lastUpdate = 0;
    ModbusIP _modbus;
    uint16_t _batteryStateOfCharge = 0;
    int32_t _batteryChargePower = 0;
    int32_t _plantPower = 0;
    int32_t _powerMeterActivePower = 0;
    uint16_t _meterStatus = 0;
    uint16_t _state1 = 0;
    uint64_t _lastHistoryUpdate = 0;
    std::deque<int> _powerMeterActivePowerHistory;
    std::deque<int> _batteryStateOfChargeHistory;

    void _addToDeque(int value, std::deque<int> *dq);

    void _printDeque(std::deque<int> *dq);

    void _updateHistory();
};

#endif //ESP_SOLAR_BOY_INVERTER_H
