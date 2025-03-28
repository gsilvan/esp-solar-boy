#include "DataCollector.h"

void DataCollector::setup(Inverter *inverter, const String &deviceId, uint64_t updateInterval) {
    this->_inverter = inverter;
    this->_deviceId = deviceId;
    this->_updateInterval = updateInterval;
}

void DataCollector::update() {
    if (!this->isEnabled) {
        return;
    }
    if (millis() - this->_updateInterval <= this->_lastUpdate) {
        return;
    }
    Serial.println("+++ BEGIN COLLECTION +++");

    this->httpClient.begin(this->wifiClient, this->url);
    this->httpClient.addHeader("Content-Type", "application/json");

    Serial.print("POST ");
    Serial.println(this->url);
    String payload;
    serializeJson(this->generateJson(), payload);
    int32_t httpStatus = this->httpClient.POST(payload);

    Serial.print("HTTP_STATUS: ");
    Serial.println(httpStatus);

    Serial.println("+++ END COLLECTION +++");
    this->_lastUpdate = millis();
}

JsonDocument DataCollector::generateJson() {
    JsonDocument doc = JsonDocument();
    doc["deviceId"] = this->_deviceId;
    doc["batteryStateOfCharge"] = this->_inverter->getBatteryStateOfCharge();
    doc["plantPower"] = this->_inverter->getPlantPower();
    doc["gridPower"] = this->_inverter->getPowerMeterActivePower();
    return doc;
}

DataCollector::DataCollector() {
    this->_preferences.begin("telemetry", false);
    this->isEnabled = this->_preferences.getBool("isEnabled", false);
    this->url = this->_preferences.getString("url", "");
    this->wifiClient = WiFiClient();
    this->httpClient = HTTPClient();
}

void DataCollector::setIsEnabled(bool value) {
    this->isEnabled = value;
    this->_preferences.putBool("isEnabled", this->isEnabled);
}

void DataCollector::setUrl(const String &url) {
    this->url = url;
    this->_preferences.putString("url", this->url);
}
