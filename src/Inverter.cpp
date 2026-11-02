#include <numeric>
#include "Inverter.h"

Inverter::Inverter() :
        _powerMeterActivePowerHistory(DEQUE_SIZE, 0),
        _batteryStateOfChargeHistory(DEQUE_SIZE, 0),
        _plantPowerHistory(DEQUE_SIZE, 0) {
    this->_modbus.client();
}

void Inverter::begin() {
#ifdef ESP32
    this->_preferences.begin("inverter", false, "nvs");
#else
    this->_preferences.begin("inverter", false);
#endif
    // Old installations have no device marker and retain SUN2000 defaults.
    parseIPv4(this->_preferences.getString("ip", "").c_str(), this->_config.ip);
    this->_config.port = this->_preferences.getUShort("port", 502);
    const String record = this->_preferences.getString("settings", "");
    decodeInverterConfig(record.c_str(), this->_config);
    this->ipAddress = IPAddress(this->_config.ip);
    this->port = this->_config.port;
    this->_initialized = true;
}

Inverter::~Inverter() = default;

void Inverter::_lockConfig() const {
#ifdef ESP32
    portENTER_CRITICAL(&this->_configMux);
#endif
}

void Inverter::_unlockConfig() const {
#ifdef ESP32
    portEXIT_CRITICAL(&this->_configMux);
#endif
}

InverterConfig Inverter::getConfig() const {
    this->_lockConfig();
    const auto config = this->_hasPendingConfig ? this->_pendingConfig : this->_config;
    this->_unlockConfig();
    return config;
}

Inverter::SaveResult Inverter::saveConfig(const InverterConfig &config) {
    char record[INVERTER_CONFIG_RECORD_SIZE];
    if (!encodeInverterConfig(config, record, sizeof(record))) {
        return SaveResult::StorageError;
    }
    this->_lockConfig();
    if (this->_hasPendingConfig || this->_savingConfig) {
        this->_unlockConfig();
        return SaveResult::Busy;
    }
    this->_savingConfig = true;
    this->_unlockConfig();

    const bool saved = this->_preferences.putString("settings", record) == strlen(record);
    this->_lockConfig();
    if (saved) {
        this->_pendingConfig = config;
        this->_hasPendingConfig = true;
    }
    this->_savingConfig = false;
    this->_unlockConfig();
    return saved ? SaveResult::Saved : SaveResult::StorageError;
}

bool Inverter::hasValidReadings() const {
    return this->isConnected && this->_readingsReady;
}

void Inverter::_resetReadings() {
    this->_readingsReady = false;
    this->_batteryStateOfCharge = 0;
    this->_batteryChargePower = 0;
    this->_plantPower = 0;
    this->_gridPower = 0;
    this->_batteryStateOfChargeHistory.assign(this->DEQUE_SIZE, 0);
    this->_plantPowerHistory.assign(this->DEQUE_SIZE, 0);
    this->_powerMeterActivePowerHistory.assign(this->DEQUE_SIZE, 0);
    this->_lastHistoryUpdate = millis();
}

void Inverter::_applyPendingConfig() {
    this->_lockConfig();
    if (!this->_hasPendingConfig) {
        this->_unlockConfig();
        return;
    }
    const bool changed = !sameInverterSource(this->_config, this->_pendingConfig);
    this->_config = this->_pendingConfig;
    this->_hasPendingConfig = false;
    this->_unlockConfig();
    if (!changed) return;

    // Cancel and drain old transactions before reusing their destination buffers.
    ++this->_generation;
    this->_modbus.disconnect(this->ipAddress);
    this->_modbus.dropTransactions();
    this->_modbus.task();
    this->_polling = false;
    this->_pendingReads = 0;
    this->isConnected = false;
    this->_resetReadings();
    this->ipAddress = IPAddress(this->_config.ip);
    this->port = this->_config.port;
    this->_lastUpdate = millis() - this->INVERTER_UPDATE_INTERVAL;
}

void Inverter::_readCompleted(Modbus::ResultCode event, uint32_t generation) {
    if (generation != this->_generation) return;
    if (event != Modbus::EX_SUCCESS) {
        this->_pollFailed = true;
        if (this->_readingsReady) this->_resetReadings();
        Serial.printf("Modbus error: %02X\n", event);
    }
    if (event == Modbus::EX_TIMEOUT) {
        this->_modbus.disconnect(this->ipAddress);
        this->_modbus.dropTransactions();
    }
    if (this->_pendingReads) --this->_pendingReads;
}

void Inverter::_startPoll() {
    this->_polling = true;
    this->_pollFailed = false;
    this->_pendingReads = TELEMETRY_COUNT;
    const uint32_t generation = this->_generation;
    auto cb = [this, generation](Modbus::ResultCode event, uint16_t, void *) -> bool {
        this->_readCompleted(event, generation);
        return true;
    };
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto mapping = this->_config.telemetry(i);
        auto *raw = this->_telemetryRaw[i];
        const uint16_t transaction = this->_modbus.readHreg(this->ipAddress, mapping.registerAddress,
            raw, mapping.words(), cb, mapping.unit(this->_config.inverterUnit()));
        if (!transaction) this->_readCompleted(Modbus::EX_GENERAL_FAILURE, generation);
    }
}

