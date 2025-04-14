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
    this->_labelRoute = this->_generateLabelRoute();

    this->_preferences.begin(_preferencesNamespace.c_str(), false);
    this->isPinEnabledSetting = _preferences.getBool(PIN_ENABLED_SETTING, false);
    this->isPinAlwaysOnSetting = _preferences.getBool(PIN_ALWAYS_ON_SETTING, false);
    this->minBatteryChargeSetting = _preferences.getUShort(MIN_BATTERY_SETTING, 90);
    this->minPlantPowerSetting = _preferences.getUInt(MIN_PLANT_POWER_SETTING, 0);
    this->minPowerMeterActivePowerSetting = _preferences.getInt(MIN_ACTIVE_POWER_SETTING, 300);
    this->monitoringWindowMinutesSetting = _preferences.getUChar(MONITOR_WINDOW_SETTING, 1);
    this->switchCycleMinutesSetting = _preferences.getUChar(SWITCH_CYCLE_SETTING, 1);
    this->name = _preferences.getString(NAME_SETTING, String(this->_pin));
}

void SmartRelay::setup(Inverter *inverter, AsyncWebServer *httpServer) {
    this->_inverter = inverter;
    this->_httpServer = httpServer;
    this->_registerHttpRoutes();
}

void SmartRelay::update() {
    if (millis() - this->_lastUpdateTime < this->UPDATE_INTERVAL) {
        return;
    }
    if (!this->isPinEnabledSetting) {
        this->_setPinOff();
        return;
    }
    if (this->isPinAlwaysOnSetting) {
        this->_setPinOn();
        return;
    }
    if (!this->_inverter->isConnected) {
        Serial.printf("[%d] OFF (Inverter not connected)\n", this->_pin);
        this->_setPinOff();
        return;
    }
    Serial.printf("minBattery: %d\n", this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting));
    Serial.printf("meanActPwr: %d\n", this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting));
    if (this->isPinOn && (millis() - this->_lastEnableTime < this->_switchCycleMillis())) {
        // If relay is on and our switch cycle is running we do nothing in this update loop
        Serial.printf("[%d] ON (Holding switch cycle)\n", this->_pin);
        this->_lastUpdateTime = millis();
        return;
    }
    if (this->_inverter->meanPlantPower(this->monitoringWindowMinutesSetting) < (int) this->minPlantPowerSetting) {
        Serial.printf("[%d] OFF (Plant power smaller than threshold %d < %d)\n",
                      this->_pin,
                      this->_inverter->meanPlantPower(this->monitoringWindowMinutesSetting),
                      (int) this->minPlantPowerSetting);
        this->_setPinOff();
        return;
    }
    if (this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= 100)) {
            Serial.printf("[%d] ON (Pin was on, active power still > 100 Watts)\n", this->_pin);
            this->_setPinOn();
            return;
        }
    }
    if (!this->isPinOn) {
        if ((this->_inverter->minBatteryStateOfCharge(this->monitoringWindowMinutesSetting) >= (int) this->minBatteryChargeSetting) &&
            (this->_inverter->meanPowerMeterActivePower(this->monitoringWindowMinutesSetting) >= (int) this->minPowerMeterActivePowerSetting)) {
            Serial.printf("[%d] ON (Pin was off, active power > %d Watts)\n", this->_pin, this->minPowerMeterActivePowerSetting);
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
/*    Serial.println("Set pin OFF!");*/
}

void SmartRelay::setIsPinEnabledSetting(bool value) {
    this->isPinEnabledSetting = value;
    this->_preferences.putBool(PIN_ENABLED_SETTING, value);
}

void SmartRelay::setIsPinAlwaysOnSetting(bool value) {
    this->isPinAlwaysOnSetting = value;
    this->_preferences.putBool(PIN_ALWAYS_ON_SETTING, value);
}

void SmartRelay::setMinBatteryChargeSetting(uint16_t value) {
    this->minBatteryChargeSetting = value;
    this->_preferences.putUShort(MIN_BATTERY_SETTING, value);
}

void SmartRelay::setMinPlantPowerSetting(uint32_t value) {
    this->minPlantPowerSetting = value;
    this->_preferences.putUInt(MIN_PLANT_POWER_SETTING, value);
}

void SmartRelay::setMinPowerMeterActivePowerSetting(int32_t value) {
    this->minPowerMeterActivePowerSetting = value;
    this->_preferences.putInt(MIN_ACTIVE_POWER_SETTING, value);
}

void SmartRelay::setMonitoringWindowMinutesSetting(uint8_t value) {
    this->monitoringWindowMinutesSetting = value;
    this->_preferences.putUChar(MONITOR_WINDOW_SETTING, value);
}

void SmartRelay::setSwitchCycleMinutesSetting(uint8_t value) {
    this->switchCycleMinutesSetting = value;
    this->_preferences.putUChar(SWITCH_CYCLE_SETTING, value);
}

void SmartRelay::setName(String value) {
    this->name = value;
    this->_preferences.putString(NAME_SETTING, value);
}

String SmartRelay::_generateSettingsRoute() {
    return String("/pin/" + String(this->_pin));
}

String SmartRelay::_generateIndicatorRoute() {
    return String(this->_settingsRoute + "/indicator");
}

String SmartRelay::_generateLabelRoute() {
    return String(this->_settingsRoute + "/label");
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
            {"PIN_ALWAYS_ON",      this->isPinAlwaysOnSetting ? "checked" : ""},
            {"PIN_NAME",           this->name},
            {"MIN_BATTERY_CHARGE", String(this->minBatteryChargeSetting)},
            {"MIN_PLANT_POWER",    String(this->minPlantPowerSetting)},
            {"MIN_ACTIVE_POWER",   String(this->minPowerMeterActivePowerSetting)},
            {"MONITORING_WINDOW",  String(this->monitoringWindowMinutesSetting)},
            {"SWITCH_CYCLE",       String(this->switchCycleMinutesSetting)},
    };
    return processTemplate(templateContent, variables);
}

