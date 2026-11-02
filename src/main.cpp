#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ESPAsyncHTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include "WiFiPortal.h"

#include "Inverter.h"
#include "DevicePresets.h"
#include "InverterSettings.h"
#include "version.h"
#include "SmartRelay.h"
#include "LongPressButton.h"
#include "StatusLed.h"

const char *dns_name = "solarboy";

AsyncWebServer httpServer(80);
ESPAsyncHTTPUpdateServer updateServer;

Inverter inverter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
u_int64_t lastInverterDataTimestamp = 0;

#ifdef ESP8266
const uint8_t relay0 = D0;
const uint8_t relay1 = D1;
const uint8_t relay2 = D2;
const uint8_t resetPin = D5;
const uint8_t ledPin = D6;
#else
const uint8_t relay0 = 16;
const uint8_t relay1 = 17;
const uint8_t relay2 = 18;
const uint8_t resetPin = 25;
const uint8_t ledPin = 26;
#endif
SmartRelay sm0(relay0);
SmartRelay sm1(relay1);
SmartRelay sm2(relay2);
LongPressButton resetButton(resetPin);
StatusLed statusLed(ledPin);

static void resetWifi() {
    Serial.println("RESET WIFI");
#ifdef ESP8266
    WiFi.disconnect(true);
    Serial.println("ERASE CONFIG");
    ESP.eraseConfig();
#else
    WiFi.disconnect(true, true);
#endif
    Serial.println("RESTART");
    ESP.restart();
}

static InverterForm readInverterForm(AsyncWebServerRequest *request, bool post) {
    // Parameter strings stay owned by the request while the form is validated.
    const auto parameter = [request, post](const String &name) -> const char * {
        const auto *value = request->getParam(name, post);
        return value ? value->value().c_str() : nullptr;
    };
    InverterForm form;
    form.ip = parameter("settings-inverter-ip");
    form.port = parameter("settings-inverter-port");
    form.preset = parameter("settings-device-preset");
    for (size_t i = 0; i < TELEMETRY_COUNT; ++i) {
        const String prefix = "settings-" + String(telemetryKey(i)) + "-";
        auto &mapping = form.telemetry[i];
        mapping.overrideValue = parameter(prefix + "override");
        mapping.address = parameter(prefix + "register");
        mapping.unit = parameter(prefix + "unit");
        mapping.valueType = parameter(prefix + "type");
        mapping.wordOrder = parameter(prefix + "order");
        mapping.scale = parameter(prefix + "scale");
        mapping.rejectUnavailable = parameter(prefix + "unavailable");
        mapping.invert = parameter(prefix + "invert");
    }
    return form;
}

void setup() {
    Serial.begin(115200);
    Serial.print("Firmware v");
    Serial.println(FIRMWARE_VERSION);
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    loadDevicePresets();
    inverter.begin();
    provision();
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

    statusLed.attachInverter(&inverter);

    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html", false,
            [](const String &key) -> String {
                if (key == "RELAY0") return String(relay0);
                if (key == "RELAY1") return String(relay1);
                if (key == "RELAY2") return String(relay2);
                return String();
            });
        request->send(response);
    });
    httpServer.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/style.css", "text/css");
        request->send(response);
    });
    httpServer.on("/htmx.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/htmx.min.js", "application/javascript");
        request->send(response);
    });
    httpServer.on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/logo.svg", "image/svg+xml");
        request->send(response);
    });
    httpServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/favicon.ico", "image/x-icon");
        request->send(response);
    });
    httpServer.on("/data/battery", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getBatteryStateOfCharge()));
    });
    httpServer.on("/getBatteryLevel", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = R"(<div class="battery-level" style="width: {{BATTERY}}{{%}};" hx-get="/getBatteryLevel" hx-trigger="every 10s" hx-swap="outerHTML"></div>)";
        html.replace("{{BATTERY}}", String(inverter.getBatteryStateOfCharge()));
        html.replace("{{%}}", inverter.getBatteryStateOfCharge() > 0 ? "%" : "");
        request->send(200, "text/html", html);
    });
    httpServer.on("/getPowerArrow", HTTP_GET, [](AsyncWebServerRequest * request) {
       String html = R"(<div class="power-arrow {{DIRECTION}}" hx-get="/getPowerArrow" hx-trigger="every 10s" hx-swap="outerHTML"></div>)";
       html.replace("{{DIRECTION}}", inverter.getBatteryChargePower() >= 0 ? "charging" : "discharging");
       request->send(200, "text/html", html);
    });
    httpServer.on("/data/batteryChargeRate", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getBatteryChargePower()));
    });
    httpServer.on("/data/plantPower", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getPlantPower()));
    });
    httpServer.on("/data/powerMeterActivePower", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getPowerMeterActivePower()));
    });
    httpServer.on("/data/firmwareVersion", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(FIRMWARE_VERSION));
    });
    httpServer.on("/getInverterIp", HTTP_GET, [](AsyncWebServerRequest *request){
        const auto config = inverter.getConfig();
        request->send(200, "text/plain", IPAddress(config.ip).toString());
    });
    httpServer.on("/getInverterPort", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getConfig().port));
    });
    httpServer.on("/getConnectionStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", inverter.hasValidReadings()
            ? R"(<div class="connection-indicator connected"></div><div class="connection-text connected-text">Inverter connected</div>)"
            : R"(<div class="connection-indicator disconnected"></div><div class="connection-text disconnected-text">Inverter disconnected</div>)");
    });
    httpServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        auto config = inverter.getConfig();
        const String reading = request->arg("reading");
        previewTelemetry(readInverterForm(request, false), reading.c_str(), config);
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/settings.html", "text/html", false,
            [config](const String &key) -> String { return inverterSettingsValue(key, config); });
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    httpServer.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->args() == 0) {
            request->send(400, "application/html", "<h1>Bad request</h1>");
            return;
        }
        auto config = inverter.getConfig();
        const auto form = readInverterForm(request, true);
        if (const char *error = parseInverterForm(form, config)) {
            request->send(400, "text/plain", error);
            return;
        }
        switch (inverter.saveConfig(config)) {
            case Inverter::SaveResult::Saved:
                request->send(200, "text/plain", "Saved 👍");
                break;
            case Inverter::SaveResult::Busy:
                request->send(409, "text/plain", "Settings are being applied. Try saving again.");
                break;
            case Inverter::SaveResult::StorageError:
                request->send(500, "text/plain", "Could not save settings.");
                break;
        }
    });

    sm0.setup(&inverter, &httpServer);
    sm1.setup(&inverter, &httpServer);
    sm2.setup(&inverter, &httpServer);
    updateServer.setup(&httpServer);
    httpServer.begin();
    Serial.println("HTTP server started");
    resetButton.onLongPress(resetWifi);
    resetButton.begin();
}

void loop() {
    statusLed.update();
    timeClient.update();
#ifdef ESP8266
    MDNS.update();
#endif

    if (inverter.update()) {
        lastInverterDataTimestamp = timeClient.getEpochTime();
    }

    sm0.update();
    sm1.update();
    sm2.update();

    resetButton.update();
}
