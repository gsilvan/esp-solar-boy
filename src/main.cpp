#include <algorithm>
#include <deque>
#include <numeric>

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>

#include "inverter.h"
#include "version.h"
#include "data_collector.h"

WiFiManager wifiManager;

const char *dns_name = "solarboy";

Preferences prefs;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/* DEQUE */
std::deque<int> input_power_history;
std::deque<int> battery_charge_history;
const uint16_t DEQUE_SIZE = 30;  // use a multiple of 60
uint64_t last_deque_update = 0;

uint16_t settings_battery_charge;
uint32_t settings_input_power;
uint8_t settings_monitoring_window_minutes;
uint8_t settings_switch_cycle_minutes;
bool settings_enable_data_collection;
String settings_data_collection_url;

/* PINS */
bool is_pin0_on = false;

Inverter inverter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
u_int64_t lastInverterDataTimestamp = 0;

DataCollector dc;

String processTemplate(const String &templateContent, const std::map<String, String> &variables) {
    String result = templateContent;

    for (const auto &pair: variables) {
        result.replace("{{" + pair.first + "}}", pair.second);
    }

    return result;
}

void handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        httpServer.send(404, "text/plain", "File Not Found");
        return;
    }
    httpServer.streamFile(file, "text/html");
    file.close();
}

void handleStyle() {
    File file = LittleFS.open("/style.css", "r");
    if (!file) {
        httpServer.send(404, "text/plain", "File Not Found");
        return;
    }
    httpServer.streamFile(file, "text/css");
    file.close();
}

void handleHTMX() {
    File file = LittleFS.open("/htmx.min.js", "r");
    if (!file) {
        httpServer.send(404, "text/plain", "File Not Found");
        return;
    }
    httpServer.streamFile(file, "application/javascript");
    file.close();
}

void handleBattery() {
    httpServer.send(200, "text/plain", String(inverter.getBatteryStateOfCharge()));
}

void handleBatteryChargeRate() {
    httpServer.send(200, "text/plain", String(inverter.getBatteryChargePower()));
}

void handlePlantPower() {
    httpServer.send(200, "text/plain", String(inverter.getPlantPower()));
}

void handlePowerMeterActivePower() {
    httpServer.send(200, "text/plain", String(inverter.getPowerMeterActivePower()));
}

void handleFirmwareVersion() {
    httpServer.send(200, "text/plain", String(FIRMWARE_VERSION));
}

void handleSettings() {
    File file = LittleFS.open("/settings.html", "r");
    if (!file) {
        httpServer.send(404, "text/plain", "File Not Found");
        return;
    }
    String templateContent = file.readString();
    file.close();
    std::map<String, String> variables = {
            {"IP_ADDRESS",          inverter.ipAddress.toString()},
            {"BATTERY_CHARGE",      String(settings_battery_charge)},
            {"PIN_0_INPUT_POWER",   String(settings_input_power)},
            {"PIN_0_TIMER",         String(settings_monitoring_window_minutes)},
            {"PIN_0_CYCLE",         String(settings_switch_cycle_minutes)},
            {"DATA_COLLECTION",     settings_enable_data_collection ? "checked" : ""},
            {"DATA_COLLECTION_URL", String(settings_data_collection_url)},
    };
    String html = processTemplate(templateContent, variables);
    httpServer.send(200, "text/html", html);
}

void handlePostSettings() {
    if (httpServer.args() > 0) {
        settings_battery_charge = (u_int16_t) httpServer.arg("pin-0-battery").toInt();
        prefs.putUShort("settings-p0-battery-charge", settings_battery_charge);

        settings_input_power = (u_int32_t) httpServer.arg("pin-0-input-power").toInt();
        prefs.putUInt("settings-p0-input-power", settings_input_power);

        settings_monitoring_window_minutes = (u_int8_t) httpServer.arg("pin-0-timer").toInt();
        prefs.putUChar("settings-p0-monitoring-window", settings_monitoring_window_minutes);

        settings_switch_cycle_minutes = (u_int8_t) httpServer.arg("pin-0-cycle").toInt();
        prefs.putUChar("settings-p0-switch-cycle", settings_switch_cycle_minutes);

        settings_enable_data_collection = httpServer.hasArg("enable-data-collection");
        prefs.putBool("settings-enable-data-collection", settings_enable_data_collection);

        settings_data_collection_url = (String) httpServer.arg("data-collection-url");
        prefs.putString("settings-data-collection-url", settings_data_collection_url);
        dc.url = settings_data_collection_url;

        String new_inverter_ip_str = httpServer.arg("settings-inverter-ip");
        if (inverter.ipAddress.fromString(new_inverter_ip_str)) {
            prefs.putString("settings-inverter-ip", inverter.ipAddress.toString());
        }

        httpServer.sendHeader("Location", "/settings", true);
        httpServer.send(200, "text/html", "Saved 👍");
    } else {
        httpServer.send(400, "application/html", "<h1>Bad request</h1>");
    }
}

