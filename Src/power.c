#include "power.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_rcc.h"

HAL_StatusTypeDef POWER_Init() {
  HAL_StatusTypeDef err = HAL_OK;
  RCC_OscInitTypeDef osc_conf = {.OscillatorType = RCC_OSCILLATORTYPE_HSI |
                                                   RCC_OSCILLATORTYPE_HSE,
                                 .HSIState = RCC_HSI_ON,
                                 .HSEState = RCC_HSE_ON,
                                 .PLL = {.PLLState = RCC_PLL_ON,
                                         .PLLSource = RCC_PLLSOURCE_HSE,
                                         .PLLM = 15,
                                         .PLLN = 144,
                                         .PLLP = 2,
                                         .PLLQ = 5}};

  if ((err = HAL_RCC_OscConfig(&osc_conf)) != HAL_OK) {
    return err;
  }

  RCC_ClkInitTypeDef clk_conf = {.ClockType =
                                     RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2,
                                 .SYSCLKSource = RCC_SYSCLKSOURCE_HSI,
                                 .AHBCLKDivider = RCC_SYSCLK_DIV1,
                                 .APB1CLKDivider = RCC_HCLK_DIV1,
                                 .APB2CLKDivider = RCC_HCLK_DIV1};

  if ((err = HAL_RCC_ClockConfig(&clk_conf, FLASH_LATENCY_0)) != HAL_OK) {
    return err;
  }

  return err;
}