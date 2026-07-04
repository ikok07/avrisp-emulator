#include "adc.h"
#include "error.h"
#include "power.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "usb.h"
#include "usbd_def.h"

int main() {
  HAL_StatusTypeDef hal_err;
  USBD_StatusTypeDef usb_err;
  if ((hal_err = HAL_Init()) != HAL_OK) {
    // Halt indefinetly because the error LED is not initialized yet
    while (1)
      ;
  }

  ERROR_Init();

  if ((hal_err = POWER_Init()) != HAL_OK) {
    ERROR_Fatal();
  }

  if ((usb_err = USB_Init()) != USBD_OK) {
    ERROR_Fatal();
  }

  if ((hal_err = SPI_Init()) != HAL_OK) {
    ERROR_Fatal();
  }

  if ((hal_err = ADC_Init()) != HAL_OK) {
    ERROR_Fatal();
  }

  // Wait and execute usb serial commands
  USB_MessageHandler();
}