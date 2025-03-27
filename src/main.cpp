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
#include "SmartRelay.h"
#include "template.h"

WiFiManager wifiManager;

const char *dns_name = "solarboy";

Preferences prefs;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

bool settings_enable_data_collection;
String settings_data_collection_url;

Inverter inverter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
u_int64_t lastInverterDataTimestamp = 0;

DataCollector dc;
SmartRelay mySmartRelay(D1);

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
            {"DATA_COLLECTION",     settings_enable_data_collection ? "checked" : ""},
            {"DATA_COLLECTION_URL", String(settings_data_collection_url)},
    };
    String html = processTemplate(templateContent, variables);
    httpServer.send(200, "text/html", html);
}

void handlePostSettings() {
    if (httpServer.args() > 0) {
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

void setup() {
    Serial.begin(115200);
    Serial.print("Firmware v");
    Serial.println(FIRMWARE_VERSION);
    wifiManager.autoConnect("esp-solar-boy", "changemeplease");

    prefs.begin("esp-solar-boy");
    settings_enable_data_collection = prefs.getBool("settings-enable-data-collection", false);
    settings_data_collection_url = prefs.getString("settings-data-collection-url", "");

    /* Inverter Settings */
    String inverter_ip_str = prefs.getString("settings-inverter-ip", "192.168.142.20");
    inverter.ipAddress.fromString(inverter_ip_str);

    IPAddress tempIpAdress;
    tempIpAdress.fromString(inverter_ip_str);

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
    mySmartRelay.setup(&inverter, &httpServer);
}

void loop() {
    timeClient.update();
    httpServer.handleClient();
    MDNS.update();

    if (inverter.update()) {
        lastInverterDataTimestamp = timeClient.getEpochTime();
    }

    mySmartRelay.update();

    if (settings_enable_data_collection) {
        // Collect data with user consent
        dc.loop();
    }
}
