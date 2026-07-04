#include "app_state.h"
#include "gpio_defs.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_pcd.h"
void SysTick_Handler() { HAL_IncTick(); }
void OTG_FS_IRQHandler() { HAL_PCD_IRQHandler(gAppState.husb.husbd.pData); }
void EXTI15_10_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_DEBUG_ENABLE); }