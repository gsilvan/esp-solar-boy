![ESP Solar Boy Image](https://raw.githubusercontent.com/gsilvan/esp-solar-boy/refs/heads/master/data/logo.svg)

# ESP Solar Boy

This project provides a firmware for the ESP32 microcontroller to intelligently control relays based on excess solar power availability. It integrates with Huawei SUN2000 inverters via Modbus-TCP to detect surplus energy and trigger connected loads accordingly. All settings are configurable via a simple HTTP interface, making it easy to set up and adjust without flashing new firmware.

## Features

- Relay control
- HTTP configuration
- Real-time monitoring via web-user-interface
- Huawei SUN2000 support via Modbus-TCP

## Setup

### Prerequisites

- PlatformIO
- ESP32 Dev Module (classic ESP32-WROOM, 4 MB flash)

### 1. Clone the repository

```bash
git clone https://github.com/gsilvan/esp-solar-boy.git
cd esp-solar-boy
```

### 2. Build the project

```bash
pio run -e esp32
```

### 3. Upload the firmware and the filesystem image

```bash
pio run -e esp32 -t upload
pio run -e esp32 -t uploadfs
```

### 4. WiFi settings

- Connect to `esp-solar-boy`
- Connect the device to your home WiFi
- Save and reboot

### 5. ESP Solar Boy configuration

- Navigate to http://solarboy.local via mDNS or use the assigned IP-Address
- Connect your relays to GPIO `16`, `17`, `18`
- Configure settings and relay-pins via web-ui

### ESP32 wiring and dependencies

| Function | GPIO | Connection |
| --- | --- | --- |
| Relay 1 | 16 | Active high relay input |
| Relay 2 | 17 | Active high relay input |
| Relay 3 | 18 | Active high relay input |
| WiFi reset button | 25 | Button to GND; hold for 5 seconds |
| Status LED | 26 | External LED with series resistor to GND; active high |

These pins target ESP32-WROOM boards. GPIO16/17 can be reserved for PSRAM on
ESP32-WROVER boards; other ESP32 variants need their own board and pin selection.
The web UI uses the GPIO numbers in its relay routes.

The port keeps Modbus, NTPClient, ArduinoJson, ESPAsyncWebServer, and
ESPAsyncHTTPUpdateServer. ESP32 uses AsyncTCP in place of ESPAsyncTCP and its
built-in Preferences library for settings. WiFi, HTTPClient, mDNS, and LittleFS
come from the ESP32 Arduino core.
Preferences explicitly opens the `nvs` partition so it is initialized when the
existing global constructors load settings, before Arduino's startup.

Upstream WiFiManager 2.0.17 provides the startup captive portal with its own
synchronous server. The portal closes before the application's async server
starts on port 80. The old vendored ESP8266 fork is retained but excluded from
the ESP32 build. Saved credentials reconnect automatically; holding the reset
button clears WiFi credentials and restarts into the portal.

Build and flash both firmware and LittleFS over USB for the initial ESP32
installation. Subsequent updates remain available at `/update`. ESP8266 binaries
and filesystem images cannot be used on ESP32, and settings are not migrated.

After flashing, verify the captive portal, saved WiFi reconnection after a power
cycle, `solarboy.local`, settings persistence, each relay, the status LED, and the
5-second WiFi reset button. Check Modbus readings against your inverter and test
firmware and LittleFS uploads through `/update` on the ESP32.

## License

MIT
