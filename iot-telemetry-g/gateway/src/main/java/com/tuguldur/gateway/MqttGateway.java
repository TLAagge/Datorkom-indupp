package com.tuguldur.gateway;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.microsoft.azure.sdk.iot.device.DeviceClient;
import com.microsoft.azure.sdk.iot.device.IotHubClientProtocol;
import com.microsoft.azure.sdk.iot.device.Message;
import org.eclipse.paho.client.mqttv3.*;

public class MqttGateway {

    private static final String MQTT_BROKER = "tcp://localhost:1883";
    private static final String MQTT_TOPIC = "iot/#";

    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    public static void main(String[] args) throws Exception {

        String connectionString = System.getenv("IOTHUB_DEVICE_CONNECTION_STRING");

        if (connectionString == null || connectionString.isBlank()) {
            throw new IllegalStateException(
                    "IOTHUB_DEVICE_CONNECTION_STRING is not set."
            );
        }

        // Connect to Azure IoT Hub
        System.out.println("Connecting to Azure IoT Hub...");

        DeviceClient azureClient = new DeviceClient(
                connectionString,
                IotHubClientProtocol.MQTT
        );

        azureClient.open(false);

        System.out.println("Connected to Azure IoT Hub.");

        // Connect to local MQTT broker
        String clientId = "iot-gateway";

        MqttClient mqttClient = new MqttClient(MQTT_BROKER, clientId);

        MqttConnectOptions options = new MqttConnectOptions();
        options.setAutomaticReconnect(true);
        options.setCleanSession(true);

        System.out.println("Connecting to MQTT broker...");

        mqttClient.connect(options);

        System.out.println("Connected to MQTT broker.");
        System.out.println("Subscribing to: " + MQTT_TOPIC);

        mqttClient.subscribe(MQTT_TOPIC, (topic, message) -> {

            String payload = new String(message.getPayload());

            System.out.println();
            System.out.println("MQTT message received:");
            System.out.println("Topic: " + topic);
            System.out.println("Payload: " + payload);

            try {
                JsonNode json = OBJECT_MAPPER.readTree(payload);

                String deviceId = json.path("deviceId").asText();
                double value = json.path("value").asDouble();
                String unit = json.path("unit").asText();

                System.out.println("Device: " + deviceId);
                System.out.println("Value: " + value);
                System.out.println("Unit: " + unit);

                // Forward original JSON to Azure IoT Hub
                Message azureMessage = new Message(payload);
                azureMessage.setContentType("application/json");
                azureMessage.setProperty("deviceId", deviceId);
                azureMessage.setProperty("unit", unit);

                azureClient.sendEvent(azureMessage);

                System.out.println("Forwarded to Azure IoT Hub.");

            } catch (Exception e) {
                System.err.println("Error processing message: " + e.getMessage());
            }
        });

        System.out.println();
        System.out.println("Gateway is running.");
        System.out.println("Waiting for MQTT messages...");

        Thread.currentThread().join();
    }
}