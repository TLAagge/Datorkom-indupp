#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h>

// =========================
// Wi-Fi
// =========================
const char* WIFI_SSID = "<WiFi-SSID>";
const char* WIFI_PASSWORD = "<WiFi-Password>";

// =========================
// MQTT
// =========================
const char* MQTT_SERVER = "RaspberryPi-Ip-Address";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "iot/esp32-02/temperature";
const char* DEVICE_ID = "esp32-02";

// =========================
// Time
// =========================
const char* NTP_SERVER = "pool.ntp.org";
const char* STOCKHOLM_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

// =========================
// DHT11
// =========================
#define DHT_PIN 4
#define DHTTYPE DHT11

DHT dht(DHT_PIN, DHTTYPE);

// =========================
// Timing
// =========================
const unsigned long POST_INTERVAL_MS = 3000;
unsigned long lastPostMs = 0;

// =========================
// MQTT client
// =========================
WiFiClient espClient;
PubSubClient mqttClient(espClient);


// =========================
// Connect to Wi-Fi
// =========================
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

  // Stockholm time
  configTzTime(
    STOCKHOLM_TZ,
    NTP_SERVER
  );

  struct tm timeInfo;

  while (!getLocalTime(&timeInfo)) {
    Serial.println("Waiting for NTP time...");
    delay(500);
  }

  Serial.println("NTP time synchronized.");
}


// =========================
// Connect to MQTT
// =========================
void connectMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Connecting to MQTT...");

    String clientId = DEVICE_ID;
    clientId += "-";
    clientId += String(
      (uint32_t)ESP.getEfuseMac(),
      HEX
    );

    if (mqttClient.connect(clientId.c_str())) {

      Serial.println("connected.");

    } else {

      Serial.print("failed, state=");
      Serial.println(mqttClient.state());

      delay(2000);
    }
  }
}


// =========================
// Get Stockholm timestamp
// =========================
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

  // Convert +0200 -> +02:00
  timestamp =
    timestamp.substring(
      0,
      timestamp.length() - 2
    )
    + ":"
    + timestamp.substring(
      timestamp.length() - 2
    );

  return timestamp;
}


// =========================
// Publish temperature
// =========================
void publishTemperature() {

  float temperature = dht.readTemperature();

  if (isnan(temperature)) {

    Serial.println(
      "Failed to read temperature from DHT11."
    );

    return;
  }

  String timestamp = getTimestamp();

  // JSON payload
  String payload = "{";

  payload += "\"deviceId\":\"";
  payload += DEVICE_ID;

  payload += "\",\"measurement\":\"temperature\"";

  payload += ",\"value\":";
  payload += String(temperature, 1);

  payload += ",\"unit\":\"C\"";

  payload += ",\"timestamp\":\"";
  payload += timestamp;
  payload += "\"";

  payload += "}";

  Serial.println();
  Serial.println("Publishing MQTT:");
  Serial.println(payload);

  bool success =
    mqttClient.publish(
      MQTT_TOPIC,
      payload.c_str()
    );

  if (success) {

    Serial.println(
      "MQTT message sent successfully."
    );

  } else {

    Serial.println(
      "MQTT message failed."
    );
  }
}


// =========================
// Setup
// =========================
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 #2 DHT11 Temperature MQTT");
  Serial.println("==============================");

  dht.begin();

  connectWiFi();

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );
}


// =========================
// Main loop
// =========================
void loop() {

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  if (millis() - lastPostMs >= POST_INTERVAL_MS) {

    lastPostMs = millis();

    publishTemperature();
  }

  delay(100);
}