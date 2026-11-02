#pragma once
#include "Arduino.h"
#include <netinet/in.h>
#include <cstring>
class IPAddress {
public:
    uint8_t bytes[4] = {};
    IPAddress() = default;
    explicit IPAddress(const uint8_t *ip) { memcpy(bytes, ip, 4); }
    String toString() const {
        return String(bytes[0]) + "." + String(bytes[1]) + "." + String(bytes[2]) + "." + String(bytes[3]);
    }
};
