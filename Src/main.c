#include "adc.h"
#include "power.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "usb.h"
#include "usbd_def.h"

int main() {
  HAL_StatusTypeDef hal_err;
  USBD_StatusTypeDef usb_err;
  if ((hal_err = HAL_Init()) != HAL_OK) {
    while (1)
      ;
  }

  if ((hal_err = POWER_Init()) != HAL_OK) {
    while (1)
      ;
  }

  if ((usb_err = USB_Init()) != USBD_OK) {
    while (1)
      ;
  }

  if ((hal_err = SPI_Init()) != HAL_OK) {
    while (1)
      ;
  }

  if ((hal_err = ADC_Init()) != HAL_OK) {
    while (1)
      ;
  }

  // Wait and execute usb serial commands
  USB_MessageHandler();
}