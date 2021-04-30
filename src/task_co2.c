#include "driver/uart.h"
#include "tasks.h"
#include <string.h>

#define BUF_SIZE 2048
#define PORT_NUM UART_NUM_1
// #define QUEUE_SIZE 10

#define START_CMD_LEN 9

#define MHZ19_START_BYTE 0xff
#define MHZ19_SENSOR_NUM 0x01
#define MHZ19_READ_CMD 0x86
#define MHZ19_CHECKSUM_READ 0x79

#define MHZ19_START_CMD_LEN 9
#define MHZ19_START_CMD                                                        \
  {                                                                            \
    MHZ19_START_BYTE, MHZ19_SENSOR_NUM, MHZ19_READ_CMD, 0x00, 0x00, 0x00,      \
        0x00, 0x00, MHZ19_CHECKSUM_READ                                        \
  }

static const char *TAG = "CO2";

void co2_task(void *param) {
  task_results *results = (task_results *)param;

  uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  ESP_ERROR_CHECK(uart_param_config(PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(PORT_NUM, CO2_TX_PIN, CO2_RX_PIN,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(
      uart_driver_install(PORT_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0));

  uint8_t *dtmp = (uint8_t *)malloc(BUF_SIZE);

  const uint8_t start[START_CMD_LEN] = MHZ19_START_CMD;

  for (;;) {
    uart_write_bytes(PORT_NUM, (const char *)start, START_CMD_LEN);

    const int rx_bytes =
        uart_read_bytes(PORT_NUM, dtmp, BUF_SIZE, 200 / portTICK_RATE_MS);

    if (rx_bytes > 0 && dtmp[0] == MHZ19_START_BYTE) {
      dtmp[rx_bytes] = 0;

      int co2 = dtmp[2] * 256 + dtmp[3];
      int temperature = dtmp[4] - 40;

      results->co2.ppm = co2;
      results->co2.temperature = temperature;

      break;
    } else {
      ESP_LOGE(TAG, "Wrong response from sensor");
    }
  }

  free(dtmp);
  dtmp = NULL;

  xEventGroupSetBits(results->tasks_event, CO2_TASK_BIT);
  vTaskDelete(NULL);
}
