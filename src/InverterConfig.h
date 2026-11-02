#ifndef ESP_SOLAR_BOY_INVERTER_CONFIG_H
#define ESP_SOLAR_BOY_INVERTER_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <initializer_list>
#include <vector>

enum class ValueType : uint8_t { U16, I16, U32, I32, Float32 };
enum class WordOrder : uint8_t { HighFirst, LowFirst };
class PresetId {
    char _key[32] = {};
public:
    PresetId(const char *key = "sun2000") {
        if (key && strlen(key) < sizeof(_key)) strcpy(_key, key);
    }
    const char *c_str() const { return _key; }
    bool operator==(const PresetId &other) const { return strcmp(_key, other._key) == 0; }
    bool operator!=(const PresetId &other) const { return !(*this == other); }
};
constexpr size_t TELEMETRY_COUNT = 4;

inline const char *telemetryKey(size_t index) {
    static const char *keys[] = {"soc", "battery", "plant", "grid"};
    return keys[index];
}

inline const char *telemetryLabel(size_t index) {
    static const char *labels[] = {"Battery state of charge", "Battery charge power", "Solar plant power", "Grid power"};
    return labels[index];
}

struct TelemetryConfig {
    uint16_t registerAddress;
    uint8_t unit;
    ValueType valueType;
    WordOrder wordOrder;
    double scale;
    bool rejectUnavailable;
    bool invert;

    constexpr TelemetryConfig(uint16_t address = 0, uint8_t modbusUnit = 0, ValueType type = ValueType::I32, double multiplier = 1,
        WordOrder order = WordOrder::HighFirst,
        bool unavailable = false, bool invertSign = false)
        : registerAddress(address), unit(modbusUnit),
          valueType(type), wordOrder(order), scale(multiplier), rejectUnavailable(unavailable), invert(invertSign) {}

    uint16_t words() const { return valueType == ValueType::U16 || valueType == ValueType::I16 ? 1 : 2; }
};

struct DevicePreset {
    PresetId id = PresetId("");
    char label[96] = {};
    bool requiresOverrides = true;
    TelemetryConfig telemetry[TELEMETRY_COUNT];
};

// Loaded once from LittleFS before HTTP handlers or polling start.
inline std::vector<DevicePreset> &devicePresetRegistry() {
    static std::vector<DevicePreset> presets;
    return presets;
}

inline const DevicePreset *findDevicePreset(const char *key) {
    if (!key) return nullptr;
    for (const auto &preset : devicePresetRegistry())
        if (strcmp(preset.id.c_str(), key) == 0) return &preset;
    return nullptr;
}

inline const DevicePreset &devicePreset(const PresetId &id) {
    if (const auto *preset = findDevicePreset(id.c_str())) return *preset;
    // Missing files never silently select a different device's mappings.
    static const DevicePreset unavailable;
    return unavailable;
}

struct InverterConfig {
    uint8_t ip[4] = {0, 0, 0, 0};
    uint16_t port = 502;
    PresetId preset;
    bool overrideTelemetry[TELEMETRY_COUNT] = {};
    TelemetryConfig telemetryOverride[TELEMETRY_COUNT];

    TelemetryConfig telemetry(size_t index) const {
        return overrideTelemetry[index] ? telemetryOverride[index] : devicePreset(preset).telemetry[index];
    }
    bool configured() const {
        const auto *selected = findDevicePreset(preset.c_str());
        if (!selected) return false;
        if (!selected->requiresOverrides) return true;
        for (bool overridden : overrideTelemetry) if (!overridden) return false;
        return true;
    }
};

inline bool applyDevicePreset(InverterConfig &config, const PresetId &id) {
    const auto *selected = findDevicePreset(id.c_str());
    if (!selected) return false;
    const auto &preset = *selected;
    config.preset = id;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        config.overrideTelemetry[i] = false;
        config.telemetryOverride[i] = preset.telemetry[i];
    }
    return true;
}

inline bool parseUnsigned(const char *text, uint32_t max, uint32_t &value) {
    if (!text || !*text) return false;
    char *end;
    errno = 0;
    const auto parsed = strtoul(text, &end, 10);
    if (*end || errno == ERANGE || parsed > max) return false;
    value = parsed;
    return true;
}

