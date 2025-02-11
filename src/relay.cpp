#include "relay.h"


void RelayManager::_addBatteryValue(uint16_t value) {
    if (this->_batteryStateOfCharge.size() >= this->_maxBufferLength) {
        this->_batteryStateOfCharge.pop_front();
    }
    this->_batteryStateOfCharge.push_back(value);
}

void RelayManager::_addActivePowerValue(int32_t value) {
    if (this->_powerMeterActivePower.size() >= this->_maxBufferLength) {
        this->_powerMeterActivePower.pop_front();
    }
    this->_powerMeterActivePower.push_back(value);
}

void RelayManager::update() {
    if (millis() - this->_lastBufferUpdate >= this->_bufferUpdateInterval) {
        this->_addBatteryValue(this->_inverter->getBatteryStateOfCharge());
        this->_addActivePowerValue(this->_inverter->getPowerMeterActivePower());
        this->_lastBufferUpdate = millis();
    }

    for (auto r: this->_relays) {
        r.update();
    }
}

void RelayManager::addRelay(Relay &r) {
    this->_relays.push_back(r);
}

static const char pinHtmlTemplate[] PROGMEM =
        R"(
<h3>PIN_%PIN%</h3>
<div>
    <label for="pin-%PIN%-battery">Battery charge (%):</label>
    <input class="input" type="number" min="0" max="100" step="1" id="pin-%PIN%-battery" name="pin-%PIN%-battery" value="%BATTERYCHARGE%">
</div>
<div>
    <label for="pin-%PIN%-input-power">Power overflow (Watts):</label>
    <input class="input" type="number" min="100" step="100" id="pin-%PIN%-input-power" name="pin-%PIN%-input-power" value="%PIN0INPUTPOWER%">
</div>
    <label for="pin-%PIN%-timer">Monitoring window (minutes):</label>
    <input class="input" type="number" min="0" max="60" step="1" id="pin-%PIN%-timer" name="pin-%PIN%-timer" value="%PIN0TIMER%">
<div>
    <label for="pin-%PIN%-cycle">Switch cycle (minutes):</label>
    <input class="input" type="number" min="0" max="60" step="1" id="pin-%PIN%-cycle" name="pin-%PIN%-cycle" value="%PIN0CYCLE%">
</div>
)";


String replaceAll(String str, const String &from, const String &to) {
    if (from.length() == 0) return str; // Prevent infinite loop
    int startPos = 0;
    while ((startPos = str.indexOf(from, startPos)) != -1) {
        str = str.substring(0, startPos) + to + str.substring(startPos + from.length());
        startPos += to.length(); // Move past the replaced substring
    }
    return str;
}

String pinHtml(uint8_t pin) {
    String html(reinterpret_cast<const char *>(pinHtmlTemplate));
    return replaceAll(html, "%PIN%", String(pin));
}

static const char settingsHtmlTemplate[] PROGMEM =
        R"(
<h1>It works</h1>
)";


void RelayManager::setup(Inverter *inverter, ESP8266WebServer &webServer) {
    this->_inverter = inverter;

    webServer.on("/foobar", [&]() {
        webServer.send(200, "text/html", pinHtml(0) + pinHtml(1) + pinHtml(2));
    });
}

uint16_t RelayManager::_getMeanBatteryStateOfCharge() {
    uint16_t sum = 0;
    for (auto val: this->_batteryStateOfCharge) {
        sum += val;
    }
    return sum / this->_batteryStateOfCharge.size();
}

int32_t RelayManager::_getMeanPowerMeterActivePower() {
    int32_t sum = 0;
    for (auto val : this->_powerMeterActivePower) {
        sum += val;
    }
    return sum / (int32_t) this->_powerMeterActivePower.size();
}

void Relay::update() {
    uint8_t _pinMode = this->_isOn ? HIGH : LOW;
    pinMode(this->_pin, _pinMode);
}
