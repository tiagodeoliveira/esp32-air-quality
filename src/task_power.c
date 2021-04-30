#include "tasks.h"
#include <math.h>

#include "esp_adc_cal.h"

#define SAMPLES 500
#define REDUCTION_FACTOR 0.047

#define DEFAULT_VREF 1100
#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTENUATION ADC_ATTEN_DB_0
#define ADC_BIT_SIZE ADC_WIDTH_12Bit

void power_task(void *param) {
  task_results *results = (task_results *)param;

  adc1_config_width(ADC_BIT_SIZE);
  adc1_config_channel_atten(POWER_PIN, ADC_ATTENUATION);

  esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT, ADC_ATTENUATION, ADC_BIT_SIZE, DEFAULT_VREF, adc_chars);

  int power_sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    power_sum += adc1_get_raw(POWER_PIN);
  }

  int voltage = (power_sum / SAMPLES);
  voltage = esp_adc_cal_raw_to_voltage(voltage, adc_chars);
  voltage = voltage / REDUCTION_FACTOR;
  voltage = roundf(voltage * 100) / 100;

  results->power.volts = voltage;

  xEventGroupSetBits(results->tasks_event, POWER_TASK_BIT);
  vTaskDelete(NULL);
}