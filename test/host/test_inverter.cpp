#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>
#include <cstdio>
#include <sys/stat.h>
#include "Inverter.h"
#include "DevicePresets.h"
#include "InverterSettings.h"
#include "SmartRelay.h"

uint32_t fakeMillis = 0;
FakeSerial Serial;
FakeLittleFS LittleFS;
std::map<uint8_t, int> pinValues;
std::map<std::string, String> Preferences::values;
bool Preferences::failWrites = false;
ModbusIP *ModbusIP::latest = nullptr;

static void reset() {
    fakeMillis = 10000;
    Preferences::values.clear();
    Preferences::failWrites = false;
}

static std::string record(const InverterConfig &config) {
    return encodeInverterConfig(config).c_str();
}

static void replyToPoll(ModbusIP &bus, int32_t gridPower, Modbus::ResultCode gridResult = Modbus::EX_SUCCESS) {
    assert(bus.requests.size() == 4);
    bus.reply(0, 950);
    bus.reply(1, -125);
    bus.reply(2, 1337);
    bus.reply(3, gridPower, gridResult);
}

static TelemetryForm mappingForm(const char *address = "31913", const char *type = "i32",
                                  const char *scale = "1", const char *invert = "1") {
    TelemetryForm form;
    form.overrideValue = "1"; form.address = address; form.unit = "0";
    form.valueType = type; form.wordOrder = "high"; form.scale = scale;
    form.rejectUnavailable = "1"; form.invert = invert;
    return form;
}

static InverterForm connectionForm() {
    InverterForm form;
    form.ip = "192.0.2.20"; form.port = "5020"; form.preset = "sun2000-emma";
    return form;
}

