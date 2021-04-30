#include "tasks.h"

void gas_task(void *param) {
  task_results *results = (task_results *)param;

  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(GAS_A_PIN, ADC_ATTEN_11db);

  int gas_pin_level = 0;
  for (int i = 0; i < READ_SAMPLES; i++) {
    gas_pin_level += adc1_get_raw(GAS_A_PIN);
  }

  gas_pin_level = gas_pin_level / READ_SAMPLES;

  results->gas.level = gas_pin_level;

  xEventGroupSetBits(results->tasks_event, GAS_TASK_BIT);
  vTaskDelete(NULL);
}