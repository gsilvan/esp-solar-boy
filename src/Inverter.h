#ifndef ESP_SOLAR_BOY_INVERTER_H
#define ESP_SOLAR_BOY_INVERTER_H

#include <deque>
#include <Arduino.h>
#include <IPAddress.h>
#include <ModbusIP_ESP8266.h>
#include <Preferences.h>
#include "InverterConfig.h"

class Inverter {
public:
    Inverter();

    ~Inverter();

    void begin();

    bool update();

    [[nodiscard]] uint16_t getBatteryStateOfCharge() const;

    [[nodiscard]] int32_t getBatteryChargePower() const;

    [[nodiscard]] int32_t getPlantPower() const;

    [[nodiscard]] int32_t getPowerMeterActivePower() const;

    void printy();

    int minBatteryStateOfCharge(int lastNMinutes);

    int meanPowerMeterActivePower(int lastNMinutes);

    int meanPlantPower(int lastNMinutes);

    enum class SaveResult { Saved, Busy, StorageError };

    InverterConfig getConfig() const;

    SaveResult saveConfig(const InverterConfig &config);

    bool hasValidReadings() const;

    IPAddress ipAddress;
    in_port_t port = 502;
    bool isConnected = false;
    const uint16_t DEQUE_SIZE = 360;
    const uint64_t INVERTER_UPDATE_INTERVAL = 10000;
    const uint64_t HISTORY_UPDATE_INTERVAL = 10000;
private:
    ModbusIP _modbus;
    Preferences _preferences;
    bool _initialized = false;
    InverterConfig _config;
    InverterConfig _pendingConfig;
    bool _hasPendingConfig = false;
    bool _savingConfig = false;
#ifdef ESP32
    mutable portMUX_TYPE _configMux = portMUX_INITIALIZER_UNLOCKED;
#endif
    uint64_t _lastUpdate = 0;
    uint16_t _telemetryRaw[TELEMETRY_COUNT][2] = {};
    uint16_t _batteryStateOfCharge = 0;
    int32_t _batteryChargePower = 0;
    int32_t _plantPower = 0;
    int32_t _gridPower = 0;
    bool _readingsReady = false;
    bool _polling = false;
    bool _pollFailed = false;
    uint8_t _pendingReads = 0;
    uint32_t _generation = 0;
    uint64_t _lastHistoryUpdate = 0;
    std::deque<int> _powerMeterActivePowerHistory;
    std::deque<int> _batteryStateOfChargeHistory;
    std::deque<int> _plantPowerHistory;

    void _addToDeque(int value, std::deque<int> *dq) const;

    static void _printDeque(std::deque<int> *dq);

    void _updateHistory();

    int _minutesToN(unsigned int minutes) const;

    void _lockConfig() const;
    void _unlockConfig() const;
    void _applyPendingConfig();
    void _resetReadings();
    void _startPoll();
    void _readCompleted(Modbus::ResultCode event, uint32_t generation);
};

#endif //ESP_SOLAR_BOY_INVERTER_H
