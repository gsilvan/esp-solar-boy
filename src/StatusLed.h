#ifndef ESP_SOLAR_BOY_STATUSLEDS_H
#define ESP_SOLAR_BOY_STATUSLEDS_H

#include <Arduino.h>
#include <Inverter.h>

class StatusLed {
public:
    explicit StatusLed(uint8_t pin);

    void attachInverter(Inverter *inverter);

    void update();

private:
    Inverter *_inverter = nullptr;
    uint8_t _ledPin;

    void _turnOn();

    void _turnOff();

    bool _isOn = false;
};


#endif //ESP_SOLAR_BOY_STATUSLEDS_H
