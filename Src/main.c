#include "app_state.h"
#include "power.h"
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

  while (1) {
    HAL_Delay(3000);
    char *str = "Hello World!";
    if ((usb_err = CDC_Transmit_FS((uint8_t *)str, strlen(str))) != USBD_OK) {
      (void)usb_err;
    };
  }
}

void CDC_DataReceived_CB(uint8_t *Buf, uint32_t *Len) {
  uint8_t test = 1;
  (void)test;
}