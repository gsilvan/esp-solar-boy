#include <algorithm>
#include <deque>
#include <numeric>

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ModbusIP_ESP8266.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESP8266HTTPUpdateServer.h>

#include "version.h"

WiFiManager wifiManager;

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

void handleIndex() {
    char html[256];
    snprintf(html, sizeof(html),
             "<!DOCTYPE html>"
             "<html>"
             "<head><title>Solar-Boy-2000</title></head>"
             "<body>"
             "<h1>Battery</h1>"
             "<p>State: %d</p>"
             "<p>Battery charge: %d</p>"
             "<p>Charge: %d</p>"
             "<p>Input power: %d</p>"
             "<a href=\"/settings\">Settings</a>"
             "</body>"
             "</html>",
             inverter.battery_state, inverter.battery_state_of_capacity / 10,
             (inverter.battery_charging_power >> 16) | (inverter.battery_charging_power << 16),
             (inverter.input_power >> 16) | (inverter.input_power << 16));
    httpServer.send(200, "text/html", html);
}

void handleGetSettings() {
    String html =
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>Solar-Boy-2000</title>"
            "<style>.input {width: 100%;}</style>"
            "</head>"
            "<body>"
            "<h1>Settings</h1>"
            "<form method=\"post\">"
            "<div style=\"max-width: 450px;\">"
            "<h2>Device</h2>"
            "<label for=\"settings-ssid\">SSID:</label>"
            "<input class=\"input\" type=\"text\" id=\"settings-ssid\" name=\"settings-ssid\" placeholder=\"SSID\">"
            "<label for=\"settings-password\">Password:</label>"
            "<input class=\"input\" type=\"password\" id=\"settings-password\" name=\"settings-password\" "
            "placeholder=\"Password\">"
            "<label for=\"settings-inverter-ip\">Inverter IPv4:</label>"
            "<input class=\"input\" type=\"text\" id=\"settings-inverter-ip\" name=\"settings-inverter-ip\" value=\"" +
            inverter.ip.toString() +
            "\">"
            "<button>Save</button>"
            "<h2>PINs</h2>"
            "<h3>PIN_0</h3>"
            "<label for=\"pin-0-battery\">Battery charge (%):</label>"
            "<input class=\"input\" type=\"number\" min=\"0\" max=\"100\" step=\"1\" id=\"pin-0-battery\" "
            "name=\"pin-0-battery\" value=\"" +
            String(settings_battery_charge) +
            "\">"
            "<label for=\"pin-0-input-power\">Power overflow (Watts):</label>"
            "<input class=\"input\" type=\"number\" step=\"100\" id=\"pin-0-input-power\" name=\"pin-0-input-power\" "
            "value=\"" +
            String(settings_input_power) +
            "\">"
            "<label for=\"pin-0-timer\">Monitoring window (minutes):</label>"
            "<input class=\"input\" type=\"number\" min=\"0\" max=\"60\" step=\"1\" id=\"pin-0-timer\" "
            "name=\"pin-0-timer\" value=\"" +
            String(settings_monitoring_window_minutes) +
            "\">"
            "<label for=\"pin-0-cycle\">Switch cycle (minutes):</label>"
            "<input class=\"input\" type=\"number\" min=\"0\" max=\"60\" step=\"1\" id=\"pin-0-cycle\" "
            "name=\"pin-0-cycle\" value=\"" +
            String(settings_switch_cycle_minutes) +
            "\">"
            "</div>"
            "<button>Save</button>"
            "</form>"
            "<a href=\"/\">back</a>"
            "</body>"
            "</html>";
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
        is_on = (min_battery >= settings_battery_charge) && (mean_input >= 0);
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

    mb.client();

    httpServer.on("/", handleIndex);
    httpServer.on("/settings", HTTP_GET, handleGetSettings);
    httpServer.on("/settings", HTTP_POST, handlePostSettings);
    httpServer.onNotFound(handleNotFound);

    httpUpdater.setup(&httpServer);  // Over the Air update via HTTP /update

    httpServer.begin();  // Actually start the server
    Serial.println("HTTP server started");
}

void loop() {
    httpServer.handleClient();

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
}