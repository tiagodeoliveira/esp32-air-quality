This project is built on top [PlatformIO](https://platformio.org/).

The goal is to use ESP32 devices to collect Air Quality information from multiple sensors and send it to the AWS Cloud Using MQTT.

To build and burn the firmware on ESP32 devices, you need to have [PlatformIO installed](https://platformio.org/install/cli), the commands can be executed either via IDE or CLI:
```
$ # To build the firmware
$ pio run 

$ # To push it to device and monitor the output via USB Serial
$ pio run -t upload -t monitor
```

For MQTT connection, this project leverages [AWS IoT Device SDK for Embedded C](https://github.com/aws/aws-iot-device-sdk-embedded-C).
