#ifndef ESP_SOLAR_BOY_PRESET_TOML_H
#define ESP_SOLAR_BOY_PRESET_TOML_H

#include "InverterConfig.h"

namespace preset_toml {

enum HeaderKey { Id, Name, RequiresOverrides, HeaderKeyCount };
enum MappingKey { Register, Unit, Type, WordOrderKey, Scale, Invert, IgnoreNoData, MappingKeyCount };
constexpr unsigned ALL_TABLES = (1u << TELEMETRY_COUNT) - 1;
constexpr unsigned ALL_HEADER_KEYS = (1u << HeaderKeyCount) - 1;
constexpr unsigned ALL_MAPPING_KEYS = (1u << MappingKeyCount) - 1;

inline char *trim(char *text) {
    while (*text == ' ' || *text == '\t') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
    return text;
}

inline bool parseString(const char *text, char *output, size_t size) {
    const char quote = *text++;
    if (quote != '\'' && quote != '"') return false;
    size_t used = 0;
    while (*text && *text != quote) {
        char value = *text++;
        if (value == '\\' && quote == '"') {
            switch (*text++) {
                case '\\': value = '\\'; break;
                case '"': value = '"'; break;
                case 'b': value = '\b'; break;
                case 't': value = '\t'; break;
                case 'n': value = '\n'; break;
                case 'f': value = '\f'; break;
                case 'r': value = '\r'; break;
                default: return false;
            }
        } else if ((static_cast<unsigned char>(value) < 0x20 && value != '\t') || value == 0x7f) return false;
        if (used + 1 >= size) return false;
        output[used++] = value;
    }
    if (*text != quote || text[1] != '\0') return false;
    output[used] = '\0';
    return true;
}

inline bool parseBoolean(const char *text, bool &value) {
    if (strcmp(text, "true") == 0) value = true;
    else if (strcmp(text, "false") == 0) value = false;
    else return false;
    return true;
}

// Decimal TOML numbers, including exponent notation and digit separators.
inline bool parseNumber(const char *text, char (&output)[96]) {
    size_t used = 0;
    const auto append = [&](char c) -> bool {
        if (used + 1 >= sizeof(output)) return false;
        output[used++] = c;
        return true;
    };
    if (*text == '+' || *text == '-') { if (!append(*text++)) return false; }
    const auto digits = [&]() -> bool {
        if (*text < '0' || *text > '9') return false;
        do {
            if (!append(*text++)) return false;
            if (*text == '_') {
                ++text;
                if (*text < '0' || *text > '9') return false;
            }
        } while (*text >= '0' && *text <= '9');
        return true;
    };
    const size_t start = used;
    const bool zero = *text == '0';
    if (!digits() || (zero && used - start > 1)) return false;
    if (*text == '.') { if (!append(*text++) || !digits()) return false; }
    if (*text == 'e' || *text == 'E') {
        if (!append(*text++)) return false;
        if (*text == '+' || *text == '-') { if (!append(*text++)) return false; }
        if (!digits()) return false;
    }
    output[used] = '\0';
    return *text == '\0';
}

inline bool unsignedNumber(const char *text, uint32_t max, uint32_t &value) {
    char normalized[96];
    if (!parseNumber(text, normalized)) return false;
    return parseUnsigned(normalized[0] == '+' ? normalized + 1 : normalized, max, value);
}

inline bool validId(const char *text) {
    if (!*text || strlen(text) >= 32) return false;
    for (; *text; ++text)
        if (!((*text >= 'a' && *text <= 'z') || (*text >= '0' && *text <= '9') || *text == '-' || *text == '_')) return false;
    return true;
}

} // namespace preset_toml