String SmartRelay::_generateIndicatorHTML() {
    String templateContent = R"(<div class="relay-indicator {{PIN_ACTIVE}}" hx-get="{{ROUTE}}" hx-trigger="every 10s" hx-swap="outerHTML">{{ON_OFF}}</div>)";
    std::map<String, String> variables = {
            {"ROUTE",      this->_indicatorRoute},
            {"PIN_ACTIVE", this->isPinOn ? "relay-on" : ""},
            {"ON_OFF",     this->isPinOn ? "on" : "off"},
    };
    return processTemplate(templateContent, variables);
}

void SmartRelay::_registerHttpRoutes() {
    this->_httpServer->on(this->_indicatorRoute.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", this->_generateIndicatorHTML());
    });
    this->_httpServer->on(this->_labelRoute.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", this->name);
    });
    this->_httpServer->on(this->_settingsRoute.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", this->_generateHTML());
    });
    this->_httpServer->on(this->_settingsRoute.c_str(), HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->setIsPinEnabledSetting(request->hasArg("pin-enable"));
        this->setIsPinAlwaysOnSetting(request->hasArg("pin-always-on"));
        this->setName(request->arg("pin-name"));
        this->setMinBatteryChargeSetting((u_int16_t) request->arg("pin-battery").toInt());
        this->setMinPlantPowerSetting((uint32_t) request->arg("pin-plant-power").toInt());
        this->setMinPowerMeterActivePowerSetting((int32_t) request->arg("pin-active-power").toInt());
        this->setMonitoringWindowMinutesSetting((u_int8_t) request->arg("pin-monitor-window").toInt());
        this->setSwitchCycleMinutesSetting((u_int8_t) request->arg("pin-switch-cycle").toInt());
        request->send(200, "text/plain", "Saved 👍");
    });
}

