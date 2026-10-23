# IoT Telemetry System (ESP32 → MQTT → Raspberry Pi Gateway → Azure IoT Hub)

Individual IoT assignment (G level). Two ESP32 devices publish JSON telemetry over Wi-Fi using MQTT to a Mosquitto broker on a Raspberry Pi. A Java gateway on the Pi forwards every message to Azure IoT Hub. Two Java console consumers display the telemetry: one locally from MQTT and one from Azure.

## 1. Architecture

```
ESP32 #1 (water sensor) ──┐  Wi-Fi / MQTT                       ┌─► Local consumer (Java, console)
                          ├─► Mosquitto on Raspberry Pi :1883 ──┤
ESP32 #2 (DHT11)        ──┘                                     └─► Gateway (Java) ──► Azure IoT Hub ──► Azure consumer (Java, console)
                                                                      (gateway device identity)           (Event Hub-compatible endpoint)
```

- The ESP32 devices only know Wi-Fi and the MQTT broker. They have **no** Azure credentials.
- The gateway is the only component with an Azure IoT Hub device identity.
- The gateway and local consumer both subscribe to `iot/#`.

## 2. Repository structure

```
iot-telemetry-g/
├── README.md
├── .gitignore
├── esp32/
│   ├── esp32-01-water/esp32-01-water.ino
│   └── esp32-02-dht11/esp32-02-dht11.ino
├── gateway/          (pom.xml, src/main/java/com/tuguldur/gateway/MqttGateway.java)
├── local-consumer/   (pom.xml, src/main/java/com/tuguldur/consumer/LocalMqttConsumer.java)
└── azure-consumer/   (pom.xml, src/main/java/com/tuguldur/consumer/AzureConsumer.java)
```

## 3. Hardware

| Device | Board | Sensor | Pin (from code) |
|---|---|---|---|
| ESP32 #1 | ESP32 | Analog water/leak sensor | Sensor signal → GPIO 34 (analog input); on-board LED on GPIO 2 |
| ESP32 #2 | ESP32 | DHT11 temperature sensor | DATA → GPIO 4 |
| Gateway host | Raspberry Pi | – | Runs Mosquitto, gateway (and optionally the consumers) |

**ESP32 #1 behaviour** (`esp32-01-water.ino`): reads the analog value every 100 ms. Water is flagged when the reading is `>= 600` for 3 consecutive samples and cleared when it is `<= 500` for 3 consecutive samples (hysteresis + debounce). The LED on GPIO 2 is on while water is detected. A message is published every 3 seconds.

**ESP32 #2 wiring** (`esp32-02-dht11.ino`):

```
DHT11 VCC  -> ESP32 3V3
DHT11 DATA -> GPIO 4
DHT11 GND  -> ESP32 GND
```

(Only DATA→GPIO 4 is defined in the code; the power pins follow the standard DHT11 connection.) The temperature is read with `dht.readTemperature()` and published every 3 seconds. If a read fails, nothing is published and an error is printed.

For the water sensor, the signal output is connected to GPIO 34. Connect VCC and GND according to the specifications of the specific water sensor module being used.

## 4. MQTT topics and device IDs

| Device | Device ID | MQTT topic | MQTT client ID |
|---|---|---|---|
| ESP32 #1 water | `esp32-01` | `iot/esp32-01/water` | `esp32-01-<chip MAC hex>` |
| ESP32 #2 DHT11 | `esp32-02` | `iot/esp32-02/temperature` | `esp32-02-<chip MAC hex>` |

Broker port: `1883` (no authentication or TLS in the code). Both sketches use the same Wi-Fi/MQTT placeholder constants (see section 6).

## 5. JSON telemetry format

Both devices send the same fields:

| Field | Meaning |
|---|---|
| `deviceId` | Unique device ID (`esp32-01` / `esp32-02`) |
| `measurement` | `water` or `temperature` |
| `value` | Water: `0` or `1`. Temperature: float with 1 decimal |
| `unit` | Water: `boolean`. Temperature: `C` |
| `timestamp` | Local Stockholm time (ISO 8601 with offset, e.g. `+02:00`), taken from NTP (`pool.ntp.org`, TZ `CET-1CEST,M3.5.0,M10.5.0/3`) |

