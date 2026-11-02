#ifndef ESP_SOLAR_BOY_DEVICE_PRESETS_H
#define ESP_SOLAR_BOY_DEVICE_PRESETS_H

// Call once after mounting LittleFS and before Inverter::begin()/HTTP startup.
bool loadDevicePresets();

#endif
