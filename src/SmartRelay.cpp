#include "SmartRelay.h"

SmartRelay::SmartRelay(uint8_t pin) {
    this->_pin = pin;
    pinMode(this->_pin, OUTPUT);
    digitalWrite(this->_pin, LOW);
    this->_inverter = nullptr;
    this->_httpServer = nullptr;
    this->_preferencesNamespace = String("smart-relay") + String(this->_pin);
    this->_settingsRoute = this->_generateSettingsRoute();
    this->_indicatorRoute = this->_generateIndicatorRoute();

    _preferences.begin(_preferencesNamespace.c_str(), false);

    minBatteryChargeSetting = _preferences.getUShort(MIN_BATTERY_SETTING, 90);
    minPowerMeterActivePowerSetting = _preferences.getULong(MIN_POWER_SETTING, 300);
    monitoringWindowMinutesSetting = _preferences.getUChar(MONITOR_WINDOW_SETTING, 1);
    switchCycleMinutesSetting = _preferences.getUChar(SWITCH_CYCLE_SETTING, 1);
}

void SmartRelay::setup(Inverter *inverter, ESP8266WebServer *httpServer) {
    this->_inverter = inverter;
    this->_httpServer = httpServer;
    this->_registerHttpRoutes();
}

void SmartRelay::update() {
    if (millis() - this->_lastUpdateTime < this->UPDATE_INTERVAL) {
        return;
    }
    if (this->isPinAlwaysOn) {
        this->_setPinOn();
        return;
    }
    if (!this->isPinEnabledSetting) {
        this->_setPinOff();
        return;
    }
    if (!this->_inverter->isConnected) {
        this->_setPinOff();
        return;
    }
    if (this->isPinOn && (millis() - this->_lastEnableTime < this->_switchCycleMillis())) {
        // If relay is on and our switch cycle is running we do nothing in this update loop
        this->_lastUpdateTime = millis();
        return;
    }
    if (this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= 100)) {
            this->_setPinOn();
            return;
        }
    }
    if (!this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= (int) this->minPowerMeterActivePowerSetting)) {
            this->_setPinOn();
            return;
        }
    }
    this->_setPinOff();
}

uint64_t SmartRelay::_switchCycleMillis() const {
    return this->switchCycleMinutesSetting * 60 * 1000;
}

void SmartRelay::_setPinOn() {
    this->isPinOn = true;
    digitalWrite(this->_pin, HIGH);
    this->_lastEnableTime = millis();
    this->_lastUpdateTime = millis();
    Serial.println("Set pin ON!");
}

void SmartRelay::_setPinOff() {
    this->isPinOn = false;
    digitalWrite(this->_pin, LOW);
    this->_lastEnableTime = 0;
    this->_lastUpdateTime = millis();
    Serial.println("Set pin OFF!");
}

void SmartRelay::setIsPinEnabledSetting(bool value) {
    this->isPinEnabledSetting = value;
    this->_preferences.putBool(PIN_ENABLED_SETTING, value);
}

void SmartRelay::setMinBatteryChargeSetting(uint16_t value) {
    this->minBatteryChargeSetting = value;
    this->_preferences.putUShort(MIN_BATTERY_SETTING, value);
}

void SmartRelay::setMinPowerMeterActivePowerSetting(uint32_t value) {
    this->minPowerMeterActivePowerSetting = value;
    this->_preferences.putUInt(MIN_POWER_SETTING, value);
}

void SmartRelay::setMonitoringWindowMinutesSetting(uint8_t value) {
    this->monitoringWindowMinutesSetting = value;
    this->_preferences.putUChar(MONITOR_WINDOW_SETTING, value);
}

void SmartRelay::setSwitchCycleMinutesSetting(uint8_t value) {
    this->switchCycleMinutesSetting = value;
    this->_preferences.putUChar(SWITCH_CYCLE_SETTING, value);
}

String SmartRelay::_generateSettingsRoute() {
    return String("/settings/pin/" + String(this->_pin));
}

String SmartRelay::_generateIndicatorRoute() {
    return String(this->_settingsRoute + "/indicator");
}

String SmartRelay::_generateHTML() {
    File file = LittleFS.open("/pin.html", "r");
    if (!file) {
        return "File Not Found";
    }
    String templateContent = file.readString();
    file.close();
    std::map<String, String> variables = {
            {"PIN_NUMBER",         String(this->_pin)},
            {"ROUTE",              this->_settingsRoute},
            {"PIN_ENABLE",         this->isPinEnabledSetting ? "checked" : ""},
            {"MIN_BATTERY_CHARGE", String(this->minBatteryChargeSetting)},
            {"MIN_ACTIVE_POWER",   String(this->minPowerMeterActivePowerSetting)},
            {"MONITORING_WINDOW",  String(this->monitoringWindowMinutesSetting)},
            {"SWITCH_CYCLE",       String(this->switchCycleMinutesSetting)},
    };
    return processTemplate(templateContent, variables);
}

String SmartRelay::_generateIndicatorHTML() {
    String templateContent = R"(<div class="pin-indicator {{PIN_ACTIVE}}" hx-get="{{ROUTE}}" hx-trigger="every 10s" hx-swap="outerHTML"></div>)";
    std::map<String, String> variables = {
            {"ROUTE",      this->_indicatorRoute},
            {"PIN_ACTIVE", this->isPinOn ? "pin-active" : ""}
    };
    return processTemplate(templateContent, variables);
}

void SmartRelay::_registerHttpRoutes() {
    Serial.println(this->_settingsRoute);
    this->_httpServer->on(this->_settingsRoute, HTTP_GET, [this]() {
        this->_httpServer->send(200, "text/html", this->_generateHTML());
    });
    this->_httpServer->on(this->_settingsRoute, HTTP_POST, [this]() {
        this->setIsPinEnabledSetting(_httpServer->hasArg("pin-enable"));
        this->setMinBatteryChargeSetting((u_int16_t) this->_httpServer->arg("pin-battery").toInt());
        this->setMinPowerMeterActivePowerSetting((u_int32_t) this->_httpServer->arg("pin-active-power").toInt());
        this->setMonitoringWindowMinutesSetting((u_int8_t) this->_httpServer->arg("pin-monitor-window").toInt());
        this->setSwitchCycleMinutesSetting((u_int8_t) this->_httpServer->arg("pin-switch-cycle").toInt());
        this->_httpServer->send(200, "text/html", "Saved 👍");
    });
    this->_httpServer->on(this->_indicatorRoute, HTTP_GET, [this]() {
        this->_httpServer->send(200, "text/html", this->_generateIndicatorHTML());
    });
}

