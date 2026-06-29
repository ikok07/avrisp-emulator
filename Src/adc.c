#include "adc.h"
#include "Legacy/stm32_hal_legacy.h"
#include "app_state.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_def.h"
#include <stdint.h>

HAL_StatusTypeDef ADC_Init() {
  HAL_StatusTypeDef hal_err = HAL_OK;

  gAppState.hadc1 = (ADC_HandleTypeDef){
      .Instance = ADC1,
      .Init =
          {
              .ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2,
              .Resolution = ADC_RESOLUTION12b,
              .DataAlign = ADC_DATAALIGN_RIGHT,
              .ScanConvMode = DISABLE, // Only one conversion will be used
              .EOCSelection = ADC_EOC_SINGLE_CONV, // Signal end of conversion
                                                   // after a single conversion
              .ContinuousConvMode = DISABLE,
              .NbrOfConversion = 1,
              .DiscontinuousConvMode = DISABLE,
              .NbrOfDiscConversion = DISABLE,
              .ExternalTrigConv = ADC_SOFTWARE_START,
          },
  };

  if ((hal_err = HAL_ADC_Init(&gAppState.hadc1)) != HAL_OK) {
    return hal_err;
  }

  ADC_ChannelConfTypeDef chan_config = {.Channel = ADC_CHANNEL_9,
                                        .Rank = 1,
                                        .SamplingTime =
                                            ADC_SAMPLETIME_112CYCLES};

  if ((hal_err = HAL_ADC_ConfigChannel(&gAppState.hadc1, &chan_config)) !=
      HAL_OK) {
    return hal_err;
  }

  return hal_err;
}

HAL_StatusTypeDef ADC_GetMV(uint32_t *Millivolts) {
  HAL_StatusTypeDef hal_err;
  if ((hal_err = HAL_ADC_Start(&gAppState.hadc1)) != HAL_OK) {
    return hal_err;
  }

  if ((hal_err = HAL_ADC_PollForConversion(&gAppState.hadc1, 1000)) != HAL_OK) {
    return hal_err;
  }

  uint32_t adc_value = HAL_ADC_GetValue(&gAppState.hadc1);
  float vout = 3300 * ((float)adc_value / 4096);
  *Millivolts =
      ((vout * (VOLTAGE_DIVIDER_R1 + VOLTAGE_DIVIDER_R2)) / VOLTAGE_DIVIDER_R2);

  return hal_err;
}