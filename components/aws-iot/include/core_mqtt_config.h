#ifndef CORE_MQTT_CONFIG_H
#define CORE_MQTT_CONFIG_H
#include "logging_levels.h"

#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "CORE_MQTT"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_ERROR
#endif

#define SdkLog( message )    printf message

#include "logging_stack.h"
#endif