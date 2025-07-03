# ESP32 Home Assistant Integration

This project implements a Home Assistant integration for ESP32 microcontrollers, providing IoT device functionality through MQTT communication. The project demonstrates how to create smart home devices that can be controlled and monitored through Home Assistant.

## Features

- **RGB LED Control**: Full color control with brightness adjustment via MQTT
- **Button Events**: Physical button press/release detection and reporting
- **Potentiometer Sensor**: Analog sensor reading for continuous value monitoring
- **WiFi Connectivity**: Wireless network connection for IoT communication
- **MQTT Integration**: Bi-directional communication with Home Assistant
- **Home Assistant Auto-Discovery**: Automatic device registration in Home Assistant

## Hardware Requirements

- ESP32 development board ( ESP32-C3)
- RGB LED strip or individual RGB LEDs
- Push button
- Potentiometer (analog sensor)
- Appropriate resistors and wiring

## Software Requirements

- ESP-IDF (Espressif IoT Development Framework)
- Home Assistant instance with MQTT broker
- MQTT broker (like Mosquitto)

## Project Structure

```
Final_Project-HomeAssistant/
├── main/
│   ├── main.c                           # Main application logic
│   └── CMakeLists.txt                   # Main component build config
├── components/                          # Custom components
│   ├── led/                             # LED control component
│   ├── buttons/                         # Button handling component
│   ├── potentiometer/                   # Potentiometer reading component
│   ├── mqtt_impl/                       # MQTT implementation
│   ├── wifi_station/                    # WiFi connection component
│   ├── ringbuffer/                      # Ring buffer utility
│   └── filter/                          # Signal filtering utility
├── homeassistant-configuration.yaml     # Home Assistant MQTT config
├── homeassistant-automations.yaml       # Home Assistant automation rules
├── CMakeLists.txt                       # Project build configuration
└── sdkconfig*                           # ESP-IDF configuration files
```

## MQTT Topics and JSON Schemas

### LED Control (`ESP32/led/set`)
Control the RGB LED with color, brightness, and state:
```json
{
  "color": {
    "r": 255,
    "g": 180,
    "b": 200
  },
  "brightness": 128,
  "state": "ON"
}
```

### LED State (`ESP32/led/state`)
Reports current LED state:
```json
{
  "color": {
    "r": 255,
    "g": 180,
    "b": 200
  },
  "brightness": 128,
  "state": "ON"
}
```

### Button Events (`ESP32/button`)
Reports button press and release events:
```json
{
  "gpio": 0,
  "event_type": "press"
}
```

### Potentiometer Reading (`ESP32/potentiometer`)
Continuous analog sensor value:
```json
{
  "value": 142
}
```

## Home Assistant Configuration

### 1. Add MQTT Configuration
Copy the contents of `homeassistant-configuration.yaml` to your Home Assistant `configuration.yaml` file.

### 2. Add Automations
Import the automations from `homeassistant-automations.yaml` or manually create:
- **Toggle LED on Button Press**: Toggles the LED when the physical button is pressed
- **Set LED Brightness from Potentiometer**: Adjusts LED brightness based on potentiometer value

## Building and Flashing

### Prerequisites
1. Install ESP-IDF following the [official guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. Set up your development environment

### Build Process
```bash
# Navigate to project directory
cd Final_Project-HomeAssistant

# Configure the project (set WiFi credentials, MQTT broker settings)
idf.py menuconfig

# Build the project
idf.py build

# Flash to ESP32
idf.py flash

# Monitor serial output
idf.py monitor
```

## Configuration

### Required Settings
This settings are not the default ones and have to be configred as shown, to make the project work and to have the same configuration in the yaml files as in the acctual build output:
- Partition Table: Single factory app (large), no OTA
- Button Configuration > Potentiomenter active: true
- MQTT Configuration > MQTT topic prefix: ESP32

### WiFi Settings
Configure your WiFi credentials through `idf.py menuconfig`:
- WiFi SSID
- WiFi Password

### MQTT Settings
Configure MQTT broker connection:
- MQTT Broker IP/Hostname
- MQTT Port (default: 1883)
- MQTT Username/Password (if required)
- MQTT Client ID

## Hardware Connections

### Typical GPIO Assignments
- **Button**: GPIO 0 (Boot button on most ESP32 boards)
- **RGB LED**: Configure in LED component (typically GPIO 2, 4, 5)
- **Potentiometer**: ADC1 channel (GPIO 36 on ESP32)

*Note: Exact GPIO assignments may vary based on your specific hardware setup and can be configured in the component headers.*

## Features in Detail

### LED Control
- Full RGB color control (0-255 per channel)
- Brightness control (0-255)
- On/Off state management
- Real-time state feedback to Home Assistant

### Button Handling
- Debounced button press detection
- Press and release event reporting
- Configurable GPIO assignment

### Potentiometer Monitoring
- Continuous analog reading
- Configurable sampling rate (default: 500ms)
- 8-bit value reporting (0-255)

### MQTT Communication
- Automatic reconnection handling
- QoS level support
- Topic subscription management
- JSON payload formatting

## Home Assistant Entity IDs

After successful integration, the following entities will be available:
- `light.esp32_light` - RGB LED control
- `event.esp32_button` - Button press events
- `sensor.esp32_potentiometer` - Potentiometer readings

## License

MIT License.
See the LICENSE file for further information.

## Author

Loß Philipp
