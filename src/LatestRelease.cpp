#include "LatestRelease.h"

LatestRelease::LatestRelease() {
    this->_preferences.begin("latest-release", false);
    this->searchForNewerFirmware = this->_preferences.getBool("search", false);
    this->automaticFirmwareInstall = this->_preferences.getBool("auto", false);
}


JsonDocument LatestRelease::_fetchLatestReleaseData() {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure());
    client->setInsecure();  // disables certificate validation (deliberately!)
    HTTPClient httpsClient;

    // We use VERY INSECURE SSL here on purpose!
    // We check the firmware's signature with the hardcoded public-key before install.
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    auto doc = JsonDocument();

    bool ok = httpsClient.begin(*client, this->apiUrl);
    Serial.printf("httpsClient.begin() returned: %s\n", ok ? "true" : "false");
    if (!ok) return doc;
    httpsClient.addHeader("User-Agent", "ESP8266");
    int httpStatusCode = httpsClient.GET();
    if (httpStatusCode != HTTP_CODE_OK) {
        Serial.printf("HTTP-Code: %d\n", httpStatusCode);
        Serial.println(httpsClient.getString());
        return doc;
    }
    deserializeJson(doc, httpsClient.getString());
    return doc;
}

void LatestRelease::install() const {
    if (!this->isNewerFirmwareAvailable) {
        return;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient httpsClient;

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, this->firmware.url);

    switch (ret) {
        case HTTP_UPDATE_OK:
            Serial.println("Update successful!");
            break;
        case HTTP_UPDATE_FAILED:
            Serial.printf("Update failed! Error (%d): %s\n",
                          ESPhttpUpdate.getLastError(),
                          ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No update found.");
            break;
    }
}

void LatestRelease::check() {
    if (!this->searchForNewerFirmware) {
        return;
    }
    if (millis() - this->_lastFirmwareCheck <= this->UPDATE_INTERVAL) {
        return;
    }
    auto doc = this->_fetchLatestReleaseData();
    this->firmware.version = String(doc["tag_name"]);
    this->firmware.url = String(doc["assets"][0]["browser_download_url"]);
    this->isNewerFirmwareAvailable = (this->_installedFirmwareVersion != this->firmware.version);
    this->_lastFirmwareCheck = millis();
    this->printy();
    if (this->automaticFirmwareInstall) {
        this->install();
    }
}

void LatestRelease::setSearchForNewerFirmware(bool value) {
    this->searchForNewerFirmware = value;
    this->_preferences.putBool("search", value);
}

void LatestRelease::setAutomaticFirmwareInstall(bool value) {
    this->automaticFirmwareInstall = value;
    this->_preferences.putBool("auto", value);
}

void LatestRelease::printy() {
    Serial.printf("Installed Firmware: %s\n", this->_installedFirmwareVersion.c_str());
    Serial.printf("Available Firmware: %s (%s)\n", this->firmware.version.c_str(), this->firmware.url.c_str());
}

