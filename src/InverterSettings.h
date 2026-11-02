#ifndef ESP_SOLAR_BOY_INVERTER_SETTINGS_H
#define ESP_SOLAR_BOY_INVERTER_SETTINGS_H

#include <LittleFS.h>
#include "InverterConfig.h"
#include "template.h"

inline void previewTelemetry(const InverterForm &form, const char *reading, InverterConfig &config) {
    if (!form.preset) return;
    applyDevicePreset(config, PresetId(form.preset));
    if (!reading) return;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        if (strcmp(reading, telemetryKey(i)) != 0) continue;
        const auto &submitted = form.telemetry[i];
        config.overrideTelemetry[i] = submitted.overrideValue != nullptr;
        if (config.overrideTelemetry[i] && submitted.address)
            parseTelemetryConfig(submitted, config.telemetryOverride[i]);
        return;
    }
}

inline String telemetryScale(double value) {
    char buffer[32];
    // Show a short decimal while preserving the configured double on a form save.
    for (int precision = 1; precision <= 17; ++precision) {
        snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        if (strtod(buffer, nullptr) == value) break;
    }
    return String(buffer);
}

inline String telemetryRow(const InverterConfig &config, size_t index) {
    const auto mapping = config.telemetry(index);
    File file = LittleFS.open("/telemetry-row.html", "r");
    return processTemplate(file.readString(), {
        {"KEY", telemetryKey(index)},
        {"LABEL", telemetryLabel(index)},
        {"DISABLED", config.overrideTelemetry[index] ? "" : "disabled"},
        {"OVERRIDE_CHECKED", config.overrideTelemetry[index] ? "checked" : ""},
        {"REGISTER", String(mapping.registerAddress)},
        {"REGISTER_MAX", mapping.words() == 1 ? "65535" : "65534"},
        {"UNIT", String(mapping.unit)},
        {"U16_SELECTED", mapping.valueType == ValueType::U16 ? "selected" : ""},
        {"I16_SELECTED", mapping.valueType == ValueType::I16 ? "selected" : ""},
        {"U32_SELECTED", mapping.valueType == ValueType::U32 ? "selected" : ""},
        {"I32_SELECTED", mapping.valueType == ValueType::I32 ? "selected" : ""},
        {"FLOAT32_SELECTED", mapping.valueType == ValueType::Float32 ? "selected" : ""},
        {"HIGH_SELECTED", mapping.wordOrder == WordOrder::HighFirst ? "selected" : ""},
        {"LOW_SELECTED", mapping.wordOrder == WordOrder::LowFirst ? "selected" : ""},
        {"SCALE", telemetryScale(mapping.scale)},
        {"INVERT_CHECKED", mapping.invert ? "checked" : ""},
        {"UNAVAILABLE_CHECKED", mapping.rejectUnavailable ? "checked" : ""},
    });
}

inline String inverterSettingsValue(const String &key, const InverterConfig &config) {
    if (key == "INVERTER_IP") return IPAddress(config.ip).toString();
    if (key == "INVERTER_PORT") return String(config.port);
    if (key == "GENERIC_HIDDEN") return devicePreset(config.preset).requiresOverrides ? "" : "hidden";
    static const char *const fields[] = {"SOC_FIELDS", "BATTERY_FIELDS", "PLANT_FIELDS", "GRID_FIELDS"};
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i)
        if (key == fields[i]) return telemetryRow(config, i);
    if (key == "DEVICE_PRESETS") {
        File file = LittleFS.open("/preset-option.html", "r");
        const String optionTemplate = file.readString();
        String options;
        for (const auto &preset : devicePresetRegistry()) {
            options += processTemplate(optionTemplate, {
                {"VALUE", escapeHtml(preset.id.c_str())},
                {"LABEL", escapeHtml(preset.label)},
                {"SELECTED", preset.id == config.preset ? "selected" : ""},
            });
        }
        return options;
    }
    return String();
}

#endif
