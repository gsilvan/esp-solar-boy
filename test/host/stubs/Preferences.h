#pragma once
#include "Arduino.h"

class Preferences {
    std::string prefix;
public:
    static std::map<std::string, String> values;
    static bool failWrites;
    bool begin(const char *name, bool = false, const char * = nullptr) { prefix = std::string(name) + "/"; return true; }
    String getString(const char *key, String fallback = "") {
        const auto it = values.find(prefix + key);
        return it == values.end() ? fallback : it->second;
    }
    size_t putString(const char *key, const String &value) {
        if (failWrites) return 0;
        values[prefix + key] = value;
        return value.length();
    }
    int32_t getInt(const char *key, int32_t fallback) {
        const auto value = getString(key);
        return value.empty() ? fallback : value.toInt();
    }
    uint16_t getUShort(const char *key, uint16_t fallback) { return getInt(key, fallback); }
    uint8_t getUChar(const char *key, uint8_t fallback) { return getInt(key, fallback); }
    uint32_t getUInt(const char *key, uint32_t fallback) { return getInt(key, fallback); }
    bool getBool(const char *key, bool fallback) { return getInt(key, fallback); }
    size_t putInt(const char *key, int32_t value) { return putString(key, String(value)); }
    size_t putUShort(const char *key, uint16_t value) { return putInt(key, value); }
    size_t putUChar(const char *key, uint8_t value) { return putInt(key, value); }
    size_t putUInt(const char *key, uint32_t value) { return putString(key, String(value)); }
    size_t putBool(const char *key, bool value) { return putInt(key, value); }
};
