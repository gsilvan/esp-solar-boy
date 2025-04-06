#include <ESP8266WiFi.h>
#include <ESPAsync_WiFiManager.h>
#include <ESPAsyncDNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncHTTPUpdateServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>

#include "Inverter.h"
#include "version.h"
#include "DataCollector.h"
#include "SmartRelay.h"

const char *dns_name = "solarboy";

AsyncWebServer httpServer(80);
AsyncDNSServer dnsServer;
ESPAsync_WiFiManager wifiManager(&httpServer, &dnsServer);
ESPAsyncHTTPUpdateServer updateServer;

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

    dc.setup(&inverter, "dev-device");

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
        request->send(200, "text/plain", String(inverter.ipAddress.toString()));
    });
    httpServer.on("/getInverterPort", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.port));
    });
    httpServer.on("/getInverterModbusUnit", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(inverter.modbusUnit));
    });
    httpServer.on("/getTelemetryCheckbox", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", dc.isEnabled
                                         ? R"(<input class="input" type="checkbox" id="enable-data-collection" name="enable-data-collection" checked>)"
                                         : R"(<input class="input" type="checkbox" id="enable-data-collection" name="enable-data-collection">)");
    });
    httpServer.on("/getTelemetryUrl", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", dc.url);
    });
    httpServer.on("/getConnectionStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", inverter.isConnected
                                         ? R"(<div class="connection-indicator connected"></div><div class="connection-text connected-text">Sun2000 connected</div>)"
                                         : R"(<div class="connection-indicator disconnected"></div><div class="connection-text disconnected-text">Sun2000 disconnected</div>)");
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
        dc.setIsEnabled(request->hasArg("enable-data-collection"));
        dc.setUrl(request->arg("data-collection-url"));
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

    dc.update();
}
