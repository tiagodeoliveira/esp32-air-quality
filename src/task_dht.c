#include "tasks.h"

#include "DHT.h"

void dht_task(void *param) {
  task_results *results = (task_results *)param;

  setDHTgpio(DHT_PIN);

  int count = 0;
  int temp = 0;
  int humi = 0;
  for (;;) {
    int status = readDHT();
    if (status == DHT_OK) {
      temp += getHumidity();
      humi += getTemperature();
      count++;

      if (count >= READ_SAMPLES) {
        results->dht.humidity = temp / READ_SAMPLES;
        results->dht.temperature = humi / READ_SAMPLES;
        break;
      }
    }

    vTaskDelay(READ_DELAY_IN_MS / portTICK_RATE_MS);
  }

  xEventGroupSetBits(results->tasks_event, DHT_TASK_BIT);
  vTaskDelete(NULL);
}