static void testConfigAndDecoding() {
    for (const auto &preset : devicePresetRegistry())
        for (const auto &mapping : preset.telemetry) assert(!mapping.rejectUnavailable);
    InverterConfig config;
    assert(config.telemetry(3).registerAddress == 37113 && config.telemetry(3).unit == 1);
    applyDevicePreset(config, PresetId("sun2000-emma"));
    assert(config.telemetry(3).registerAddress == 31657 && config.telemetry(3).unit == 0);
    assert(config.telemetry(3).invert);
    const uint16_t emmaRegisters[] = {37004, 37001, 32064};
    for (size_t i = 0; i < 3; ++i) {
        const auto mapping = config.telemetry(i);
        assert(mapping.registerAddress == emmaRegisters[i]);
        assert(mapping.unit == 2 && !mapping.invert);
        assert(mapping.wordOrder == WordOrder::HighFirst);
        assert(mapping.valueType == (i == 0 ? ValueType::U16 : ValueType::I32));
        assert(mapping.scale == (i == 0 ? 0.1 : 1));
    }
    auto equivalent = config;
    equivalent.overrideTelemetry[3] = true;
    equivalent.telemetryOverride[3] = config.telemetry(3);
    assert(sameInverterSource(config, equivalent));

    int32_t power = 123;
    TelemetryConfig format;
    for (int32_t value : {0, 1, -1, 500, -500, INT32_MIN, INT32_MAX - 1}) {
        uint16_t words[2] = {uint16_t(uint32_t(value) >> 16), uint16_t(value)};
        format.invert = false;
        assert(decodeTelemetry(words, format, power) && power == value);
        format.invert = true;
        const bool invertible = decodeTelemetry(words, format, power);
        assert(invertible == (value != INT32_MIN));
        if (invertible) assert(power == -int64_t(value));
    }
    const uint16_t unavailable[2] = {0x7fff, 0xffff};
    assert(decodeTelemetry(unavailable, format, power) && power == -INT32_MAX);
    format.rejectUnavailable = true;
    assert(!decodeTelemetry(unavailable, format, power));
    format.invert = false;
    assert(!decodeTelemetry(unavailable, format, power));
    format.valueType = ValueType::I16;
    const uint16_t negative16[2] = {uint16_t(-125), 0};
    assert(decodeTelemetry(negative16, format, power) && power == -125);
    format.valueType = ValueType::U32; format.scale = 0.01; format.rejectUnavailable = false;
    const uint16_t unsigned32[2] = {0xffff, 0xffff};
    assert(decodeTelemetry(unsigned32, format, power) && power == 42949672);
    format.valueType = ValueType::Float32; format.scale = 1000;
    const uint16_t float32[2] = {0x3fc0, 0};
    assert(decodeTelemetry(float32, format, power) && power == 1500);
    const uint16_t nan[2] = {0x7fc0, 0};
    const uint16_t infinity[2] = {0x7f80, 0};
    assert(!decodeTelemetry(nan, format, power) && !decodeTelemetry(infinity, format, power));
    format.wordOrder = WordOrder::LowFirst;
    const uint16_t swapped[2] = {0, 0x3fc0};
    assert(decodeTelemetry(swapped, format, power) && power == 1500);

    auto form = connectionForm();
    InverterConfig initial;
    assert(!parseInverterForm(form, initial) && initial.telemetry(0).unit == 2);
    assert(!parseInverterForm(form, config));
    form.telemetry[3] = mappingForm();
    assert(!parseInverterForm(form, config));
    assert(config.overrideTelemetry[3] && config.telemetry(3).registerAddress == 31913);
    const auto saved = record(config);
    const auto reject = [&](InverterForm invalid) {
        auto candidate = config;
        assert(parseInverterForm(invalid, candidate));
        assert(record(candidate) == saved);
    };
    for (const char *bad : {"", "256.0.0.1", "1.2.3", "1.2.3.4.5", " 1.2.3.4", "-1.2.3.4", "999999999999.0.0.0"}) {
        auto badForm = form; badForm.ip = bad; reject(badForm);
    }
    for (const char *bad : {"", "0", "65536", "-1", "502x", "9999999999999999999999"}) {
        auto badForm = form; badForm.port = bad; reject(badForm);
    }
    auto badForm = form; badForm.preset = "unknown"; reject(badForm);
    badForm = form; badForm.preset = nullptr; reject(badForm);
    badForm = form; badForm.telemetry[3].address = "65535"; reject(badForm);
    badForm = form; badForm.telemetry[3].unit = "248"; reject(badForm);
    badForm = form; badForm.telemetry[3].valueType = "unknown"; reject(badForm);
    badForm = form; badForm.telemetry[3].wordOrder = "unknown"; reject(badForm);
    for (const char *bad : {"0", "-1", "nan", "inf", "1e999", "1e-999", "1junk"}) {
        badForm = form; badForm.telemetry[3].scale = bad; reject(badForm);
    }
    badForm = form; badForm.telemetry[3].valueType = "u16"; badForm.telemetry[3].address = "65535";
    auto boundary = config;
    assert(!parseInverterForm(badForm, boundary));

    InverterConfig restored;
    assert(decodeInverterConfig(saved.c_str(), restored) && record(restored) == saved);
    form.telemetry[3].overrideValue = nullptr; form.preset = "sun2000";
    assert(!parseInverterForm(form, config) && !config.overrideTelemetry[3]);
}

