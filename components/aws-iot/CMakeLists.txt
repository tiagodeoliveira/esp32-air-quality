set(aws_sdk_folder "${CMAKE_CURRENT_LIST_DIR}/aws-iot-device-sdk-embedded-C")

include("${aws_sdk_folder}/libraries/standard/coreJSON/jsonFilePaths.cmake")
include("${aws_sdk_folder}/libraries/standard/coreMQTT/mqttFilePaths.cmake")
include("${aws_sdk_folder}/libraries/standard/backoffAlgorithm/backoffAlgorithmFilePaths.cmake")

set (includes)
set (srcs)
set (priv_includes)

list(APPEND includes
    "${CMAKE_CURRENT_LIST_DIR}/include/"
    "${MQTT_INCLUDE_PUBLIC_DIRS}"
    "${JSON_INCLUDE_PUBLIC_DIRS}"
    "${BACKOFF_ALGORITHM_INCLUDE_PUBLIC_DIRS}"
)

list(APPEND srcs
    "${JSON_SOURCES}"
    "${MQTT_SERIALIZER_SOURCES}"
    "${MQTT_SOURCES}"
    "${BACKOFF_ALGORITHM_SOURCES}"
    "${CMAKE_CURRENT_LIST_DIR}/source/aws_mqtt.c"
    "${CMAKE_CURRENT_LIST_DIR}/source/tls_freertos.c"
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ${includes}
                       PRIV_INCLUDE_DIRS ${priv_includes}
                       REQUIRES lwip nghttp mbedtls tcp_transport freertos)