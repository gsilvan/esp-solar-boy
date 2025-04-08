#ifndef ESP_SOLAR_BOY_LATESTRELEASE_H
#define ESP_SOLAR_BOY_LATESTRELEASE_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "version.h"

struct Firmware {
    String version;
    String url;
};

class LatestRelease {
public:
    LatestRelease();
    ~LatestRelease() = default;
    void check();
    void install() const;
    void setSearchForNewerFirmware(bool value);
    void setAutomaticFirmwareInstall(bool value);
    void printy();
    Firmware firmware = {"", ""};
    bool searchForNewerFirmware = false;
    bool automaticFirmwareInstall = false;
    bool isNewerFirmwareAvailable = false;

private:
    JsonDocument _fetchLatestReleaseData();
    const uint64_t UPDATE_INTERVAL = 10000;
    const char *apiUrl = "https://api.github.com/repos/gsilvan/esp-solar-boy/releases/latest";
    String _installedFirmwareVersion = FIRMWARE_VERSION;
    uint64_t _lastFirmwareCheck = 0;
    Preferences _preferences;
};


#endif //ESP_SOLAR_BOY_LATESTRELEASE_H
