
#ifndef ESP_SOLAR_BOY_LONGPRESSBUTTON_H
#define ESP_SOLAR_BOY_LONGPRESSBUTTON_H

#include <Arduino.h>

class LongPressButton {
public:
    explicit LongPressButton(int pin, unsigned long pressDuration = 5000);
    void begin() const;
    void onLongPress(void (*cb)());
    void update();

private:
    int pin;
    unsigned long pressStartTime = 0;
    bool wasPressed = false;
    bool triggered = false;
    unsigned long requiredDuration;
    void (*callback)() = nullptr;
};


#endif //ESP_SOLAR_BOY_LONGPRESSBUTTON_H
