package com.tuguldur.consumer;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.eclipse.paho.client.mqttv3.*;

public class LocalMqttConsumer {

    private static final String MQTT_BROKER = "tcp://localhost:1883";
    private static final String MQTT_TOPIC = "iot/#";

    private static final ObjectMapper OBJECT_MAPPER =
            new ObjectMapper();

    public static void main(String[] args) throws Exception {

        String clientId = "local-consumer";

        MqttClient mqttClient =
                new MqttClient(MQTT_BROKER, clientId);

        MqttConnectOptions options =
                new MqttConnectOptions();

        options.setAutomaticReconnect(true);
        options.setCleanSession(true);

        System.out.println("Connecting to MQTT broker...");

        mqttClient.connect(options);

        System.out.println("Connected to MQTT broker.");
        System.out.println("Subscribing to: " + MQTT_TOPIC);

        mqttClient.subscribe(MQTT_TOPIC, (topic, message) -> {

            String payload =
                    new String(message.getPayload());

            System.out.println();
            System.out.println("================================");
            System.out.println("Local MQTT telemetry received:");
            System.out.println("Topic: " + topic);
            System.out.println("Payload: " + payload);

            try {

                JsonNode json =
                        OBJECT_MAPPER.readTree(payload);

                String deviceId =
                        json.path("deviceId").asText();

                double value =
                        json.path("value").asDouble();

                String unit =
                        json.path("unit").asText();

                System.out.println("Device: " + deviceId);
                System.out.println("Value: " + value);
                System.out.println("Unit: " + unit);

            } catch (Exception e) {

                System.err.println(
                        "Could not parse MQTT message: "
                                + e.getMessage()
                );
            }

            System.out.println("================================");
        });

        System.out.println();
        System.out.println("Local MQTT consumer is running.");
        System.out.println("Waiting for telemetry...");
        System.out.println();

        Thread.currentThread().join();
    }
}
