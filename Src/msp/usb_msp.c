#include "gpio_defs.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_rcc_ex.h"

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd) {
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef gpio_conf = {.Mode = GPIO_MODE_AF_PP,
                                .Pin = GPIO_PIN_USB_DM,
                                .Alternate = GPIO_AF10_OTG_FS,
                                .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
                                .Pull = GPIO_NOPULL};

  HAL_GPIO_Init(GPIO_PORT_USB_DM, &gpio_conf);

  gpio_conf.Pin = GPIO_PIN_USB_DP;
  HAL_GPIO_Init(GPIO_PORT_USB_DP, &gpio_conf);

  gpio_conf.Pin = GPIO_PIN_USB_ID;
  HAL_GPIO_Init(GPIO_PORT_USB_ID, &gpio_conf);
}