void handleNotFound() { httpServer.send(404, "text/html", "<h1>404: Not found</h1>"); }

bool switch_pin(uint8_t pin) {
    bool is_on;
    auto min_battery = *std::min_element(battery_charge_history.begin(), battery_charge_history.end());
    double input_sum = std::accumulate(input_power_history.begin(), input_power_history.end(), 0);
    int mean_input = (int) (input_sum / input_power_history.size());
    if (is_pin0_on) {
        is_on = (min_battery >= settings_battery_charge) && (mean_input >= 100);
    } else {
        is_on = (min_battery >= settings_battery_charge) && (mean_input >= settings_input_power);
    }
    digitalWrite(pin, is_on);
    Serial.print("Min battery: ");
    Serial.println(min_battery);
    Serial.print("Mean input power: ");
    Serial.println(mean_input);
    Serial.print("PIN ON: ");
    Serial.println(is_on);
    return is_on;
}

void setup() {
    Serial.begin(115200);
    Serial.print("Firmware v");
    Serial.println(FIRMWARE_VERSION);
    wifiManager.autoConnect("esp-solar-boy", "changemeplease");

    prefs.begin("esp-solar-boy");
    settings_battery_charge = prefs.getUShort("settings-p0-battery-charge", 95);
    settings_input_power = prefs.getUInt("settings-p0-input-power", 1500);
    settings_monitoring_window_minutes = prefs.getUShort("settings-p0-monitoring-window", 5);
    settings_switch_cycle_minutes = prefs.getUShort("settings-p0-switch-cycle", 10);
    settings_enable_data_collection = prefs.getBool("settings-enable-data-collection", false);
    settings_data_collection_url = prefs.getString("settings-data-collection-url", "");

    /* Inverter Settings */
    String inverter_ip_str = prefs.getString("settings-inverter-ip", "192.168.142.20");
    inverter.ipAddress.fromString(inverter_ip_str);

    IPAddress tempIpAdress;
    tempIpAdress.fromString(inverter_ip_str);

    pinMode(D0, OUTPUT);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(dns_name)) {
        Serial.println("mDNS responder started");
        Serial.println("http://" + String(dns_name) + ".local/");
    }

    inverter.begin(tempIpAdress);
    dc.setup(&inverter, settings_data_collection_url, "dev-device");

    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    httpServer.on("/", HTTP_GET, handleRoot);
    httpServer.on("/htmx.min.js", HTTP_GET, handleHTMX);
    httpServer.on("/style.css", HTTP_GET, handleStyle);
    httpServer.on("/data/battery", HTTP_GET, handleBattery);
    httpServer.on("/data/batteryChargeRate", HTTP_GET, handleBatteryChargeRate);
    httpServer.on("/data/plantPower", HTTP_GET, handlePlantPower);
    httpServer.on("/data/powerMeterActivePower", HTTP_GET, handlePowerMeterActivePower);
    httpServer.on("/data/firmwareVersion", HTTP_GET, handleFirmwareVersion);
    httpServer.on("/settings", HTTP_GET, handleSettings);
    httpServer.on("/settings", HTTP_POST, handlePostSettings);
    httpServer.onNotFound(handleNotFound);

    httpUpdater.setup(&httpServer);  // Over the Air update via HTTP /update

    httpServer.begin();  // Actually start the server
    Serial.println("HTTP server started");
}

void loop() {
    timeClient.update();
    httpServer.handleClient();
    MDNS.update();

    if (inverter.update()) {
        lastInverterDataTimestamp = timeClient.getEpochTime();
    }

    uint64_t currentMillis = millis();
    if (currentMillis - last_deque_update >= (settings_monitoring_window_minutes * 60 * 1000 / DEQUE_SIZE)) {
        if (input_power_history.size() >= DEQUE_SIZE) {
            input_power_history.pop_front();
        }
        input_power_history.push_back(inverter.getPowerMeterActivePower());

        for (int val: input_power_history) {
            Serial.print(val);
            Serial.print(" ");
        }
        Serial.println();

        if (battery_charge_history.size() >= DEQUE_SIZE) {
            battery_charge_history.pop_front();
        }
        battery_charge_history.push_back(inverter.getBatteryStateOfCharge());

        for (int val: battery_charge_history) {
            Serial.print(val);
            Serial.print(" ");
        }
        Serial.println();

        last_deque_update = millis();
    }

    currentMillis = millis();
    static uint64_t last_switch_check = 0;
    if (currentMillis - last_switch_check >= (settings_switch_cycle_minutes * 60 * 1000)) {
        last_switch_check = millis();
        is_pin0_on = switch_pin(D0);
    }

    if (settings_enable_data_collection) {
        // Collect data with user consent
        dc.loop();
    }
}
