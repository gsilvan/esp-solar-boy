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

    [[nodiscard]] int32_t getGridPower() const;

    [[nodiscard]] int32_t getPowerMeterActivePower() const;

    [[nodiscard]] String getMeterStatus() const;

    IPAddress ipAddress;
    in_port_t port = 502;

    void printy() const;


private:
    const uint64_t _updateInterval = 10000;
    uint64_t _lastUpdate = 0;
    ModbusIP _modbus;
    uint16_t _batteryStateOfCharge = 0;
    int32_t _batteryChargePower = 0;
    int32_t _gridPower = 0;
    int32_t _powerMeterActivePower = 0;
    uint16_t _meterStatus = 0;
};

#endif //ESP_SOLAR_BOY_INVERTER_H