// The preset schema uses single-line strings, decimal numbers, booleans and the
// four named tables. Other TOML constructs are rejected rather than guessed.
inline bool parseDevicePresetToml(const char *text, size_t length, DevicePreset &preset, const char *&error) {
    using namespace preset_toml;
    DevicePreset candidate;
    uint16_t seen[TELEMETRY_COUNT] = {};
    uint8_t tables = 0, header = 0;
    int section = -1;
    error = "Invalid or unsupported TOML syntax";
    for (size_t offset = 0; offset < length;) {
        char buffer[256];
        size_t used = 0;
        while (offset < length && text[offset] != '\n') {
            if (used + 1 >= sizeof(buffer) || text[offset] == '\0') return false;
            buffer[used++] = text[offset++];
        }
        if (offset < length) ++offset;
        if (used && buffer[used - 1] == '\r') --used;
        buffer[used] = '\0';
        char quote = 0;
        bool escaped = false;
        for (char *p = buffer; *p; ++p) {
            if (escaped) { escaped = false; continue; }
            if (quote == '"' && *p == '\\') { escaped = true; continue; }
            if (quote && *p == quote) quote = 0;
            else if (!quote && (*p == '\'' || *p == '"')) quote = *p;
            else if (!quote && *p == '#') { *p = '\0'; break; }
        }
        if (quote || escaped) return false;
        char *line = trim(buffer);
        if (!*line) continue;
        if (*line == '[') {
            const size_t size = strlen(line);
            if (size < 3 || line[size - 1] != ']') return false;
            line[size - 1] = '\0';
            const char *name = trim(line + 1);
            section = -1;
            for (size_t i = 0; i < TELEMETRY_COUNT; ++i) if (strcmp(name, telemetryKey(i)) == 0) section = i;
            if (section < 0) { error = "Unknown telemetry table"; return false; }
            if (tables & (1 << section)) { error = "Duplicate telemetry table"; return false; }
            tables |= 1 << section;
            continue;
        }
        char *equals = strchr(line, '=');
        if (!equals) return false;
        *equals = '\0';
        const char *key = trim(line);
        const char *value = trim(equals + 1);
        char decoded[96];
        uint32_t numberValue;
        if (section < 0) {
            static const char *const keys[] = {"id", "name", "requires_overrides"};
            int index = -1;
            for (size_t i = 0; i < HeaderKeyCount; ++i) if (strcmp(keys[i], key) == 0) index = i;
            if (index < 0 || (header & (1 << index))) { error = "Unknown or duplicate preset key"; return false; }
            header |= 1 << index;
            switch (index) {
                case Id:
                    if (!parseString(value, decoded, sizeof(decoded)) || !validId(decoded)) return false;
                    candidate.id = PresetId(decoded);
                    break;
                case Name:
                    if (!parseString(value, candidate.label, sizeof(candidate.label)) || !*candidate.label) return false;
                    break;
                case RequiresOverrides: if (!parseBoolean(value, candidate.requiresOverrides)) return false; break;
            }
            continue;
        }
        static const char *const keys[] = {"register", "unit", "type", "word_order", "scale", "invert", "ignore_no_data"};
        int index = -1;
        for (size_t i = 0; i < MappingKeyCount; ++i) if (strcmp(keys[i], key) == 0) index = i;
        if (index < 0 || (seen[section] & (1 << index))) { error = "Unknown or duplicate telemetry key"; return false; }
        seen[section] |= 1 << index;
        auto &mapping = candidate.telemetry[section];
        if (index == Type || index == WordOrderKey) {
            if (!parseString(value, decoded, sizeof(decoded))) return false;
        }
        switch (index) {
            case Register:
                if (!unsignedNumber(value, 65535, numberValue)) return false;
                mapping.registerAddress = numberValue;
                break;
            case Unit:
                if (!unsignedNumber(value, 247, numberValue)) return false;
                mapping.unit = numberValue;
                break;
            case Type: {
                bool found = false;
                for (auto type : {ValueType::U16, ValueType::I16, ValueType::U32, ValueType::I32, ValueType::Float32})
                    if (strcmp(decoded, valueTypeKey(type)) == 0) { mapping.valueType = type; found = true; }
                if (!found) return false;
                break;
            }
            case WordOrderKey:
                if (strcmp(decoded, "high") == 0) mapping.wordOrder = WordOrder::HighFirst;
                else if (strcmp(decoded, "low") == 0) mapping.wordOrder = WordOrder::LowFirst;
                else return false;
                break;
            case Scale: {
                char normalized[96], *end;
                if (!parseNumber(value, normalized)) return false;
                errno = 0;
                mapping.scale = strtod(normalized, &end);
                if (*end || errno == ERANGE || !isfinite(mapping.scale) || mapping.scale <= 0) return false;
                break;
            }
            case Invert: if (!parseBoolean(value, mapping.invert)) return false; break;
            case IgnoreNoData: if (!parseBoolean(value, mapping.rejectUnavailable)) return false; break;
        }
    }
    error = "Incomplete preset or telemetry tables";
    if (header != ALL_HEADER_KEYS || tables != ALL_TABLES) return false;
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        if (seen[i] != ALL_MAPPING_KEYS) return false;
        if (uint32_t(candidate.telemetry[i].registerAddress) + candidate.telemetry[i].words() > 65536) {
            error = "Register range exceeds 65535";
            return false;
        }
    }
    preset = candidate;
    error = nullptr;
    return true;
}

#endif
