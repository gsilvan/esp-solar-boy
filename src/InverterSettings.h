#ifndef ESP_SOLAR_BOY_INVERTER_SETTINGS_H
#define ESP_SOLAR_BOY_INVERTER_SETTINGS_H

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

inline String telemetrySelect(const String &id, const char *label, const char *const *values,
                               const char *const *labels, size_t count, const char *selected, bool disabled, const String &attributes = "") {
    String html = "<div class=\"field\"><label class=\"visually-hidden\" for=\"" + id + "\">" + label + ":</label>";
    html += "<select class=\"input\" id=\"" + id + "\" name=\"" + id + "\"" + (disabled ? " disabled" : "") + attributes + ">";
    for (size_t i = 0; i < count; ++i) {
        html += "<option value=\"" + String(values[i]) + "\"";
        if (strcmp(values[i], selected) == 0) html += " selected";
        html += ">" + String(labels[i]) + "</option>";
    }
    return html + "</select></div>";
}

inline String telemetryInput(const String &id, const char *label, const String &value, const char *limits, bool disabled) {
    return "<div class=\"field\"><label class=\"visually-hidden\" for=\"" + id + "\">" + label + ":</label>" +
        "<input class=\"input\"\n"
        "       type=\"number\"\n"
        "       required\n"
        "       id=\"" + id + "\"\n"
        "       name=\"" + id + "\"\n"
        "       value=\"" + value + "\"\n       " + limits + (disabled ? "\n       disabled" : "") + "></div>";
}

inline String telemetryCheckbox(const String &id, const char *label, bool checked, bool disabled, const String &attributes = "") {
    return "<input type=\"checkbox\"\n"
        "       id=\"" + id + "\"\n"
        "       name=\"" + id + "\"\n"
        "       value=\"1\"" + (checked ? "\n       checked" : "") + (disabled ? "\n       disabled" : "") + attributes + ">" +
        "<label class=\"visually-hidden\" for=\"" + id + "\">" + label + "</label>";
}

inline String telemetryRow(const InverterConfig &config, size_t index) {
    static const char *const types[] = {"u16", "i16", "u32", "i32", "float32"};
    static const char *const typeLabels[] = {"Unsigned 16-bit", "Signed 16-bit", "Unsigned 32-bit", "Signed 32-bit", "Float 32-bit"};
    static const char *const orders[] = {"high", "low"};
    static const char *const orderLabels[] = {"High word first", "Low word first"};
    const auto mapping = config.telemetry(index);
    const bool disabled = !config.overrideTelemetry[index];
    const String prefix = "settings-" + String(telemetryKey(index)) + "-";
    const String rowId = "telemetry-" + String(telemetryKey(index));
    const String refresh = " hx-get=\"/settings?reading=" + String(telemetryKey(index)) +
        "\" hx-trigger=\"change\" hx-target=\"closest tr\" hx-select=\"#" + rowId +
        "\" hx-swap=\"outerHTML\" hx-include=\"#settings-device-preset, closest tr\"";
    String html = "<tr id=\"" + rowId + "\"><th scope=\"row\">" + String(telemetryLabel(index)) + "</th><td class=\"telemetry-checkbox\">";
    html += telemetryCheckbox(prefix + "override", "Override preset mapping", !disabled, false, refresh);
    html += "</td><td>";
    html += telemetryInput(prefix + "register", "Register address", String(mapping.registerAddress),
        mapping.words() == 1 ? "min=\"0\"\n       max=\"65535\"" : "min=\"0\"\n       max=\"65534\"", disabled);
    html += "</td><td>";
    html += telemetryInput(prefix + "unit", "Modbus unit", String(mapping.unit), "min=\"0\"\n       max=\"247\"", disabled);
    html += "</td><td>";
    html += telemetrySelect(prefix + "type", "Value format", types, typeLabels, 5, valueTypeKey(mapping.valueType), disabled, refresh);
    html += telemetrySelect(prefix + "order", "Word order (32-bit values)", orders, orderLabels, 2, mapping.wordOrder == WordOrder::LowFirst ? "low" : "high", disabled);
    html += "</td><td>";
    html += telemetryInput(prefix + "scale", "Scale multiplier", telemetryScale(mapping.scale), "min=\"0\"\n       step=\"any\"", disabled);
    html += "</td><td class=\"telemetry-checkbox\">";
    html += telemetryCheckbox(prefix + "invert", "Invert sign", mapping.invert, disabled);
    html += "</td><td class=\"telemetry-checkbox\">";
    html += telemetryCheckbox(prefix + "unavailable", "Ignore Huawei 'no data' values", mapping.rejectUnavailable, disabled);
    return html + "</td></tr>";
}

inline String inverterSettingsValue(const String &key, const InverterConfig &config) {
    if (key == "INVERTER_IP") return IPAddress(config.ip).toString();
    if (key == "INVERTER_PORT") return String(config.port);
    if (key == "GENERIC_HIDDEN") return devicePreset(config.preset).requiresOverrides ? "" : "hidden";
    static const char *const fields[] = {"SOC_FIELDS", "BATTERY_FIELDS", "PLANT_FIELDS", "GRID_FIELDS"};
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i)
        if (key == fields[i]) return telemetryRow(config, i);
    if (key == "DEVICE_PRESETS") {
        const auto &presets = devicePresetRegistry();
        String options;
        for (const auto &preset : presets) {
            options += "<option value=\"" + String(preset.id.c_str()) + "\"";
            options += preset.id == config.preset ? " selected>" : ">";
            options += escapeHtml(preset.label) + "</option>";
        }
        return options;
    }
    return String();
}

#endif
