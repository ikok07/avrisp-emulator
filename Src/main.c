#include "app_state.h"
#include "power.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal.h"
#include "usb.h"
#include "usbd_cdc_if.h"
#include "usbd_def.h"
#include <stdint.h>

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

  // Wait and execute usb serial commands
  USB_MessageHandler();
}