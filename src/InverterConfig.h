#ifndef ESP_SOLAR_BOY_INVERTER_CONFIG_H
#define ESP_SOLAR_BOY_INVERTER_CONFIG_H

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
constexpr size_t INVERTER_CONFIG_RECORD_SIZE = 512;

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

inline const DevicePreset *devicePresets(size_t &count) {
    const auto &presets = devicePresetRegistry();
    count = presets.size();
    return presets.data();
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
    uint32_t result = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const uint32_t digit = *p - '0';
        if (digit > max || result > (max - digit) / 10) return false;
        result = result * 10 + digit;
    }
    value = result;
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
    for (const char *p = form.scale; *p; ++p)
        if (!strchr("0123456789+-.eE", *p)) return false;
    char *end;
    errno = 0;
    parsed.scale = strtod(form.scale, &end);
    if (end == form.scale || *end || errno == ERANGE || !isfinite(parsed.scale) || parsed.scale <= 0) return false;
    if (form.rejectUnavailable && strcmp(form.rejectUnavailable, "1") != 0) return false;
    if (form.invert && strcmp(form.invert, "1") != 0) return false;
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
    if (preset->id != candidate.preset) applyDevicePreset(candidate, preset->id);
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto &submitted = form.telemetry[i];
        if (submitted.overrideValue && strcmp(submitted.overrideValue, "1") != 0)
            return "Invalid telemetry override choice.";
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

inline bool validInverterConfig(const InverterConfig &config) {
    if (config.port == 0 || !findDevicePreset(config.preset.c_str())) return false;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto mapping = config.telemetry(i);
        if (mapping.unit > 247 ||
            mapping.valueType > ValueType::Float32 || mapping.wordOrder > WordOrder::LowFirst ||
            !isfinite(mapping.scale) || mapping.scale <= 0 || uint32_t(mapping.registerAddress) + mapping.words() > 65536) return false;
    }
    return true;
}

constexpr size_t CONFIG_HEADER_FIELDS = 4;
constexpr size_t CONFIG_MAPPING_FIELDS = 8;
constexpr size_t CONFIG_FIELD_COUNT = CONFIG_HEADER_FIELDS + TELEMETRY_COUNT * CONFIG_MAPPING_FIELDS;

inline bool encodeInverterConfig(const InverterConfig &config, char *record, size_t length) {
    if (!validInverterConfig(config)) return false;
    int used = snprintf(record, length, "3|%u.%u.%u.%u|%u|%s", config.ip[0], config.ip[1], config.ip[2],
        config.ip[3], config.port, config.preset.c_str());
    if (used < 0 || size_t(used) >= length) return false;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto mapping = config.telemetry(i);
        const int size = snprintf(record + used, length - used, "|%u|%u|%u|%s|%s|%.17g|%u|%u",
            config.overrideTelemetry[i] ? 1 : 0, mapping.registerAddress, mapping.unit,
            valueTypeKey(mapping.valueType), mapping.wordOrder == WordOrder::LowFirst ? "low" : "high",
            mapping.scale, mapping.rejectUnavailable ? 1 : 0, mapping.invert ? 1 : 0);
        if (size < 0 || size_t(size) >= length - used) return false;
        used += size;
    }
    return true;
}

inline bool decodeInverterConfig(const char *record, InverterConfig &config) {
    if (!record || strlen(record) >= INVERTER_CONFIG_RECORD_SIZE) return false;
    char buffer[INVERTER_CONFIG_RECORD_SIZE];
    strcpy(buffer, record);
    char *fields[CONFIG_FIELD_COUNT];
    size_t count = 1;
    fields[0] = buffer;
    while (char *separator = strchr(fields[count - 1], '|')) {
        if (count == CONFIG_FIELD_COUNT) return false;
        *separator = '\0';
        fields[count++] = separator + 1;
    }
    if (count != CONFIG_FIELD_COUNT || strcmp(fields[0], "3") != 0) return false;

    InverterConfig parsed;
    uint32_t port;
    const PresetId id(fields[3]);
    if (!*id.c_str() || strcmp(id.c_str(), fields[3]) || !parseIPv4(fields[1], parsed.ip) ||
        !parseUnsigned(fields[2], 65535, port) || port == 0) return false;
    for (const char *p = id.c_str(); *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return false;
    parsed.preset = id; // Keep unavailable selections so polling remains disabled.
    parsed.port = port;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto mappingFields = fields + CONFIG_HEADER_FIELDS + i * CONFIG_MAPPING_FIELDS;
        uint32_t overridden, unavailable, invert;
        if (!parseUnsigned(mappingFields[0], 1, overridden) ||
            !parseUnsigned(mappingFields[6], 1, unavailable) ||
            !parseUnsigned(mappingFields[7], 1, invert)) return false;
        TelemetryForm form;
        form.address = mappingFields[1];
        form.unit = mappingFields[2];
        form.valueType = mappingFields[3];
        form.wordOrder = mappingFields[4];
        form.scale = mappingFields[5];
        form.rejectUnavailable = unavailable ? "1" : nullptr;
        form.invert = invert ? "1" : nullptr;
        parsed.overrideTelemetry[i] = overridden != 0;
        if (!parseTelemetryConfig(form, parsed.telemetryOverride[i])) return false;
    }
    config = parsed;
    return true;
}

#endif
