package com.tuguldur.consumer;

import com.azure.core.util.IterableStream;
import com.azure.messaging.eventhubs.EventHubConsumerClient;
import com.azure.messaging.eventhubs.EventHubClientBuilder;
import com.azure.messaging.eventhubs.models.EventPosition;
import com.azure.messaging.eventhubs.models.PartitionEvent;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;

public class AzureConsumer {

    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    public static void main(String[] args) throws Exception {

        String connectionString =
                System.getenv("IOTHUB_EVENTHUB_CONNECTION_STRING");

        if (connectionString == null || connectionString.isBlank()) {
            throw new IllegalStateException(
                    "IOTHUB_EVENTHUB_CONNECTION_STRING is not set."
            );
        }

        System.out.println("Connecting to Azure IoT Hub Event Hub endpoint...");

        EventHubConsumerClient consumer = new EventHubClientBuilder()
                .connectionString(connectionString)
                .consumerGroup(EventHubClientBuilder.DEFAULT_CONSUMER_GROUP_NAME)
                .buildConsumerClient();

        System.out.println("Connected.");

        IterableStream<String> partitionIds =
                consumer.getPartitionIds();

        for (String partitionId : partitionIds) {

            System.out.println(
                    "Listening to Azure partition: " + partitionId
            );

            Thread thread = new Thread(() -> {

                try {
                    while (true) {

                        IterableStream<PartitionEvent> events =
                                consumer.receiveFromPartition(
                                        partitionId,
                                        100,
                                        EventPosition.latest()
                                );

                        for (PartitionEvent event : events) {
                            handleEvent(event);
                        }
                    }

                } catch (Exception e) {
                    System.err.println(
                            "Partition "
                                    + partitionId
                                    + " error: "
                                    + e.getMessage()
                    );
                }

            });

            thread.setDaemon(true);
            thread.start();
        }

        System.out.println();
        System.out.println("Azure consumer is running.");
        System.out.println("Waiting for telemetry...");
        System.out.println();

        Thread.currentThread().join();
    }

    private static void handleEvent(PartitionEvent event) {

        String payload =
                event.getData().getBodyAsString();

        System.out.println();
        System.out.println("================================");
        System.out.println("Azure telemetry received:");
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
                    "Could not parse Azure message: "
                            + e.getMessage()
            );
        }

        System.out.println("================================");
    }
}
