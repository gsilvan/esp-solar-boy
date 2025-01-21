#include <algorithm>
#include <deque>
#include <numeric>

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ModbusIP_ESP8266.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "version.h"
#include "templates.h"

WiFiManager wifiManager;

const char* dns_name = "solarboy";

Preferences prefs;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
ModbusIP mb;

/* MODBUS REGISTERS */
uint16_t RUNNING_STATUS = 37000;
uint16_t CHARGE_DISCHARGE = 37001;
uint16_t SOC = 37004;
uint16_t INPUT_POWER = 32064;

/* TIMERS */
const uint64_t interval = 2000;
uint64_t previousMillis = 0;
uint64_t last_deque_update = 0;
uint64_t last_cycle_start = 0;

/* DEQUE */
std::deque<int> input_power_history;
std::deque<int> battery_charge_history;
const uint16_t DEQUE_SIZE = 60;  // use a multiple of 60

uint16_t settings_battery_charge;
uint32_t settings_input_power;
uint8_t settings_monitoring_window_minutes;
uint8_t settings_switch_cycle_minutes;

/* PINS */
bool is_pin0_on = false;

/* INVERTERS */
enum Sun2000BatteryState {
    offline = 1,
    stand_by = 2,
    running = 3,
    fault = 4,
    sleep_mode = 5,
};

struct Sun2000 {
    u_int16_t device_state = 0;
    int32_t input_power = 0;
    u_int16_t battery_state_of_capacity = 0;
    Sun2000BatteryState battery_state = offline;
    int32_t battery_charging_power = 0;
    IPAddress ip;
    in_port_t port = 502;
};

Sun2000 inverter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
u_int64_t lastInverterDataTimestamp = 0;

void handleIndex() {
    String html(reinterpret_cast<const char *>(indexHtmlTemplate));
    html.replace("%STATE%", String(inverter.battery_state));
    html.replace("%BATTERYCHARGE%", String(inverter.battery_state_of_capacity / 10));
    html.replace("%CHARGE%", String((inverter.battery_charging_power >> 16) | (inverter.battery_charging_power << 16)));
    html.replace("%INPUTPOWER%", String((inverter.input_power >> 16) | (inverter.input_power << 16)));
    html.replace("%UNIXTIMESTAMP%", String(lastInverterDataTimestamp));
    httpServer.send(200, "text/html", html);
}

void handleSettings() {
    String html(reinterpret_cast<const char *>(settingsHtmlTemplate));
    html.replace("%IPADDRESS%", inverter.ip.toString());
    html.replace("%BATTERYCHARGE%", String(settings_battery_charge));
    html.replace("%PIN0INPUTPOWER%", String(settings_input_power));
    html.replace("%PIN0TIMER%", String(settings_monitoring_window_minutes));
    html.replace("%PIN0CYCLE%", String(settings_switch_cycle_minutes));
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

        String new_inverter_ip_str = httpServer.arg("settings-inverter-ip");
        if (inverter.ip.fromString(new_inverter_ip_str)) {
            prefs.putString("settings-inverter-ip", inverter.ip.toString());
        }

        httpServer.sendHeader("Location", "/settings", true);
        httpServer.send(302, "text/plain", "");
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

    /* Inverter Settings */
    String inverter_ip_str = prefs.getString("settings-inverter-ip", "192.168.142.20");
    inverter.ip.fromString(inverter_ip_str);

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

    mb.client();

    httpServer.on("/", handleIndex);
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

    uint64_t currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = millis();
        if (mb.isConnected(inverter.ip)) {
            mb.readHreg(inverter.ip, SOC, &inverter.battery_state_of_capacity, 1, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, RUNNING_STATUS, (uint16_t *) &inverter.battery_state, 1, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, CHARGE_DISCHARGE, (uint16_t *) &inverter.battery_charging_power, 2, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, INPUT_POWER, (uint16_t *) &inverter.input_power, 2, nullptr, 1);
            delay(100);
            mb.task();

            lastInverterDataTimestamp = timeClient.getEpochTime();
        } else {
            mb.connect(inverter.ip);
        }

        currentMillis = millis();
        if (currentMillis - last_cycle_start >= settings_monitoring_window_minutes * 60 * 1000) {
            is_pin0_on = switch_pin(D0);
            if (is_pin0_on) {
                last_cycle_start = millis();
            }
        }

        Serial.printf("Battery: %d\n", inverter.battery_state_of_capacity / 10);
        Serial.printf("Battery state: %d\n", inverter.battery_state);
        Serial.printf("Charge: %d\n",
                      (inverter.battery_charging_power >> 16) | (inverter.battery_charging_power << 16));
        Serial.printf("Input Power: %d\n", (inverter.input_power >> 16) | (inverter.input_power << 16));
    }

    currentMillis = millis();
    if (currentMillis - last_deque_update >= (settings_monitoring_window_minutes * 60 * 1000 / DEQUE_SIZE)) {
        if (input_power_history.size() >= DEQUE_SIZE) {
            input_power_history.pop_front();
        }
        input_power_history.push_back((inverter.input_power >> 16) | (inverter.input_power << 16));

        for (int val: input_power_history) {
            Serial.print(val);
            Serial.print(" ");
        }
        Serial.println();

        if (battery_charge_history.size() >= DEQUE_SIZE) {
            battery_charge_history.pop_front();
        }
        battery_charge_history.push_back(inverter.battery_state_of_capacity / 10);

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
}
