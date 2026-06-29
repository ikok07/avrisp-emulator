#include "gpio_defs.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_rcc.h"

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance == ADC1) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio_conf = (GPIO_InitTypeDef){
        .Mode = GPIO_MODE_ANALOG,
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
        .Pull = GPIO_NOPULL,
        .Pin = GPIO_PIN_ADC_VTARGET,
    };

    HAL_GPIO_Init(GPIO_PORT_ADC_VTARGET, &gpio_conf);
  }
}