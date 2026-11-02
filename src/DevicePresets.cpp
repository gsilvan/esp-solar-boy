#include <algorithm>
#include <string>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "DevicePresets.h"
#include "InverterConfig.h"

void loadDevicePresets() {
    auto &presets = devicePresetRegistry();
    presets.clear();
    std::vector<std::string> paths;
#ifdef ESP32
    File directory = LittleFS.open("/presets");
    for (File file = directory.openNextFile(); file; file = directory.openNextFile()) {
        if (!file.isDirectory()) paths.emplace_back(file.path());
    }
#else
    Dir directory = LittleFS.openDir("/presets");
    while (directory.next())
        if (!directory.isDirectory()) paths.emplace_back(std::string("/presets/") + directory.fileName().c_str());
#endif
    std::sort(paths.begin(), paths.end());
    for (const auto &path : paths) {
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".json") != 0) continue;
        File file = LittleFS.open(path.c_str(), "r");
        const String text = file.readString();
        DynamicJsonDocument document(2048);
        if (deserializeJson(document, text.c_str(), text.length())) continue;

        DevicePreset preset;
        preset.id = PresetId(document["id"].as<const char *>());
        strncpy(preset.label, document["name"] | "", sizeof(preset.label) - 1);
        preset.requiresOverrides = document["requires_overrides"].as<bool>();
        for (size_t i = 0; i < TELEMETRY_COUNT; ++i)
            preset.telemetry[i] = readTelemetryJson(document[telemetryKey(i)].as<JsonObjectConst>());
        presets.push_back(preset);
    }
}
