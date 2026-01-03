# Repository Overview

## Project Description
**MicroHomebridgeAlexaEsp8266** is an ESP8266-based project designed to act as a bridge for controlling RF switches and IR modules via Amazon Alexa. It implements a Homebridge-compatible server directly on the ESP8266, eliminating the need for a separate Homebridge server (like a Raspberry Pi). It essentially replaces `fauxmoesp` but includes precompiled options to avoid manual compilation hassles for end-users.

**Key Goals:**
- Control 433MHz RF switches and IR devices via Alexa voice commands.
- Provide a web interface for configuration and management.
- Operate directly on ESP8266 hardware (D1 Mini, NodeMCU, ESP-01).

**Key Technologies:**
- **Platform:** ESP8266 / Arduino Framework
- **Protocols:** MQTT, HTTP, mDNS
- **Libraries:** ESPAsyncTCP, ESP8266WiFi, IRremoteESP8266, RCSwitch, ArduinoOTA
- **Build System:** PlatformIO (recommended) or Arduino IDE

## Architecture Overview
The system architecture revolves around the ESP8266 acting as a central hub.

- **Main Loop:** Handles WiFi connection, MQTT communication, web server requests, and OTA updates.
- **Web Interface:** A built-in web server (port 80) serves a GUI for configuring devices (switches, IR codes), WiFi settings, and Homebridge credentials. It uses SPIFFS to store web assets (`.gz` files) and configuration (`EEPROM.TXT`, `state.json`).
- **MQTT Client:** Connects to a cloud MQTT broker (defaulting to `homebridge.cloudwatch.net` or a local broker) to receive commands from the Alexa Homebridge skill.
- **Device Control:**
  - **RF Control:** Uses `RCSwitch` library to send 433MHz signals via GPIO.
  - **IR Control:** Uses `IRremoteESP8266` library to send IR signals.
  - **HTTP Requests:** Can trigger external URLs when switches are toggled.
- **Data Flow:** Alexa -> Homebridge Skill -> MQTT Broker -> ESP8266 (MQTT Client) -> Action (RF/IR/HTTP) -> Response (via MQTT).

## Directory Structure
- **MicroHomebridgeAlexaEsp8266/**: Main source code directory.
  - `MicroHomebridgeAlexaEsp8266.ino`: Main entry point. Setup and loop functions.
  - `MiniMqttClient.h` / `.cpp`: Handles MQTT connections and message parsing.
  - `WebInterface.h`: Handles web server requests and API endpoints for configuration.
  - `Estore.h`: Manages persistent storage (EEPROM/SPIFFS) for configuration data.
  - `RemoteControl.h`: Logic for sending RF and IR signals.
  - `data/`: Contains web interface assets (HTML, JS, CSS) to be uploaded to SPIFFS.
- **platformio.ini**: Configuration file for PlatformIO build system.
- **README.md**: User documentation and installation guide.

## Development Workflow
### Build and Run
1. **PlatformIO (Recommended):**
   - Install PlatformIO (VSCode extension or CLI).
   - Open the project directory.
   - Run `pio run` to build.
   - Run `pio run --target upload` to flash the firmware.
   - Run `pio run --target uploadfs` to upload the SPIFFS data (web interface files).

2. **Arduino IDE:**
   - Requires installing ESP8266 core and specific libraries.
   - Not recommended due to dependency management complexity compared to PlatformIO.

### Configuration
- **WiFi & Credentials:** Configured via the web interface (access point `EasyAlexa` on first boot, IP `192.168.4.1`) or directly in `Estore` / `MicroHomebridgeAlexaEsp8266.ino` for development.
- **SPIFFS:** Essential for the web interface. Must be uploaded separately.

### Testing
- **Manual Testing:** Deploy to hardware and use the web interface to toggle switches.
- **Logs:** Serial monitor (115200 baud) provides debug output for WiFi, MQTT, and device actions.

### Linting and Formatting
- Follow standard C++/Arduino coding conventions.
- PlatformIO can handle some linting tasks, but no specific linter config is present in the root.

