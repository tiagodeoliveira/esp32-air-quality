#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "aws_mqtt.h"
#include "backoff_algorithm.h"
#include "core_mqtt.h"
#include "tls_freertos.h"

#define RETRY_MAX_ATTEMPTS (5U)
#define RETRY_MAX_BACKOFF_DELAY_MS (5000U)
#define RETRY_BACKOFF_BASE_MS (500U)

#define MILLISECONDS_PER_SECOND (1000U)
#define MILLISECONDS_PER_TICK (MILLISECONDS_PER_SECOND / configTICK_RATE_HZ)

static uint32_t get_time_in_ms(void);
static TlsTransportStatus_t
connect_to_broker_with_backoff(NetworkCredentials_t *network_credentials,
                               NetworkContext_t *network_context,
                               const char *mqtt_url, const int mqtt_port);
static uint32_t ulGlobalEntryTimeMs;

MQTTStatus_t connect_to_broker(MQTTContext_t *mqtt_context,
                               NetworkContext_t *network_context,
                               MQTTEventCallback_t event_callback,
                               MQTTFixedBuffer_t *mqtt_buffer,
                               const char *mqtt_url, const int mqtt_port,
                               const char *root_ca, char *cert, char *key,
                               char *thing_name) {
  LogInfo(("Connecting to AWS MQTT Broker [%s:%d] with name [%s]", mqtt_url,
           mqtt_port, thing_name));

  MQTTStatus_t ret = MQTTIllegalState;
  TlsTransportStatus_t network_status;
  TransportInterface_t network_transport;
  NetworkCredentials_t network_credentials = {0};
  MQTTConnectInfo_t mqtt_connection_info = {0};
  bool mqtt_session_present;

  network_credentials.pRootCa = root_ca;
  network_credentials.rootCaSize = strlen(root_ca);
  network_credentials.pClientCert = cert;
  network_credentials.clientCertSize = strlen(cert);
  network_credentials.pPrivateKey = key;
  network_credentials.privateKeySize = strlen(key);

  LogDebug(("client cert %s", network_credentials.pClientCert));

  network_transport.pNetworkContext = network_context;
  network_transport.send = TLS_FreeRTOS_send;
  network_transport.recv = TLS_FreeRTOS_recv;

  network_status = connect_to_broker_with_backoff(
      &network_credentials, network_context, mqtt_url, mqtt_port);
  if (network_status != TLS_TRANSPORT_SUCCESS) {
    LogError(("TLS Connection failed failed [%d]", network_status));
    return ret;
  }

  ret = MQTT_Init(mqtt_context, &network_transport, get_time_in_ms,
                  event_callback, mqtt_buffer);
  if (ret != MQTTSuccess) {
    LogError(("MQTT_Init failed [%d]", ret));
    return ret;
  }

  mqtt_connection_info.cleanSession = true;
  mqtt_connection_info.pClientIdentifier = thing_name;
  mqtt_connection_info.clientIdentifierLength = (uint16_t)strlen(thing_name);
  mqtt_connection_info.keepAliveSeconds = 20;

  ret = MQTT_Connect(mqtt_context, &mqtt_connection_info, NULL, 10000,
                     &mqtt_session_present);
  if (ret != MQTTSuccess) {
    LogError(("MQTT_Connect failed [%d]", ret));
    return ret;
  }

  LogInfo(("Connection to AWS MQTT Broker succeded!"));
  return ret;
}