static void testPollingAndSwitching() {
    reset();
    Inverter inverter;
    inverter.begin();
    auto &bus = *ModbusIP::latest;
    assert(inverter.getConfig().preset == PresetId("sun2000"));
    assert(!inverter.update());
    assert(inverter.isConnected && !inverter.hasValidReadings());
    assert(bus.requests[3].address == 37113 && bus.requests[3].unit == 1);
    for (size_t i = 0; i < 6; ++i) {
        if (i) { fakeMillis += 10000; inverter.update(); }
        replyToPoll(bus, 500);
        assert(inverter.update());
    }
    assert(inverter.hasValidReadings());
    assert(inverter.getBatteryStateOfCharge() == 95);
    assert(inverter.getBatteryChargePower() == -125);
    assert(inverter.meanPowerMeterActivePower(1) == 500);
    // Saving the same effective setup must not interrupt working automation.
    assert(inverter.saveConfig(inverter.getConfig()) == Inverter::SaveResult::Saved);
    assert(!inverter.update() && inverter.hasValidReadings());
    assert(inverter.meanPowerMeterActivePower(1) == 500);

    AsyncWebServer server;
    SmartRelay relay(16);
    relay.setup(&inverter, &server);
    relay.isPinEnabledSetting = true;
    relay.update();
    assert(relay.isPinOn);

    fakeMillis += 10000;
    inverter.update();
    const auto staleCallback = bus.requests[3].callback;
    auto config = inverter.getConfig();
    config.preset = PresetId("sun2000-emma");
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Busy);
    assert(inverter.getConfig().preset == PresetId("sun2000-emma"));
    assert(!inverter.update());
    assert(!inverter.hasValidReadings() && inverter.getPowerMeterActivePower() == 0);
    assert(inverter.meanPowerMeterActivePower(1) == 0);
    assert(inverter.meanPlantPower(1) == 0 && inverter.minBatteryStateOfCharge(1) == 0);
    relay.update();
    assert(!relay.isPinOn); // Switch-cycle hold cannot retain old data.
    assert(bus.requests.size() == 4);
    for (size_t i = 0; i < 3; ++i) assert(bus.requests[i].unit == 2);
    assert(bus.requests[3].address == 31657 && bus.requests[3].unit == 0);
    staleCallback(Modbus::EX_SUCCESS, 1, nullptr);
    assert(!inverter.update() && !inverter.hasValidReadings());

    // Partial replies cannot publish a mixed snapshot.
    bus.reply(0, 950);
    assert(!inverter.update());
    assert(inverter.getBatteryStateOfCharge() == 0 && !inverter.hasValidReadings());
    bus.reply(0, -125);
    bus.reply(1, 1337);
    bus.reply(2, -500);
    assert(inverter.update());
    assert(inverter.getPowerMeterActivePower() == 500 && inverter.hasValidReadings());

    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, 500);
    assert(inverter.update() && inverter.getPowerMeterActivePower() == -500);

    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, 0, Modbus::EX_ILLEGAL_ADDRESS);
    assert(!inverter.update() && !inverter.hasValidReadings());
    relay.isPinAlwaysOnSetting = true;
    relay.update();
    assert(relay.isPinOn); // Explicit manual override is retained.
    relay.isPinAlwaysOnSetting = false;
    relay.update();
    assert(!relay.isPinOn);

    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, -500);
    assert(inverter.update() && inverter.hasValidReadings());
    fakeMillis += 10000;
    inverter.update();
    bus.reply(0, 0, Modbus::EX_TIMEOUT);
    assert(!inverter.update() && !inverter.hasValidReadings());
    assert(bus.requests.empty() && !inverter.isConnected);

    Inverter rebooted;
    rebooted.begin();
    const auto restored = rebooted.getConfig();
    assert(record(restored) == record(config));
    assert(rebooted.port == 502 && rebooted.getConfig().telemetry(0).unit == 2);
}

static void testOverridesAndStorageFailures() {
    reset();
    Inverter inverter;
    inverter.begin();
    auto &bus = *ModbusIP::latest;
    auto config = inverter.getConfig();
    config.preset = PresetId("sun2000-emma");
    config.overrideTelemetry[3] = true;
    config.telemetryOverride[3] = TelemetryConfig(31913, 0, ValueType::I32, 1, WordOrder::HighFirst, true, true);
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    inverter.update();
    assert(bus.requests[3].address == 31913 && bus.requests[3].unit == 0);
    replyToPoll(bus, -500);
    assert(inverter.update());
    Inverter rebooted;
    rebooted.begin();
    assert(rebooted.getConfig().overrideTelemetry[3]);
    assert(rebooted.getConfig().telemetry(3).registerAddress == 31913);

    // Each reading has its own unit; changing one leaves the others intact.
    config.telemetryOverride[3].unit = 5;
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    inverter.update();
    assert(bus.requests[3].address == 31913 && bus.requests[3].unit == 5);
    for (size_t i = 0; i < 3; ++i) assert(bus.requests[i].unit == 2);
    replyToPoll(bus, INT32_MAX);
    assert(!inverter.update() && !inverter.hasValidReadings());
    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, INT32_MIN);
    assert(!inverter.update() && !inverter.hasValidReadings());

    const auto stored = Preferences::values["inverter/settings"];
    Preferences::failWrites = true;
    config.port = 1502;
    assert(inverter.saveConfig(config) == Inverter::SaveResult::StorageError);
    assert(inverter.getConfig().port == 502);
    assert(Preferences::values["inverter/settings"] == stored);
    Preferences::failWrites = false;

    // A successful TCP connection with no queued reads is not telemetry.
    reset();
    Inverter empty;
    empty.begin();
    ModbusIP::latest->rejectRequests = true;
    empty.update();
    assert(empty.isConnected && !empty.hasValidReadings());
    empty.update();
    assert(!empty.hasValidReadings());
}

