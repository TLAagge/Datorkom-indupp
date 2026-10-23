#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// WiFi
const char* WIFI_SSID = "WiFi-SSID";
const char* WIFI_PASSWORD = "WiFi-Password";

// MQTT
const char* MQTT_SERVER = "RaspberryPi-Ip-Address";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "iot/esp32-01/water";
const char* DEVICE_ID = "esp32-01";

// Time
const char* NTP_SERVER = "pool.ntp.org";
const char* STOCKHOLM_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

// Water sensor
const int WATER_ANALOG_PIN = 34;
const int LED_PIN = 2;
const int WET_THRESHOLD_ON = 600;
const int WET_THRESHOLD_OFF = 500;
const int DEBOUNCE_SAMPLES = 3;
const unsigned long SAMPLE_DELAY_MS = 100;
const unsigned long POST_INTERVAL_MS = 3000;

// MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Water state
bool waterDetected = false;
int wetSamples = 0;
int drySamples = 0;
unsigned long lastPostMs = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  configTzTime(STOCKHOLM_TZ, NTP_SERVER);

  struct tm timeInfo;

  while (!getLocalTime(&timeInfo)) {
    Serial.println("Waiting for NTP time...");
    delay(500);
  }

  Serial.println("NTP time synchronized.");
}

void connectMQTT() {
  while (!mqttClient.connected()) {

    Serial.print("Connecting to MQTT...");

    String clientId = DEVICE_ID;
    clientId += "-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected.");
    } else {
      Serial.print("failed, state=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void updateLeakState(int analogValue) {

  bool aboveOnThreshold =
      analogValue >= WET_THRESHOLD_ON;

  bool belowOffThreshold =
      analogValue <= WET_THRESHOLD_OFF;

  if (!waterDetected) {

    if (aboveOnThreshold) {
      wetSamples++;
    } else {
      wetSamples = 0;
    }

    if (wetSamples >= DEBOUNCE_SAMPLES) {
      waterDetected = true;
      wetSamples = 0;

      Serial.println("WATER DETECTED!");
    }

  } else {

    if (belowOffThreshold) {
      drySamples++;
    } else {
      drySamples = 0;
    }

    if (drySamples >= DEBOUNCE_SAMPLES) {
      waterDetected = false;
      drySamples = 0;

      Serial.println("Water cleared.");
    }
  }
}

String getTimestamp() {

  struct tm timeInfo;

  if (!getLocalTime(&timeInfo)) {
    return "";
  }

  char timestampBuffer[32];

  strftime(
      timestampBuffer,
      sizeof(timestampBuffer),
      "%Y-%m-%dT%H:%M:%S%z",
      &timeInfo
  );

  String timestamp(timestampBuffer);

  timestamp =
      timestamp.substring(0, timestamp.length() - 2)
      + ":"
      + timestamp.substring(timestamp.length() - 2);

  return timestamp;
}

void publishMeasurement() {

  String timestamp = getTimestamp();

  String payload = "{";

  payload += "\"deviceId\":\"";
  payload += DEVICE_ID;

  payload += "\",\"measurement\":\"water\"";

  payload += ",\"value\":";
  payload += waterDetected ? "1" : "0";

  payload += ",\"unit\":\"boolean\"";

  payload += ",\"timestamp\":\"";
  payload += timestamp;
  payload += "\"";

  payload += "}";

  Serial.println("Publishing MQTT:");
  Serial.println(payload);

  bool success =
      mqttClient.publish(
          MQTT_TOPIC,
          payload.c_str()
      );

  if (success) {
    Serial.println("MQTT message sent successfully.");
  } else {
    Serial.println("MQTT message failed.");
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(WATER_ANALOG_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 #1 Water Sensor MQTT");
  Serial.println("==============================");

  connectWiFi();

  mqttClient.setServer(
      MQTT_SERVER,
      MQTT_PORT
  );
}

void loop() {

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  int analogValue =
      analogRead(WATER_ANALOG_PIN);

  updateLeakState(analogValue);

  digitalWrite(
      LED_PIN,
      waterDetected ? HIGH : LOW
  );

  Serial.print("Analog value: ");
  Serial.print(analogValue);

  Serial.print(" | Water: ");

  Serial.println(
      waterDetected
          ? "DETECTED"
          : "DRY"
  );

  if (millis() - lastPostMs >= POST_INTERVAL_MS) {

    lastPostMs = millis();

    publishMeasurement();
  }

  delay(SAMPLE_DELAY_MS);
}