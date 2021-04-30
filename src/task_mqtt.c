#include <math.h>

#include "aws_mqtt.h"
#include "tasks.h"

#define NETWORK_BUFFER_SIZE 1024
#define TOPIC_TEMPLATE "device/%s/data"
#define PAYLOAD_TEMPLATE                                                       \
  "{\"dht\": { \"temperature\": %d, \"humidity\": %d }, \"gas\": { "           \
  "\"level\": %d }, \"ldr\": { \"intensity\": %d }, \"co2\": { \"ppm\": %d, "  \
  "\"temperature\": %d }, \"power\": { \"volts\": %d }}"

static const char *TAG = "MQTT";

NetworkContext_t network_context = {0};
MQTTContext_t mqtt_context = {0};
static uint8_t mqtt_shared_buffer[NETWORK_BUFFER_SIZE];
static MQTTFixedBuffer_t mqtt_buffer = {mqtt_shared_buffer,
                                        NETWORK_BUFFER_SIZE};

EventGroupHandle_t event_group;

static void event_callback(MQTTContext_t *pxMQTTContext,
                           MQTTPacketInfo_t *pxPacketInfo,
                           MQTTDeserializedInfo_t *pxDeserializedInfo) {
  ESP_LOGI(TAG, "Response [%d] received for packet Id [%u].",
           pxPacketInfo->type, pxDeserializedInfo->packetIdentifier);

  xEventGroupSetBits(event_group, MQTT_TASK_BIT);
}

void mqtt_task(void *param) {
  mqtt_params *params = (mqtt_params *)param;
  task_results *results = params->results;
  event_group = results->tasks_event;

  connect_to_broker(&mqtt_context, &network_context, event_callback,
                    &mqtt_buffer, params->mqtt_host, params->mqtt_port,
                    params->root_ca, params->cert, params->key,
                    params->thing_name);

  xEventGroupWaitBits(event_group,
                      LDR_TASK_BIT | GAS_TASK_BIT | DHT_TASK_BIT |
                          CO2_TASK_BIT | POWER_TASK_BIT,
                      pdTRUE, pdTRUE, portMAX_DELAY);

  char topic[128];
  char message[1024];
  sprintf(message, PAYLOAD_TEMPLATE, results->dht.temperature,
          results->dht.humidity, results->gas.level, results->ldr.light,
          results->co2.ppm, results->co2.temperature, results->power.volts);
  sprintf(topic, TOPIC_TEMPLATE, params->thing_name);

  ESP_LOGI(TAG, "Publishing reading [%s]", message);

  MQTTStatus_t ret = publish_message(&mqtt_context, topic, message, MQTTQoS1);
  if (ret != MQTTSuccess) {
    ESP_LOGI(TAG, "Failed to send mqtt message: %d", ret);
  }

  do {
    ret = MQTT_ProcessLoop(&mqtt_context, 2000);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  } while (ret == MQTTSuccess);

  vTaskDelete(NULL);
}