static void testTelemetryRecovery() {
    reset();
    Inverter inverter;
    inverter.begin();
    auto &bus = *ModbusIP::latest;
    AsyncWebServer server;
    SmartRelay relay(16);
    relay.setup(&inverter, &server);
    relay.isPinEnabledSetting = true;

    const auto fillHistory = [&]() {
        for (size_t i = 0; i < 6; ++i) {
            fakeMillis += 10000;
            inverter.update();
            replyToPoll(bus, 500);
            assert(inverter.update());
        }
        relay.update();
        assert(relay.isPinOn);
    };
    const auto assertHistoryCleared = [&]() {
        assert(!inverter.hasValidReadings());
        assert(inverter.getPowerMeterActivePower() == 0);
        assert(inverter.meanPowerMeterActivePower(1) == 0);
        assert(inverter.meanPlantPower(1) == 0);
        assert(inverter.minBatteryStateOfCharge(1) == 0);
        relay.update();
        assert(!relay.isPinOn);
    };

    fillHistory();
    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, 500, Modbus::EX_ILLEGAL_ADDRESS);
    inverter.update();
    assertHistoryCleared();

    // One recovered snapshot must not reuse the history from before the failure.
    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, 500);
    assert(inverter.update());
    relay.update();
    assert(!relay.isPinOn);

    fillHistory();
    fakeMillis += 10000;
    inverter.update();
    replyToPoll(bus, 500);
    bus.reply(0, 1100); // An invalid state of charge invalidates the whole snapshot.
    inverter.update();
    assertHistoryCleared();

    fillHistory();
    bus.connected = false;
    inverter.update();
    assertHistoryCleared();

    SmartRelay uninitialized(17);
    uninitialized.update(); // setup() may have stopped after a filesystem mount failure.
    assert(!uninitialized.isPinOn);
}

static void testAllTelemetryOverrides() {
    reset();
    Inverter inverter;
    inverter.begin();
    auto &bus = *ModbusIP::latest;
    auto config = inverter.getConfig();
    applyDevicePreset(config, PresetId("generic"));
    assert(!config.configured());
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    inverter.update();
    assert(bus.requests.empty() && !inverter.hasValidReadings());
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) config.overrideTelemetry[i] = true;
    config.telemetryOverride[0] = TelemetryConfig(100, 3, ValueType::U32, 0.01);
    config.telemetryOverride[1] = TelemetryConfig(110, 4, ValueType::I16);
    config.telemetryOverride[2] = TelemetryConfig(120, 5, ValueType::Float32, 1000);
    config.telemetryOverride[3] = TelemetryConfig(130, 0, ValueType::I32, 1,
        WordOrder::LowFirst, false, true);
    assert(config.configured());
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    inverter.update();
    assert(bus.requests.size() == 4);
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const auto t = config.telemetry(i);
        assert(bus.requests[i].address == t.registerAddress);
        assert(bus.requests[i].unit == t.unit && bus.requests[i].count == t.words());
    }
    bus.reply(0, 7500);
    bus.reply(1, -125);
    bus.reply(2, 0x3fc00000); // Float32 1.5, expressed in kW.
    bus.reply(3, -500);
    std::swap(bus.requests[3].words[0], bus.requests[3].words[1]);
    assert(inverter.update() && inverter.hasValidReadings());
    assert(inverter.getBatteryStateOfCharge() == 75 && inverter.getBatteryChargePower() == -125);
    assert(inverter.getPlantPower() == 1500 && inverter.getPowerMeterActivePower() == 500);
    {
        Inverter rebooted;
        rebooted.begin();
        assert(record(rebooted.getConfig()) == record(config));
    }
    // Changing any metric must invalidate the previous complete snapshot.
    config.telemetryOverride[1].invert = true;
    assert(inverter.saveConfig(config) == Inverter::SaveResult::Saved);
    inverter.update();
    assert(!inverter.hasValidReadings() && inverter.getPlantPower() == 0);
    bus.reply(0, 7500); bus.reply(1, -125); bus.reply(2, 0x3fc00000); bus.reply(3, -500);
    std::swap(bus.requests[3].words[0], bus.requests[3].words[1]);
    assert(inverter.update() && inverter.getBatteryChargePower() == 125);
}

