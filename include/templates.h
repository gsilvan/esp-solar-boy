//
// Created by silvan on 17.01.25.
//
#include <Arduino.h>

#ifndef ESP_SOLAR_BOY_TEMPLATES_H
#define ESP_SOLAR_BOY_TEMPLATES_H

static const char settingsHtmlTemplate[] PROGMEM =
        R"(
<!DOCTYPE html>
<html>
<head>
    <title>Solar-Boy-2000</title>
    <style>
        .input { width: 100%; }
    </style>
</head>
<body>
    <h1>Settings</h1>
    <form method="post">
    <div style="max-width: 450px;">
        <h2>Device</h2>
        <label for="settings-ssid">SSID:</label>
        <input class="input" type="text" id="settings-ssid" name="settings-ssid" placeholder="SSID">
        <label for="settings-password">Password:</label>
        <input class="input" type="password" id="settings-password" name="settings-password" placeholder="Password">
        <label for="settings-inverter-ip">Inverter IPv4:</label>
        <input class="input" type="text" id="settings-inverter-ip" name="settings-inverter-ip" value="%IPADDRESS%">
        <label for="enable-data-collection">Telemetry:</label>
        <input class="input" type="checkbox" id="enable-data-collection" name="enable-data-collection" %DATACOLLECTION%>
        <label for="data-collection-url">URL:</label>
        <input class="input" type="text" id="data-collection-url" name="data-collection-url" value="%DATACOLLECTIONURL%">
        <button>Save</button>
        <h2>PINs</h2>
        <h3>PIN_0</h3>
        <label for="pin-0-battery">Battery charge (%):</label>
        <input class="input" type="number" min="0" max="100" step="1" id="pin-0-battery" name="pin-0-battery" value="%BATTERYCHARGE%">
        <label for="pin-0-input-power">Power overflow (Watts):</label>
        <input class="input" type="number" min="100" step="100" id="pin-0-input-power" name="pin-0-input-power" value="%PIN0INPUTPOWER%">
        <label for="pin-0-timer">Monitoring window (minutes):</label>
        <input class="input" type="number" min="0" max="60" step="1" id="pin-0-timer" name="pin-0-timer" value="%PIN0TIMER%">
        <label for="pin-0-cycle">Switch cycle (minutes):</label>
        <input class="input" type="number" min="0" max="60" step="1" id="pin-0-cycle" name="pin-0-cycle" value="%PIN0CYCLE%">
    </div>
    <button>Save</button>
    </form>
    <a href="/">back</a>
</body>
</html>
)";

#endif //ESP_SOLAR_BOY_TEMPLATES_H
