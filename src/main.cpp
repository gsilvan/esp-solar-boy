#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ModbusIP_ESP8266.h>
#include <Preferences.h>

#include <deque>

Preferences prefs;

ESP8266WebServer server(80);

ModbusIP mb;

uint16_t RUNNING_STATUS = 37000;

uint16_t CHARGE_DISCHARGE = 37001;

uint16_t SOC = 37004;
uint16_t soc = 0;

uint16_t INPUT_POWER = 32064;

/* TIMERS */
const uint64_t interval = 2000;
uint64_t previousMillis = 0;

/* DEQUE */
std::deque<int> input_power_history;

uint8_t settings_battery_charge;
uint32_t settings_power_overflow;
uint8_t settings_monitoring_window_minutes;
uint8_t settings_switch_cycle_minutes;

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

void all_off() {
    digitalWrite(D0, LOW);
    digitalWrite(D1, LOW);
    digitalWrite(D2, LOW);
}

void all_on() {
    digitalWrite(D0, HIGH);
    digitalWrite(D1, HIGH);
    digitalWrite(D2, HIGH);
}

void led_check() {
    all_off();
    delay(400);
    all_on();
    delay(400);
    all_off();
    delay(400);
    digitalWrite(D0, HIGH);
    delay(400);
    digitalWrite(D1, HIGH);
    delay(400);
    digitalWrite(D2, HIGH);
    delay(400);
    digitalWrite(D0, LOW);
    delay(400);
    digitalWrite(D1, LOW);
    delay(400);
    digitalWrite(D2, LOW);
}

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
    server.send(200, "text/html", html);
}

void handleSettings() {
    char html[2048];
    snprintf(html, sizeof(html),
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<title>Solar-Boy-2000</title>"
             "<style>.input {width: 100%%;}</style>"
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
             "<button>Save</button>"
             "<h2>PINs</h2>"
             "<h3>PIN_0</h3>"
             "<label for=\"pin-0-battery\">Battery charge (%%):</label>"
             "<input class=\"input\" type=\"number\" min=\"0\" max=\"100\" step=\"1\" id=\"pin-0-battery\" "
             "name=\"pin-0-battery\" value=\"%d\">"
             "<label for=\"pin-0-input-power\">Power overflow (Watts):</label>"
             "<input class=\"input\" type=\"number\" step=\"100\" id=\"pin-0-input-power\" name=\"pin-0-input-power\" "
             "value=\"%d\">"
             "<label for=\"pin-0-timer\">Monitoring window (minutes):</label>"
             "<input class=\"input\" type=\"number\" min=\"0\" max=\"60\" step=\"1\" id=\"pin-0-timer\" "
             "name=\"pin-0-timer\" value=\"%d\">"
             "<label for=\"pin-0-cycle\">Switch cycle (minutes):</label>"
             "<input class=\"input\" type=\"number\" min=\"0\" max=\"60\" step=\"1\" id=\"pin-0-cycle\" "
             "name=\"pin-0-cycle\" value=\"%d\">"
             "</div>"
             "<button>Save</button>"
             "</form>"
             "<a href=\"/\">back</a>"
             "</body>"
             "</html>",
             settings_battery_charge, settings_power_overflow, settings_monitoring_window_minutes,
             settings_switch_cycle_minutes);
    server.send(200, "text/html", html);
}

void handleNotFound() { server.send(404, "text/html", "<h1>404: Not found</h1>"); }

void setup() {
    prefs.begin("esp-solar-boy");
    settings_battery_charge = prefs.getUShort("settings-p0-battery-charge", 95);
    settings_power_overflow = prefs.getUInt("settings-p0-power-overflow", 1500);
    settings_monitoring_window_minutes = prefs.getUShort("settings-p0-monitoring-window", 5);
    settings_switch_cycle_minutes = prefs.getUShort("settings-p0-switch-cycle", 10);

    /* Initial Settings */
    inverter.ip = IPAddress(127, 0, 0, 1);

    pinMode(D0, OUTPUT);
    pinMode(D1, OUTPUT);
    pinMode(D2, OUTPUT);
    led_check();
    Serial.begin(9600);

    WiFi.begin("change", "me");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    mb.client();

    server.on("/", handleIndex);
    server.on("/settings", handleSettings);
    server.onNotFound(handleNotFound);

    server.begin();  // Actually start the server
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();

    uint64_t currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = millis();
        if (mb.isConnected(inverter.ip)) {
            mb.readHreg(inverter.ip, SOC, &inverter.battery_state_of_capacity, 1, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, RUNNING_STATUS, (uint16_t *)&inverter.battery_state, 1, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, CHARGE_DISCHARGE, (uint16_t *)&inverter.battery_charging_power, 2, nullptr, 1);
            delay(100);
            mb.task();

            mb.readHreg(inverter.ip, INPUT_POWER, (uint16_t *)&inverter.input_power, 2, nullptr, 1);
            delay(100);
            mb.task();

        } else {
            mb.connect(inverter.ip);
        }

        if (inverter.battery_state_of_capacity / 10 >= 0) {
            digitalWrite(D2, HIGH);
        }
        if (inverter.battery_state_of_capacity / 10 >= 33) {
            digitalWrite(D1, HIGH);
        }
        if (inverter.battery_state_of_capacity / 10 < 33) {
            digitalWrite(D1, LOW);
        }
        if (inverter.battery_state_of_capacity / 10 >= 66) {
            digitalWrite(D0, HIGH);
        }
        if (inverter.battery_state_of_capacity / 10 < 66) {
            digitalWrite(D0, LOW);
        }

        Serial.printf("Battery: %d\n", inverter.battery_state_of_capacity / 10);
        Serial.printf("Battery state: %d\n", inverter.battery_state);
        Serial.printf("Charge: %d\n",
                      (inverter.battery_charging_power >> 16) | (inverter.battery_charging_power << 16));
        Serial.printf("Input Power: %d\n", (inverter.input_power >> 16) | (inverter.input_power << 16));

        if (input_power_history.size() >= 128) {
            input_power_history.pop_front();
        }
        input_power_history.push_back((inverter.input_power >> 16) | (inverter.input_power << 16));

        for (int val : input_power_history) {
            Serial.print(val);
            Serial.print(" ");
        }
        Serial.println();
    }
}