#include <algorithm>
#include <string>
#include <LittleFS.h>
#include "DevicePresets.h"
#include "PresetToml.h"

bool loadDevicePresets() {
    devicePresetRegistry().clear();
    std::vector<std::string> paths;
#ifdef ESP32
    File directory = LittleFS.open("/presets");
    if (!directory || !directory.isDirectory()) {
        Serial.println("Preset directory /presets is missing");
        return false;
    }
    for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
        if (!file.isDirectory()) paths.emplace_back(file.path());
        file.close();
    }
    directory.close();
#else
    Dir directory = LittleFS.openDir("/presets");
    while (directory.next())
        if (!directory.isDirectory()) paths.emplace_back(std::string("/presets/") + directory.fileName().c_str());
#endif
    std::sort(paths.begin(), paths.end());
    std::vector<DevicePreset> loaded;
    bool clean = true;
    for (const auto &path : paths) {
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".toml") != 0) continue;
        File file = LittleFS.open(path.c_str(), "r");
        if (!file || file.size() == 0 || file.size() > 8192 || loaded.size() >= 16) {
            Serial.printf("Cannot load preset %s (missing, empty, oversized or too many presets)\n", path.c_str());
            clean = false;
            continue;
        }
        const size_t expected = file.size();
        const String text = file.readString();
        file.close();
        DevicePreset preset;
        const char *error = "File read failed";
        if (text.length() != expected || !parseDevicePresetToml(text.c_str(), text.length(), preset, error)) {
            Serial.printf("Invalid preset %s: %s\n", path.c_str(), error);
            clean = false;
            continue;
        }
        for (const auto &existing : loaded) {
            if (existing.id == preset.id) {
                Serial.printf("Duplicate preset id %s; no presets loaded\n", preset.id.c_str());
                return false;
            }
        }
        loaded.push_back(preset);
    }
    devicePresetRegistry().swap(loaded);
    Serial.printf("Loaded %u device presets from LittleFS\n", unsigned(devicePresetRegistry().size()));
    return clean && !devicePresetRegistry().empty();
}
