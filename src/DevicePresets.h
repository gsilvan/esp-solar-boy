#ifndef ESP_SOLAR_BOY_DEVICE_PRESETS_H
#define ESP_SOLAR_BOY_DEVICE_PRESETS_H

#include "InverterConfig.h"

bool parseDevicePresetJson(const char *text, size_t length, DevicePreset &preset, const char *&error);

// Call once after mounting LittleFS and before Inverter::begin()/HTTP startup.
bool loadDevicePresets();

#endif
