#include "freertos/FreeRTOSConfig.h"
#include "logging_levels.h"

#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "AWS_MQTT"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

#define SdkLog( message )    printf message

#include "logging_stack.h"

#include "core_mqtt.h"
#include "tls_freertos.h"

MQTTStatus_t connect_to_broker(MQTTContext_t* mqtt_context, NetworkContext_t* network_context, MQTTEventCallback_t event_callback, MQTTFixedBuffer_t* mqtt_buffer, const char* mqtt_url, const int mqtt_port, const char* root_ca, char* cert, char* key, char* serial_number);
void disconnect_from_broker(MQTTContext_t* mqtt_context, NetworkContext_t* network_context);
MQTTStatus_t publish_message(MQTTContext_t* mqtt_context, const char* topic, const char* payload, MQTTQoS_t qos);
MQTTStatus_t subscribe_to_topic(MQTTContext_t* mqtt_context, char *topics[], int topics_count, MQTTQoS_t qos);