#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include "tasks.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "AQ";
static const bool enable_upd_logging = true;
static const char udp_logging_address[20] = "255.255.255.255";
static const int udp_logging_port = 1337;

static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t tasks_event_group;

static void init_system(void) {
  esp_err_t ret = nvs_flash_init();
  if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
      (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    ESP_LOGI(TAG, "Wifi started");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "Wifi disconnected");
    xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  } else {
    ESP_LOGW(TAG, "Unhandled event [%s: %d]", event_base, event_id);
  }
}

static void init_wifi(const char *ssid, const char *ssid_pass,
                      const char *hostname) {
  wifi_event_group = xEventGroupCreate();

  ESP_LOGI(TAG, "Connecting wifi at %s", ssid);
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  esp_netif_t *netif = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_netif_set_hostname(netif, hostname));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {};
  strcpy((char *)wifi_config.sta.ssid, ssid);
  strcpy((char *)wifi_config.sta.password, ssid_pass);

  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

char *get_key_string_value(nvs_handle_t nvs_handler, const char *key) {
  size_t required_size = 0;

  esp_err_t err = nvs_get_str(nvs_handler, key, NULL, &required_size);

  if (err == ESP_OK && required_size > 1) {
    char *out_value = malloc(required_size);
    err = nvs_get_str(nvs_handler, key, out_value, &required_size);
    if (err == ESP_OK) {
      return out_value;
    } else {
      free(out_value);
    }
  }

  return NULL;
}

void app_main() {
  init_system();

  nvs_handle_t creds_handle;

  char *partition_name = "credentials";

  esp_err_t ret = nvs_flash_init_partition(partition_name);

  if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
      (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    ESP_ERROR_CHECK(nvs_flash_erase_partition(partition_name));
    ESP_ERROR_CHECK(nvs_flash_init_partition(partition_name));
  }

  ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "secrets",
                                          NVS_READONLY, &creds_handle));
  ESP_LOGI(TAG, "Partition correctly loaded!");

  uint16_t mqtt_port = 8883;
  char *ssid = get_key_string_value(creds_handle, "ssid");
  char *ssid_pass = get_key_string_value(creds_handle, "ssid_pass");
  char *serial_number = get_key_string_value(creds_handle, "serial_number");
  char *key_content = get_key_string_value(creds_handle, "key");
  char *cert_content = get_key_string_value(creds_handle, "certificate_pem");
  char *root_ca = get_key_string_value(creds_handle, "root_ca");
  char *thing_name = get_key_string_value(creds_handle, "thing_name");
  char *mqtt_host_url = get_key_string_value(creds_handle, "mqtt_url");
  nvs_get_u16(creds_handle, "mqtt_port", &mqtt_port);

  init_wifi(ssid, ssid_pass, serial_number);
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
                      portMAX_DELAY);

  if (enable_upd_logging) {
    ESP_ERROR_CHECK(udp_logging_init(udp_logging_address, udp_logging_port,
                                     udp_logging_vprintf));
  }

  task_results *results = (task_results *)pvPortMalloc(sizeof(task_results));
  mqtt_params p = {
      .results = results,
      .cert = cert_content,
      .key = key_content,
      .thing_name = thing_name,
      .mqtt_host = mqtt_host_url,
      .mqtt_port = mqtt_port,
      .root_ca = root_ca,
  };

  tasks_event_group = xEventGroupCreate();
  results->tasks_event = tasks_event_group;

  ESP_LOGI(TAG, "Starting tasks...");
  xTaskCreate(&power_task, "power_task", 2000, (void *)results, 4, NULL);
  xTaskCreate(&co2_task, "co2_task", 2000, (void *)results, 4, NULL);
  xTaskCreate(&dht_task, "dht_task", 2000, (void *)results, 4, NULL);
  xTaskCreate(&ldr_task, "ldr_task", 2000, (void *)results, 4, NULL);
  xTaskCreate(&gas_task, "gas_task", 2000, (void *)results, 4, NULL);
  xTaskCreate(&mqtt_task, "mqtt_task", 5000, (void *)&p, 4, NULL);
  ESP_LOGI(TAG, "Waiting for tasks to finish");

  xEventGroupWaitBits(tasks_event_group, MQTT_TASK_BIT, pdTRUE, pdFALSE,
                      portMAX_DELAY);

  ESP_LOGI(TAG, "All tasks are finished, sleeping for %ds",
           SLEE_TIME / US_TO_MS);
  vPortFree(results);

  esp_sleep_enable_timer_wakeup(SLEE_TIME);
  esp_deep_sleep_start();
}