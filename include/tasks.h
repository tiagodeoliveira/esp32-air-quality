#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "esp_event.h"
#include "esp_log.h"

#include "driver/adc.h"
#include "driver/gpio.h"

#define US_TO_MS 1000000

#define READ_DELAY_IN_MS 50
#define READ_SAMPLES 10
#define SLEE_TIME (US_TO_MS * 15)

#define DHT_PIN GPIO_NUM_4
#define LDR_PIN ADC1_CHANNEL_7
#define GAS_A_PIN ADC1_CHANNEL_5
#define POWER_PIN ADC1_CHANNEL_6
#define CO2_TX_PIN GPIO_NUM_22
#define CO2_RX_PIN GPIO_NUM_25

#define CO2_TASK_BIT BIT0
#define DHT_TASK_BIT BIT1
#define GAS_TASK_BIT BIT2
#define LDR_TASK_BIT BIT3
#define POWER_TASK_BIT BIT4
#define MQTT_TASK_BIT BIT5

typedef struct {
  int ppm;
  int temperature;
} co2_level;

typedef struct {
  int level;
} gas_level;

typedef struct {
  int volts;
} power_level;

typedef struct {
  int light;
} ldr_level;

typedef struct {
  int temperature;
  int humidity;
} dht_level;

typedef struct {
  power_level power;
  ldr_level ldr;
  dht_level dht;
  gas_level gas;
  co2_level co2;
  int uptime;
  EventGroupHandle_t tasks_event;
} task_results;

typedef struct {
  task_results *results;
  char *cert;
  char *key;
  char *thing_name;
  char *mqtt_host;
  char *root_ca;
  int mqtt_port;
} mqtt_params;

void co2_task(void *param);
void dht_task(void *param);
void ldr_task(void *param);
void gas_task(void *param);
void power_task(void *param);
void mqtt_task(void *param);

int udp_logging_init(const char *ipaddr, unsigned long port,
                     vprintf_like_t func);
int udp_logging_vprintf(const char *str, va_list l);
void udp_logging_free(va_list l);