Water (`value` 0 = no water detected, 1 = water detected):

```json
{"deviceId":"esp32-01","measurement":"water","value":0,"unit":"boolean","timestamp":"2026-01-01T12:00:00+01:00"}
```

Temperature (format produced by the DHT11 sketch; example value is illustrative):

```json
{"deviceId":"esp32-02","measurement":"temperature","value":22.0,"unit":"C","timestamp":"2026-01-01T12:00:00+01:00"}
```

The timestamp is an empty string if NTP time is unavailable; the sketches wait for NTP sync at start-up.

## 6. ESP32 setup (both devices)

Each sketch contains placeholders that **must be configured locally** before upload:

```cpp
const char* WIFI_SSID     = "...";   // water: "WiFi-SSID",      DHT11: "<WiFi-SSID>"
const char* WIFI_PASSWORD = "...";   // water: "WiFi-Password",  DHT11: "<WiFi-Password>"
const char* MQTT_SERVER   = "RaspberryPi-Ip-Address";
```

Replace them with your Wi-Fi name, Wi-Fi password and the Raspberry Pi's current LAN IP. **Never commit the edited values** (see section 12).

Steps:

1. Install the Arduino IDE.
2. Install ESP32 board support (Boards Manager → "esp32" by Espressif).
3. Install libraries (Library Manager):
   - ESP32 #1: **PubSubClient** (WiFi and time are part of the ESP32 core).
   - ESP32 #2: **PubSubClient**, **DHT sensor library** (Adafruit; it also asks for *Adafruit Unified Sensor*).
4. Open `esp32/esp32-01-water/esp32-01-water.ino` or `esp32/esp32-02-dht11/esp32-02-dht11.ino`.
5. Set the Wi-Fi SSID, password and Raspberry Pi IP.
6. Connect the ESP32 with a **data-capable** USB cable.
7. Select your ESP32 board and COM port in the Arduino IDE.
8. Upload the sketch.
9. Open Serial Monitor at **115200 baud**.
10. Check for `WiFi connected.` and `NTP time synchronized.`
11. Check for `Connecting to MQTT...connected.`
12. Check for `Publishing MQTT:` followed by the JSON and `MQTT message sent successfully.`

## 7. Raspberry Pi / Mosquitto setup

The Pi runs the Mosquitto broker on port `1883`. Java apps on the Pi connect to `tcp://localhost:1883` (hard-coded); the ESP32s connect to the Pi's LAN IP.

```bash
sudo apt update && sudo apt install -y mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto
systemctl status mosquitto
hostname -I                      # current Pi IP -> use as MQTT_SERVER
mosquitto_sub -h localhost -t 'iot/#' -v    # watch all telemetry
```

The Pi's IP depends on the network and may change; replace `RaspberryPi-Ip-Address` in the sketches with the current IP. Depending on the Raspberry Pi and network setup, Mosquitto 2.x may require a listener configuration to allow ESP32 devices on the LAN to connect.

## 8. Java applications

Requirements: **Java 21** and **Maven**. Dependencies (from the `pom.xml` files):

| Module | Main class | Libraries |
|---|---|---|
| `gateway` | `com.tuguldur.gateway.MqttGateway` | Eclipse Paho MQTT 1.2.5, Azure `iot-device-client` 2.5.1, Jackson 2.20.0, slf4j-simple 2.0.17 |
| `local-consumer` | `com.tuguldur.consumer.LocalMqttConsumer` | Paho MQTT 1.2.5, Jackson 2.20.0, slf4j-simple 2.0.17 |
| `azure-consumer` | `com.tuguldur.consumer.AzureConsumer` | `azure-messaging-eventhubs` 5.21.3, Jackson 2.20.0, slf4j-simple 2.0.17 |

Compile (in each module folder): `mvn clean compile`

Run (in each module folder; the poms use `exec-maven-plugin` without a preset main class):

```bash
cd gateway         && mvn exec:java -Dexec.mainClass=com.tuguldur.gateway.MqttGateway
cd local-consumer  && mvn exec:java -Dexec.mainClass=com.tuguldur.consumer.LocalMqttConsumer
cd azure-consumer  && mvn exec:java -Dexec.mainClass=com.tuguldur.consumer.AzureConsumer
```