inline bool parseIPv4(const char *text, uint8_t (&ip)[4]) {
    if (!text) return false;
    uint8_t parsed[4];
    for (size_t i = 0; i < 4; ++i) {
        const char *start = text;
        unsigned value = 0;
        while (*text >= '0' && *text <= '9') {
            value = value * 10 + (*text++ - '0');
            if (value > 255 || text - start > 3) return false;
        }
        if (start == text || *text != (i == 3 ? '\0' : '.')) return false;
        parsed[i] = value;
        if (i != 3) ++text;
    }
    memcpy(ip, parsed, sizeof(parsed));
    return true;
}

inline const char *valueTypeKey(ValueType type) {
    static const char *keys[] = {"u16", "i16", "u32", "i32", "float32"};
    return keys[static_cast<size_t>(type)];
}

inline TelemetryConfig readTelemetryJson(JsonObjectConst object) {
    TelemetryConfig mapping;
    mapping.registerAddress = object["register"].as<uint16_t>();
    mapping.unit = object["unit"].as<uint8_t>();
    mapping.scale = object["scale"].as<double>();
    mapping.invert = object["invert"].as<bool>();
    mapping.rejectUnavailable = object["ignore_no_data"].as<bool>();
    mapping.wordOrder = object["word_order"] == "low" ? WordOrder::LowFirst : WordOrder::HighFirst;
    for (auto type : {ValueType::U16, ValueType::I16, ValueType::U32, ValueType::I32, ValueType::Float32})
        if (object["type"] == valueTypeKey(type)) mapping.valueType = type;
    return mapping;
}

inline void writeTelemetryJson(JsonObject object, const TelemetryConfig &mapping) {
    object["register"] = mapping.registerAddress;
    object["unit"] = mapping.unit;
    object["type"] = valueTypeKey(mapping.valueType);
    object["word_order"] = mapping.wordOrder == WordOrder::LowFirst ? "low" : "high";
    char scale[32];
    snprintf(scale, sizeof(scale), "%.17g", mapping.scale);
    object["scale"] = serialized(scale);
    object["invert"] = mapping.invert;
    object["ignore_no_data"] = mapping.rejectUnavailable;
}

struct TelemetryForm {
    const char *overrideValue = nullptr;
    const char *address = nullptr;
    const char *unit = nullptr;
    const char *valueType = nullptr;
    const char *wordOrder = nullptr;
    const char *scale = nullptr;
    const char *rejectUnavailable = nullptr;
    const char *invert = nullptr;
};

inline bool parseTelemetryConfig(const TelemetryForm &form, TelemetryConfig &config) {
    uint32_t address, unit;
    if (!parseUnsigned(form.address, 65535, address) || !parseUnsigned(form.unit, 247, unit) ||
        !form.valueType || !form.wordOrder || !form.scale || !*form.scale) return false;
    TelemetryConfig parsed;
    bool validType = false;
    for (auto type : {ValueType::U16, ValueType::I16, ValueType::U32, ValueType::I32, ValueType::Float32}) {
        if (strcmp(form.valueType, valueTypeKey(type)) == 0) { parsed.valueType = type; validType = true; }
    }
    if (!validType || address + parsed.words() > 65536) return false;
    if (strcmp(form.wordOrder, "high") == 0) parsed.wordOrder = WordOrder::HighFirst;
    else if (strcmp(form.wordOrder, "low") == 0) parsed.wordOrder = WordOrder::LowFirst;
    else return false;
    char *end;
    errno = 0;
    parsed.scale = strtod(form.scale, &end);
    if (end == form.scale || *end || errno == ERANGE || !isfinite(parsed.scale) || parsed.scale <= 0) return false;
    parsed.rejectUnavailable = form.rejectUnavailable != nullptr;
    parsed.invert = form.invert != nullptr;
    parsed.registerAddress = address;
    parsed.unit = unit;
    config = parsed;
    return true;
}

struct InverterForm {
    const char *ip = nullptr;
    const char *port = nullptr;
    const char *preset = nullptr;
    TelemetryForm telemetry[TELEMETRY_COUNT];
};