static std::string readFile(const std::string &path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static std::string replaceOnce(std::string source, const std::string &from, const std::string &to) {
    const auto at = source.find(from);
    assert(at != std::string::npos);
    source.replace(at, from.size(), to);
    return source;
}

static void testPresetFiles(const std::string &directory) {
    const auto source = readFile("data/presets/sun2000-emma.json");
    const auto presets = directory + "/presets";
    assert(mkdir(presets.c_str(), 0700) == 0);
    auto custom = replaceOnce(source, "sun2000-emma", "lab-device");
    custom = replaceOnce(custom, "\"unit\": 2", "\"unit\": 3");
    custom = replaceOnce(custom, "\"register\": 37004", "\"register\": 123");
    std::ofstream(presets + "/custom.json") << custom;
    std::ofstream(presets + "/ignored.txt") << "not a preset";
    LittleFS.root = directory;
    loadDevicePresets();
    assert(devicePresetRegistry().size() == 1 && findDevicePreset("lab-device"));
    InverterConfig config;
    assert(applyDevicePreset(config, PresetId("lab-device")) && config.telemetry(0).unit == 3);
    assert(config.telemetry(0).registerAddress == 123);
    assert(telemetryRow(config, 0).find("name=\"settings-soc-unit\"\n       value=\"3\"") != std::string::npos);
    assert(inverterSettingsValue("DEVICE_PRESETS", config).find("lab-device") != std::string::npos);
    reset();
    Inverter customInverter;
    assert(!customInverter.update()); // No boot-time reads before begin().
    customInverter.begin();
    assert(customInverter.saveConfig(config) == Inverter::SaveResult::Saved);
    customInverter.update();
    assert(ModbusIP::latest->requests[0].address == 123 && ModbusIP::latest->requests[0].unit == 3);
    assert(ModbusIP::latest->requests[1].unit == 2 && ModbusIP::latest->requests[2].unit == 2);
    assert(ModbusIP::latest->requests[3].unit == 0);
    custom = replaceOnce(custom, "\"register\": 123", "\"register\": 124");
    custom = replaceOnce(custom, "\"unit\": 3", "\"unit\": 4");
    std::ofstream(presets + "/custom.json") << custom;
    loadDevicePresets();
    Inverter changedFile;
    changedFile.begin();
    changedFile.update();
    assert(changedFile.getConfig().preset == PresetId("lab-device"));
    assert(ModbusIP::latest->requests[0].address == 124); // File defaults apply to non-overridden telemetry.
    assert(ModbusIP::latest->requests[0].unit == 4); // The preset's unit supersedes the saved unit.
    LittleFS.root = "data";
    loadDevicePresets();
    assert(devicePresetRegistry().size() == 3);
}

static void testTelemetryPreview() {
    InverterConfig saved;
    applyDevicePreset(saved, PresetId("sun2000-emma"));
    saved.overrideTelemetry[3] = true;
    saved.telemetryOverride[3].unit = 7;
    auto preview = saved;
    InverterForm form;
    previewTelemetry(form, nullptr, preview);
    assert(preview.overrideTelemetry[3] && preview.telemetry(3).unit == 7);

    form.preset = "sun2000-emma";
    previewTelemetry(form, nullptr, preview);
    assert(!preview.overrideTelemetry[3] && preview.telemetry(3).unit == 0);
    form.telemetry[0].overrideValue = "1";
    previewTelemetry(form, "soc", preview);
    assert(preview.overrideTelemetry[0] && preview.telemetry(0).unit == 2);

    form.telemetry[0] = mappingForm("123", "i32", "0.01");
    form.telemetry[0].unit = "5";
    previewTelemetry(form, "soc", preview);
    assert(preview.telemetry(0).registerAddress == 123 && preview.telemetry(0).unit == 5);
    assert(preview.telemetry(0).valueType == ValueType::I32 && preview.telemetry(0).invert);
    form.telemetry[0].overrideValue = nullptr;
    previewTelemetry(form, "soc", preview);
    assert(!preview.overrideTelemetry[0] && preview.telemetry(0).registerAddress == 37004);
    assert(preview.telemetry(0).valueType == ValueType::U16 && !preview.telemetry(0).invert);
    assert(saved.overrideTelemetry[3] && saved.telemetry(3).unit == 7);
}

static void renderSettings(const std::string &directory) {
    const auto source = readFile("data/settings.html");
    assert(!source.empty());
    const std::regex placeholder("%([A-Z_]+)%");
    const auto render = [&](const InverterConfig &config, const std::string &name) {
        std::string page;
        size_t offset = 0;
        for (auto it = std::sregex_iterator(source.begin(), source.end(), placeholder); it != std::sregex_iterator(); ++it) {
            page += source.substr(offset, it->position() - offset);
            page += inverterSettingsValue((*it)[1].str(), config);
            offset = it->position() + it->length();
        }
        page += source.substr(offset);
        std::ofstream(directory + "/" + name + ".html") << page;
    };
    for (const auto preset : {PresetId("sun2000"), PresetId("sun2000-emma"), PresetId("generic")}) {
        InverterConfig config;
        applyDevicePreset(config, preset);
        parseIPv4("192.0.2.10", config.ip);
        render(config, preset.c_str());
        InverterForm form;
        form.preset = preset.c_str();
        form.telemetry[0].overrideValue = "1";
        previewTelemetry(form, "soc", config);
        render(config, std::string(preset.c_str()) + "-soc-on");
        form.telemetry[0] = mappingForm("123", "i32", "0.01");
        form.telemetry[0].unit = "5";
        previewTelemetry(form, "soc", config);
        render(config, std::string(preset.c_str()) + "-soc-i32");
    }
}

int main(int argc, char **argv) {
    loadDevicePresets();
    testConfigAndDecoding();
    testPollingAndSwitching();
    testOverridesAndStorageFailures();
    testTelemetryRecovery();
    testAllTelemetryOverrides();
    testTelemetryPreview();
    if (argc == 2) {
        testPresetFiles(argv[1]);
        renderSettings(argv[1]);
        std::ofstream dump(std::string(argv[1]) + "/presets.tsv");
        dump << "preset\treading\trequires_overrides\tregister\tunit\ttype\torder\tscale\tinvert\tunavailable\n";
        for (const auto &preset : devicePresetRegistry()) {
            for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
                const auto &mapping = preset.telemetry[i];
                dump << preset.id.c_str() << '\t' << telemetryKey(i) << '\t' << preset.requiresOverrides << '\t'
                     << mapping.registerAddress << '\t' << unsigned(mapping.unit) << '\t' << valueTypeKey(mapping.valueType) << '\t'
                     << (mapping.wordOrder == WordOrder::HighFirst ? "high" : "low") << '\t' << telemetryScale(mapping.scale) << '\t'
                     << mapping.invert << '\t' << mapping.rejectUnavailable << '\n';
            }
        }
    }
    std::cout << "Inverter configuration, polling, persistence and relay regressions passed.\n";
}
