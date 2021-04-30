#include "tasks.h"

void ldr_task(void *param) {
  task_results *results = (task_results *)param;

  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(LDR_PIN, ADC_ATTEN_11db);

  int sum = 0;
  for (int i = 0; i < READ_SAMPLES; i++) {
    sum += adc1_get_raw(LDR_PIN);

    vTaskDelay(READ_DELAY_IN_MS / portTICK_PERIOD_MS);
  }

  results->ldr.light = sum / READ_SAMPLES;

  xEventGroupSetBits(results->tasks_event, LDR_TASK_BIT);
  vTaskDelete(NULL);
}