bool Inverter::update() {
    if (!this->_initialized) return false;
    this->_applyPendingConfig();
    // Service replies and cancellations on every loop, rather than every 10 seconds.
    this->_modbus.task();
    this->isConnected = this->_modbus.isConnected(this->ipAddress);
    if (!this->isConnected) {
        if (this->_readingsReady) this->_resetReadings();
        this->_modbus.dropTransactions();
    }

    bool updated = false;
    if (this->_polling && this->_pendingReads == 0) {
        this->_polling = false;
        int32_t values[TELEMETRY_COUNT] = {};
        bool valid = !this->_pollFailed && this->isConnected;
        for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
            if (!decodeTelemetry(this->_telemetryRaw[i], this->_config.telemetry(i), values[i])) {
                valid = false;
                break;
            }
        }
        if (valid && values[0] >= 0 && values[0] <= 100) {
            this->_batteryStateOfCharge = values[0];
            this->_batteryChargePower = values[1];
            this->_plantPower = values[2];
            this->_gridPower = values[3];
            this->_readingsReady = true;
            this->printy();
            updated = true;
        } else {
            if (this->_readingsReady) this->_resetReadings();
        }
    }

    if (!this->_polling && millis() - this->_lastUpdate >= this->INVERTER_UPDATE_INTERVAL) {
        this->_lastUpdate = millis();
        if (!this->isConnected) {
            this->_modbus.connect(this->ipAddress, this->port);
            this->isConnected = this->_modbus.isConnected(this->ipAddress);
        }
        if (this->isConnected && this->_config.configured()) this->_startPoll();
    }
    if (this->hasValidReadings()) this->_updateHistory();
    return updated;
}

uint16_t Inverter::getBatteryStateOfCharge() const {
    return this->_batteryStateOfCharge;
}

int32_t Inverter::getBatteryChargePower() const {
    return this->_batteryChargePower;
}

int32_t Inverter::getPlantPower() const {
    return this->_plantPower;
}

void Inverter::printy() {
    Serial.printf("Battery: %d\n", this->getBatteryStateOfCharge());
    Serial.printf("Battery charge power: %d\n", this->getBatteryChargePower());
    Serial.printf("Plant power: %d\n", this->getPlantPower());
    Serial.printf("Power meter active power: %d\n", this->getPowerMeterActivePower());
    Inverter::_printDeque(&this->_powerMeterActivePowerHistory);
    Inverter::_printDeque(&this->_batteryStateOfChargeHistory);
}

int32_t Inverter::getPowerMeterActivePower() const {
    return this->_gridPower;
}

void Inverter::_addToDeque(int value, std::deque<int> *dq) const {
    if (dq->size() >= this->DEQUE_SIZE) {
        dq->pop_front();
    }
    dq->push_back(value);
}

void Inverter::_printDeque(std::deque<int> *dq) {
    if (!dq) {
        return;
    }
    for (auto val: *dq) {
        Serial.print(val);
        Serial.print(" ");
    }
    Serial.println();
}

void Inverter::_updateHistory() {
    if (millis() - this->_lastHistoryUpdate >= this->HISTORY_UPDATE_INTERVAL) {
        this->_addToDeque(this->getPowerMeterActivePower(), &this->_powerMeterActivePowerHistory);
        this->_addToDeque(this->getBatteryStateOfCharge(), &this->_batteryStateOfChargeHistory);
        this->_addToDeque(this->getPlantPower(), &this->_plantPowerHistory);
        this->_lastHistoryUpdate = millis();
    }
}

int Inverter::_minutesToN(unsigned int minutes) const {
    return 60000 * minutes / this->HISTORY_UPDATE_INTERVAL;
}

int Inverter::minBatteryStateOfCharge(int lastNMinutes) {
    if (this->_batteryStateOfChargeHistory.empty()) {
        return 0;
    }
    int indexOffset = this->_minutesToN(lastNMinutes);
    return *std::min_element(this->_batteryStateOfChargeHistory.end() - indexOffset, this->_batteryStateOfChargeHistory.end());
}

int Inverter::meanPowerMeterActivePower(int lastNMinutes) {
    if (this->_powerMeterActivePowerHistory.empty()) {
        return 0;
    }
    int indexOffset = this->_minutesToN(lastNMinutes);
    double input_sum = std::accumulate(this->_powerMeterActivePowerHistory.end() - indexOffset,
                                       this->_powerMeterActivePowerHistory.end(), int64_t(0));
    return (int) (input_sum / indexOffset);
}

int Inverter::meanPlantPower(int lastNMinutes) {
    if (this->_plantPowerHistory.empty()) {
        return 0;
    }
    int indexOffset = this->_minutesToN(lastNMinutes);
    double input_sum = std::accumulate(this->_plantPowerHistory.end() - indexOffset,
                                       this->_plantPowerHistory.end(), int64_t(0));
    return (int) (input_sum / indexOffset);
}
