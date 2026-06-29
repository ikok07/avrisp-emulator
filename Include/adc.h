#ifndef INCLUDE_ADC
#define INCLUDE_ADC

#include "stm32f4xx_hal_def.h"
#include <stdint.h>

#define VOLTAGE_DIVIDER_R1                  3300
#define VOLTAGE_DIVIDER_R2                  2000

HAL_StatusTypeDef ADC_Init();
HAL_StatusTypeDef ADC_GetMV(uint32_t *Millivolts);

#endif /* INCLUDE_ADC */
