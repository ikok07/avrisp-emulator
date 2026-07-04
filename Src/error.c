#include "error.h"
#include "gpio_defs.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include <stdint.h>

void ERROR_Init() {
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_OUTPUT_PP,
      .Pull = GPIO_PULLDOWN,
      .Speed = GPIO_SPEED_FREQ_LOW,
      .Pin = GPIO_PIN_ERR_LED,
  };

  HAL_GPIO_Init(GPIO_PORT_ERR_LED, &gpio_conf);
}

void ERROR_Trigger(uint32_t DurationMs) {
  HAL_GPIO_WritePin(GPIO_PORT_ERR_LED, GPIO_PIN_ERR_LED, GPIO_PIN_SET);
  HAL_Delay(DurationMs);
  HAL_GPIO_WritePin(GPIO_PORT_ERR_LED, GPIO_PIN_ERR_LED, GPIO_PIN_RESET);
}

void ERROR_Fatal() {
  __disable_irq();
  HAL_GPIO_WritePin(GPIO_PORT_ERR_LED, GPIO_PIN_ERR_LED, GPIO_PIN_SET);
  while (1)
    ;
}