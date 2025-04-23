#include "LongPressButton.h"

LongPressButton::LongPressButton(int pin, unsigned long pressDuration) : pin(pin),
                                                                         requiredDuration(pressDuration) {}

void LongPressButton::begin() const {
    pinMode(pin, INPUT_PULLUP);
}

void LongPressButton::onLongPress(void (*cb)()) {
    this->callback = cb;
}

void LongPressButton::update() {
    bool isPressed = digitalRead(pin) == LOW;
    unsigned long now = millis();

    if (isPressed && !this->wasPressed) {
        this->pressStartTime = now;
        this->triggered = false;
    }

    if (isPressed && this->wasPressed && !this->triggered && now - this->pressStartTime >= this->requiredDuration) {
        this->triggered = true;
        if (this->callback) {
            this->callback();
        }
    }

    if (!isPressed) {
        this->pressStartTime = 0;
        this->triggered = false;
    }

    this->wasPressed = isPressed;
}
