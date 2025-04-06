![ESP Solar Boy Image](https://raw.githubusercontent.com/gsilvan/esp-solar-boy/refs/heads/master/data/logo.svg)

# ESP Solar Boy

This project provides a firmware for the ESP8266 microcontroller to intelligently control relays based on excess solar power availability. It integrates with Huawei SUN2000 inverters via Modbus-TCP to detect surplus energy and trigger connected loads accordingly. All settings are configurable via a simple HTTP interface, making it easy to set up and adjust without flashing new firmware.

## Features

- Relay control
- HTTP configuration
- Real-time monitoring via web-user-interface
- Huawei SUN2000 support via Modbus-TCP

## Setup

### Prerequisites

- PlatformIO
- ESP8266 board

### 1. Clone the repository

```bash
git clone https://github.com/gsilvan/esp-solar-boy.git
cd esp-solar-boy
```

### 2. Build the project

```bash
pio run
```

### 3. Upload the firmware and the filesystem image

```bash
pio run -t upload
pio run -t uploadfs
```

### 4. WiFi settings

- Connect to `esp-solar-boy`
- Connect the device to your home WiFi
- Save and reboot

### 5. ESP Solar Boy configuration

- Navigate to http://solarboy.local via mDNS or use the assigned IP-Address
- Connect your relays to `D0`, `D1`, `D2`
- Configure settings and relay-pins via web-ui

## License

MIT
