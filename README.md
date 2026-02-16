# IoT Plant Monitor

An ESP32-based smart plant monitoring system that tracks environmental conditions (temperature, humidity, soil moisture, light), displays plant health through an expressive OLED face, and publishes telemetry to the cloud via MQTT.

Initial device setup is performed wirelessly over Bluetooth Low Energy from a companion mobile app.

## Features

- **Sensor monitoring** -- BME280 (temperature & humidity), capacitive soil moisture sensor, photoresistor (light detection)
- **Plant health FSM** -- Three emotional states (Happy, Angry, Dying) driven by configurable thresholds and timeouts
- **128x128 OLED display** -- Animated faces reflecting plant health, plus dedicated pages for temperature, humidity, and soil moisture
- **BLE provisioning** -- Zero-config setup via Bluetooth: the companion app sends Wi-Fi credentials and plant thresholds over a Nordic UART Service (NUS)
- **MQTT telemetry** -- Periodic sensor data publishing to a HiveMQ Cloud broker over TLS
- **Factory reset** -- Hold the button for 10 seconds to clear the stored configuration and reboot into BLE pairing mode
- **Dual-core FreeRTOS architecture** -- Display/UI on Core 0, networking and sensors on Core 1, ensuring responsive button handling and stable Wi-Fi

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 (Denky32 / ESP32-DevKitC) |
| Display | SH1107 128x128 monochrome OLED (I2C) |
| Temp/Humidity | Bosch BME280 (I2C) |
| Soil moisture | Capacitive analog sensor (GPIO 34) |
| Light | Photoresistor (analog) |
| Button | Tactile switch on GPIO 32 (internal pull-up) |

### Wiring summary

| Signal | GPIO |
|---|---|
| I2C SDA | 21 |
| I2C SCL | 22 |
| Soil moisture (ADC) | 34 |
| Light sensor (ADC) | 35 |
| Button | 32 |

## Project Structure

```
iot-plant-monitor/
├── include/                     # Global headers (pin config, network template)
├── src/
│   ├── main.cpp                 # Entry point, FreeRTOS task startup
│   ├── drivers/                 # Hardware Abstraction Layers
│   │   ├── bluetooth/           #   BLE UART (NimBLE)
│   │   ├── display/             #   SH1107 OLED
│   │   ├── sensors/             #   Button, light, moisture, temperature
│   │   └── wifi/                #   Wi-Fi connection manager
│   ├── tasks/                   # FreeRTOS tasks
│   │   ├── display/             #   UI rendering + button handling
│   │   ├── iot/                 #   BLE config, Wi-Fi, MQTT FSM
│   │   ├── plant/               #   Plant health state machine
│   │   └── sensor/              #   Periodic sensor reading
│   ├── iot/                     # MQTT telemetry publisher + TLS cert
│   └── utils/                   # Shared utilities
│       ├── bitmap/              #   Display icons (happy, angry, dying, BT)
│       ├── configuration/       #   NVS config storage & JSON parser
│       ├── derivative-filter/   #   Rate-of-change filter
│       ├── moving-average/      #   Circular-buffer moving average
│       └── timer/               #   Thread-safe periodic timer
├── test/                        # Unity test framework
├── platformio.ini               # Build configuration
└── LICENSE.md                   # Apache 2.0
```

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VSCode extension)
- An ESP32 development board
- The sensors and display listed above

### Setup

1. **Clone the repository**

   ```bash
   git clone https://github.com/valedotc/iot-plant-monitor.git
   cd iot-plant-monitor
   ```

2. **Create the private credentials file**

   Copy the template and fill in your MQTT broker credentials:

   ```bash
   cp src/utils/configuration/private-data.h.example src/utils/configuration/private-data.h
   ```

   Edit `private-data.h` with your HiveMQ (or other) broker URL, port, username and password.
   This file is listed in `.gitignore` and will not be committed.

3. **Build and flash**

   ```bash
   pio run -t upload
   ```

4. **Monitor serial output**

   ```bash
   pio device monitor
   ```

### First-time configuration

On first boot (or after a factory reset) the device enters **BLE pairing mode** and shows a Bluetooth icon on the display. Use the companion mobile app to:

1. Connect to the device named **PlantMonitor**
2. Send Wi-Fi credentials (SSID and password)
3. Send plant-specific thresholds (temperature, humidity, moisture ranges, light hours)

Once configured, the device connects to Wi-Fi, starts MQTT telemetry and shows the plant's emotional face on the display.

### Factory Reset

Hold the button for **10 seconds**. A progress bar is shown on the display. Once complete, the stored configuration is erased and the microcontroller reboots into BLE pairing mode.

## Architecture

The system runs three FreeRTOS tasks across the ESP32's two cores:

| Task | Core | Priority | Responsibility |
|---|---|---|---|
| **DisplayTask** | 0 | 3 (highest) | UI rendering, button input, plant state updates |
| **SensorTask** | 1 | 2 | Periodic sensor reads, filtering, shared data |
| **IoTTask** | 1 | 1 | BLE provisioning, Wi-Fi, MQTT telemetry |

### IoT Task FSM

```
Boot ──> BleAdvertising ──> BleConfiguring ──> BleTestingWifi
                                                     │
                                                     v
                        MqttOperating <── WifiConnecting
```

### Plant Health FSM

```
HAPPY ──(sensors out of range for debounce period)──> ANGRY
ANGRY ──(sensors back in range)──> HAPPY
ANGRY ──(out of range beyond timeout)──> DYING
DYING ──(sensors back in range)──> HAPPY
```

Timing parameters are configurable in [plant-config.h](src/tasks/plant/plant-config.h).

## Configuration Parameters

Parameters sent via BLE during provisioning (array indices):

| Index | Parameter | Unit |
|---|---|---|
| 0 | Plant type ID | - |
| 1 | Min temperature | C |
| 2 | Max temperature | C |
| 3 | Min humidity | % |
| 4 | Max humidity | % |
| 5 | Min soil moisture | % |
| 6 | Max soil moisture | % |
| 7 | Min light hours | h |
| 8 | Device ID | - |

## Dependencies

Managed automatically by PlatformIO:

| Library | Version | Purpose |
|---|---|---|
| NimBLE-Arduino | ^1.4.3 | Bluetooth Low Energy |
| Adafruit SH110X | ^2.1.14 | OLED display driver |
| Adafruit BME280 | ^2.3.0 | Temperature & humidity sensor |
| ArduinoMqttClient | ^0.1.8 | MQTT protocol |
| ArduinoJson | ^7.4.2 | JSON serialization |

## License

This project is licensed under the Apache License 2.0. See [LICENSE.md](LICENSE.md) for details.