MQTTStatus_t publish_message(MQTTContext_t *mqtt_context, const char *topic,
                             const char *payload, MQTTQoS_t qos) {
  LogInfo(("Publishing to %s.", topic));

  MQTTPublishInfo_t mqtt_publish_info;
  (void)memset((void *)&mqtt_publish_info, 0x00, sizeof(mqtt_publish_info));

  uint16_t package_id = MQTT_GetPacketId(mqtt_context);
  mqtt_publish_info.qos = qos;
  mqtt_publish_info.retain = false;
  mqtt_publish_info.pTopicName = topic;
  mqtt_publish_info.topicNameLength = (uint16_t)strlen(topic);
  mqtt_publish_info.pPayload = payload;
  mqtt_publish_info.payloadLength = strlen(payload);

  LogInfo(("Sending %d bytes.", mqtt_publish_info.payloadLength));

  MQTTStatus_t ret = MQTT_Publish(mqtt_context, &mqtt_publish_info, package_id);

  if (ret != MQTTSuccess) {
    LogError(("MQTT_Publish failed. Error %d.", ret));
  }

  return ret;
}

MQTTStatus_t subscribe_to_topic(MQTTContext_t *mqtt_context, char *topics[],
                                int topics_count, MQTTQoS_t qos) {
  LogInfo(("Subscribing to %d topics.", topics_count));

  uint16_t package_id = MQTT_GetPacketId(mqtt_context);

  MQTTSubscribeInfo_t mqtt_subscription[topics_count];
  (void)memset((void *)&mqtt_subscription, 0x00, sizeof(mqtt_subscription));

  for (int i = 0; i < topics_count; i++) {
    mqtt_subscription[i].qos = qos;
    mqtt_subscription[i].pTopicFilter = topics[i];
    mqtt_subscription[i].topicFilterLength = (uint16_t)strlen(topics[i]);
  }

  int mqtt_subscription_size =
      sizeof(mqtt_subscription) / sizeof(MQTTSubscribeInfo_t);
  MQTTStatus_t ret = MQTT_Subscribe(mqtt_context, mqtt_subscription,
                                    mqtt_subscription_size, package_id);
  if (ret != MQTTSuccess) {
    LogError(("MQTT_Subscribe failed. Error %d.", ret));
  }

  return ret;
}

void disconnect_from_broker(MQTTContext_t *mqtt_context,
                            NetworkContext_t *network_context) {
  LogInfo(("Disconnecting from broker"));
  MQTT_Disconnect(mqtt_context);
  TLS_FreeRTOS_Disconnect(network_context);
}

static TlsTransportStatus_t
connect_to_broker_with_backoff(NetworkCredentials_t *network_credentials,
                               NetworkContext_t *network_context,
                               const char *mqtt_url, const int mqtt_port) {
  TlsTransportStatus_t network_status;
  BackoffAlgorithmStatus_t retry_status = BackoffAlgorithmSuccess;
  BackoffAlgorithmContext_t retry_params;
  uint16_t next_retry_backoff = 0;

  BackoffAlgorithm_InitializeParams(&retry_params, RETRY_BACKOFF_BASE_MS,
                                    RETRY_MAX_BACKOFF_DELAY_MS,
                                    RETRY_MAX_ATTEMPTS);
  do {
    network_status = TLS_FreeRTOS_Connect(network_context, mqtt_url, mqtt_port,
                                          network_credentials, 5000, 5000);

    if (network_status != TLS_TRANSPORT_SUCCESS) {
      LogWarn(("Connection to the broker failed. Retrying connection with "
               "backoff and jitter."));
      retry_status = BackoffAlgorithm_GetNextBackoff(&retry_params, rand(),
                                                     &next_retry_backoff);
      (void)usleep(next_retry_backoff * 1000U);
    }

  } while ((network_status != TLS_TRANSPORT_SUCCESS) &&
           (retry_status != BackoffAlgorithmRetriesExhausted));

  return network_status;
}

static uint32_t get_time_in_ms(void) {
  TickType_t xTickCount = 0;
  uint32_t ulTimeMs = 0UL;

  /* Get the current tick count. */
  xTickCount = xTaskGetTickCount();

  /* Convert the ticks to milliseconds. */
  ulTimeMs = (uint32_t)xTickCount * MILLISECONDS_PER_TICK;

  /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
   * elapsed time in the application. */
  ulTimeMs = (uint32_t)(ulTimeMs - ulGlobalEntryTimeMs);

  return ulTimeMs;
}
