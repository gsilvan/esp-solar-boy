#include <algorithm>
#include <string>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "DevicePresets.h"

bool parseDevicePresetJson(const char *text, size_t length, DevicePreset &preset, const char *&error) {
    DynamicJsonDocument document(2048);
    if (const auto result = deserializeJson(document, text, length)) {
        error = result.c_str();
        return false;
    }
    const auto root = document.as<JsonObjectConst>();
    const char *id = root["id"];
    const char *name = root["name"];
    DevicePreset candidate;
    error = "Invalid preset header";
    if (root.size() != 3 + TELEMETRY_COUNT || !id || !*id || strlen(id) >= 32 ||
        !name || !*name || strlen(name) >= sizeof(candidate.label) || !root["requires_overrides"].is<bool>()) return false;
    for (const char *p = id; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return false;
    candidate.id = PresetId(id);
    strcpy(candidate.label, name);
    candidate.requiresOverrides = root["requires_overrides"].as<bool>();

    error = "Invalid telemetry mapping";
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto object = root[telemetryKey(i)].as<JsonObjectConst>();
        const char *type = object["type"];
        const char *order = object["word_order"];
        if (object.size() != 7 || !object["register"].is<uint16_t>() || !object["unit"].is<uint8_t>() ||
            !type || !order || !object["scale"].is<double>() ||
            !object["invert"].is<bool>() || !object["ignore_no_data"].is<bool>()) return false;
        auto &mapping = candidate.telemetry[i];
        mapping.registerAddress = object["register"].as<uint16_t>();
        mapping.unit = object["unit"].as<uint8_t>();
        mapping.scale = object["scale"].as<double>();
        mapping.invert = object["invert"].as<bool>();
        mapping.rejectUnavailable = object["ignore_no_data"].as<bool>();
        bool knownType = false;
        for (auto valueType : {ValueType::U16, ValueType::I16, ValueType::U32, ValueType::I32, ValueType::Float32}) {
            if (strcmp(type, valueTypeKey(valueType)) == 0) {
                mapping.valueType = valueType;
                knownType = true;
                break;
            }
        }
        if (!knownType) return false;
        if (strcmp(order, "high") == 0) mapping.wordOrder = WordOrder::HighFirst;
        else if (strcmp(order, "low") == 0) mapping.wordOrder = WordOrder::LowFirst;
        else return false;
        if (mapping.unit > 247 || !isfinite(mapping.scale) || mapping.scale <= 0 ||
            uint32_t(mapping.registerAddress) + mapping.words() > 65536) return false;
    }
    preset = candidate;
    error = nullptr;
    return true;
}

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
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".json") != 0) continue;
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
        if (text.length() != expected || !parseDevicePresetJson(text.c_str(), text.length(), preset, error)) {
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
