#include "power.h"
#include "stm32f4xx_hal.h"

int main() {
  HAL_StatusTypeDef hal_err;
  if ((hal_err = HAL_Init()) != HAL_OK) {
    while (1)
      ;
  }

  if ((hal_err = POWER_Init()) != HAL_OK) {
    while (1)
      ;
  }

  return 0;
}