inline const char *parseInverterForm(const InverterForm &form, InverterConfig &config) {
    InverterConfig candidate = config;
    uint32_t port;
    if (!parseIPv4(form.ip, candidate.ip)) return "Enter a valid IPv4 address.";
    if (!parseUnsigned(form.port, 65535, port) || port == 0) return "Port must be 1 to 65535.";
    candidate.port = port;
    const auto *preset = findDevicePreset(form.preset);
    if (!preset) return "Select a valid device preset.";
    candidate.preset = preset->id;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto &submitted = form.telemetry[i];
        candidate.overrideTelemetry[i] = submitted.overrideValue != nullptr;
        candidate.telemetryOverride[i] = preset->telemetry[i];
        if (candidate.overrideTelemetry[i] && !parseTelemetryConfig(submitted, candidate.telemetryOverride[i]))
            return "Invalid telemetry mapping. Check register range, unit, format, word order and positive finite scale.";
    }
    config = candidate;
    return nullptr;
}

inline bool decodeTelemetry(const uint16_t (&words)[2], const TelemetryConfig &config, int32_t &value) {
    const uint16_t high = words[config.wordOrder == WordOrder::HighFirst ? 0 : 1];
    const uint16_t low = words[config.wordOrder == WordOrder::HighFirst ? 1 : 0];
    const uint32_t bits = config.words() == 1 ? words[0] : (uint32_t(high) << 16) | low;
    double raw;
    uint32_t unavailable;
    switch (config.valueType) {
        case ValueType::U16: raw = bits; unavailable = UINT16_MAX; break;
        case ValueType::I16: raw = bits & 0x8000 ? int64_t(bits) - 0x10000 : bits; unavailable = INT16_MAX; break;
        case ValueType::U32: raw = bits; unavailable = UINT32_MAX; break;
        case ValueType::I32: raw = bits & 0x80000000UL ? int64_t(bits) - 0x100000000LL : bits; unavailable = INT32_MAX; break;
        case ValueType::Float32: {
            float number;
            static_assert(sizeof(number) == sizeof(bits), "Float32 requires 32-bit floats");
            memcpy(&number, &bits, sizeof(number));
            raw = number;
            unavailable = 0;
            break;
        }
        default: return false;
    }
    if (config.valueType != ValueType::Float32 && config.rejectUnavailable && bits == unavailable) return false;
    const double scaled = raw * config.scale * (config.invert ? -1 : 1);
    if (!isfinite(scaled) || scaled < INT32_MIN || scaled > INT32_MAX) return false;
    value = static_cast<int32_t>(scaled);
    return true;
}

inline bool sameInverterSource(const InverterConfig &a, const InverterConfig &b) {
    if (memcmp(a.ip, b.ip, sizeof(a.ip)) || a.port != b.port ||
        a.configured() != b.configured()) return false;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto x = a.telemetry(i), y = b.telemetry(i);
        if (x.registerAddress != y.registerAddress || x.unit != y.unit ||
            x.valueType != y.valueType || x.wordOrder != y.wordOrder ||
            x.scale != y.scale || x.invert != y.invert || x.rejectUnavailable != y.rejectUnavailable) return false;
    }
    return true;
}

inline String encodeInverterConfig(const InverterConfig &config) {
    DynamicJsonDocument document(2048);
    char ip[16];
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
    document["ip"] = ip;
    document["port"] = config.port;
    document["preset"] = config.preset.c_str();
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        if (config.overrideTelemetry[i])
            writeTelemetryJson(document["overrides"][telemetryKey(i)].to<JsonObject>(), config.telemetry(i));
    }
    String record;
    serializeJson(document, record);
    return record;
}

inline bool decodeInverterConfig(const char *record, InverterConfig &config) {
    DynamicJsonDocument document(2048);
    if (deserializeJson(document, record)) return false;
    InverterConfig parsed;
    parseIPv4(document["ip"] | "0.0.0.0", parsed.ip);
    parsed.port = document["port"].as<uint16_t>();
    parsed.preset = PresetId(document["preset"].as<const char *>());
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto object = document["overrides"][telemetryKey(i)].as<JsonObjectConst>();
        parsed.overrideTelemetry[i] = !object.isNull();
        if (parsed.overrideTelemetry[i]) parsed.telemetryOverride[i] = readTelemetryJson(object);
    }
    config = parsed;
    return true;
}

#endif
