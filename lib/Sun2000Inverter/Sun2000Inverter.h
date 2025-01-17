#include <Arduino.h>
#include <IPAddress.h>
#include <ModbusIP_ESP8266.h>

#ifndef ESP_SOLAR_BOY_SUN2000INVERTER_H
#define ESP_SOLAR_BOY_SUN2000INVERTER_H


class Sun2000Inverter {
public:
    Sun2000Inverter();

    ~Sun2000Inverter();

    void begin(IPAddress ipAddress, in_port_t port = 502);

    void update();

    uint8_t getBatteryCharge();

private:
    IPAddress ipAddress;
    in_port_t port = 502;
    uint64_t lastUpdate = 0;
    const uint64_t updateInterval = 2000;
    ModbusIP modbus;
    uint8_t batteryCharge = 0;

    void updateBatteryCharge();
};


#endif //ESP_SOLAR_BOY_SUN2000INVERTER_H