- **Gateway**: subscribes to `iot/#` (client ID `iot-gateway`, auto-reconnect), parses `deviceId`, `value`, `unit`, and sends the **original JSON payload** to Azure IoT Hub as `application/json` with message properties `deviceId` and `unit`. So all fields (`measurement`, `timestamp`, …) are preserved.
- **Local consumer**: subscribes to `iot/#` (client ID `local-consumer`) and prints device, value and unit for each message.
- **Azure consumer**: reads all partitions of the IoT Hub Event Hub-compatible endpoint (default consumer group, only new events, no checkpointing) and prints device, value and unit.

## 9. Azure setup and environment variables

The gateway authenticates to IoT Hub with **its own device identity**. The ESP32s have no Azure identity. The Azure consumer uses the IoT Hub's Event Hub-compatible endpoint.

| Variable | Used by | Value |
|---|---|---|
| `IOTHUB_DEVICE_CONNECTION_STRING` | `gateway` | Device connection string of the gateway's IoT Hub device |
| `IOTHUB_EVENTHUB_CONNECTION_STRING` | `azure-consumer` | IoT Hub Event Hub-compatible endpoint connection string |

PowerShell:

```powershell
$env:IOTHUB_DEVICE_CONNECTION_STRING="<your-device-connection-string>"
$env:IOTHUB_EVENTHUB_CONNECTION_STRING="<your-eventhub-compatible-connection-string>"
```

Linux/macOS:

```bash
export IOTHUB_DEVICE_CONNECTION_STRING="<your-device-connection-string>"
export IOTHUB_EVENTHUB_CONNECTION_STRING="<your-eventhub-compatible-connection-string>"
```

> **Azure resources are no longer active.** The Azure part was tested during development; the IoT Hub and related resources were deleted afterwards as instructed by the teacher. To reproduce it, create a new IoT Hub, register a device for the gateway, and obtain both connection strings. **No Azure secrets are included in this repository.**

## 10. Running the complete system

1. On the Pi: start Mosquitto (`sudo systemctl start mosquitto`).
2. Set `IOTHUB_DEVICE_CONNECTION_STRING`, then start the gateway.
3. Start the local consumer.
4. If Azure resources exist: set `IOTHUB_EVENTHUB_CONNECTION_STRING` and start the Azure consumer.
5. Power ESP32 #1. ESP32 #2 can be connected and tested after flashing the included DHT11 sketch.
6. Verify messages locally: `mosquitto_sub -h localhost -t 'iot/#' -v`.
7. Verify the gateway prints `MQTT message received:` … `Forwarded to Azure IoT Hub.`
8. Verify the local consumer shows the device, value and unit.
9. If Azure is available, verify the Azure consumer shows the same telemetry.
10. Check that each consumer shows the right device (`esp32-01` water / `esp32-02` temperature).

Without the ESP32 hardware, telemetry can be simulated, e.g.:

```bash
mosquitto_pub -h localhost -t iot/esp32-02/temperature -m '{"deviceId":"esp32-02","measurement":"temperature","value":22.5,"unit":"C","timestamp":"2026-01-01T12:00:00+01:00"}'
```

## 11. Test results

The complete telemetry pipeline was tested successfully during development:

- ESP32 #1 water sensor telemetry was tested with the physical sensor.
- ESP32 #2 DHT11 telemetry implementation was verified through the MQTT device path.
- Both device topics and JSON telemetry formats were verified.
- Raspberry Pi Mosquitto received the telemetry.
- The local MQTT consumer displayed the telemetry.
- The Java gateway received and forwarded telemetry to Azure IoT Hub.
- The Azure consumer received and displayed the telemetry.
- Device IDs, measurement values and units were preserved throughout the pipeline.

The Azure resources were deleted after testing as instructed by the teacher.

## 12. Security / secrets

**Never commit** Wi-Fi passwords, MQTT credentials (if authentication is enabled later), Azure connection strings, shared access keys, certificates/private keys, or `.env` files containing secrets. The ESP32 sketches contain placeholders only; edit them locally and do not commit the edited versions. Azure credentials are only read from environment variables. `.gitignore` excludes `.env`, `.env.*`, `secrets.h` and `arduino_secrets.h`.
