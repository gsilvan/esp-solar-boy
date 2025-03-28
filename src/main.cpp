#include <ESP8266WiFi.h>
#include <ESPAsync_WiFiManager.h>
#include <ESPAsyncDNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <Preferences.h>
#include <ESPAsyncHTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>

#include "Inverter.h"
#include "version.h"
#include "DataCollector.h"
#include "SmartRelay.h"

const char *dns_name = "solarboy";

Preferences prefs;
AsyncWebServer httpServer(80);
AsyncDNSServer dnsServer;
ESPAsync_WiFiManager wifiManager(&httpServer, &dnsServer);
ESPAsyncHTTPUpdateServer updateServer;

bool settings_enable_data_collection;
String settings_data_collection_url;

Inverter inverter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
u_int64_t lastInverterDataTimestamp = 0;

DataCollector dc;
SmartRelay sm0(D0);
SmartRelay sm1(D1);
SmartRelay sm2(D2);

void setup() {
    Serial.begin(115200);
    Serial.print("Firmware v");
    Serial.println(FIRMWARE_VERSION);
    wifiManager.autoConnect("esp-solar-boy", "changemeplease");

    prefs.begin("esp-solar-boy");
    settings_enable_data_collection = prefs.getBool("settings-enable-data-collection", false);
    settings_data_collection_url = prefs.getString("settings-data-collection-url", "");

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

    dc.setup(&inverter, settings_data_collection_url, "dev-device");

    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html");
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
    httpServer.on("/data/battery", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.getBatteryStateOfCharge()));
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
        request->send(200, "text/plain", String(inverter.ipAddress.toString()));
    });
    httpServer.on("/getInverterPort", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.port));
    });
    httpServer.on("/getInverterModbusUnit", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.modbusUnit));
    });
    httpServer.on("/getTelemetryCheckbox", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", settings_enable_data_collection
                                         ? R"(<input class="input" type="checkbox" id="enable-data-collection" name="enable-data-collection" checked>)"
                                         : R"(<input class="input" type="checkbox" id="enable-data-collection" name="enable-data-collection">)");
    });
    httpServer.on("/getTelemetryUrl", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(settings_data_collection_url));
    });
    httpServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/settings.html", "text/html");
        request->send(response);
    });
    httpServer.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->args() == 0) {
            request->send(400, "application/html", "<h1>Bad request</h1>");
            return;
        }
        settings_enable_data_collection = request->hasArg("enable-data-collection");
        prefs.putBool("settings-enable-data-collection", settings_enable_data_collection);

        settings_data_collection_url = (String) request->arg("data-collection-url");
        prefs.putString("settings-data-collection-url", settings_data_collection_url);
        dc.url = settings_data_collection_url;

        inverter.setIpAddress(request->arg("settings-inverter-ip"));
        inverter.setPort(request->arg("settings-inverter-port"));
        inverter.setModbusUnit(request->arg("settings-inverter-modbus-unit"));
        request->send(200, "text/plain", "Saved 👍");
    });

    sm0.setup(&inverter, &httpServer);
    sm1.setup(&inverter, &httpServer);
    sm2.setup(&inverter, &httpServer);
    updateServer.setup(&httpServer);
    httpServer.begin();
    Serial.println("HTTP server started");
}

void loop() {
    timeClient.update();
    MDNS.update();

    if (inverter.update()) {
        lastInverterDataTimestamp = timeClient.getEpochTime();
    }

    sm0.update();
    sm1.update();
    sm2.update();

    if (settings_enable_data_collection) {
        // Collect data with user consent
        dc.loop();
    }
}
