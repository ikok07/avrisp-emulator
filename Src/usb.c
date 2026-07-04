#include "usb.h"
#include "app_state.h"
#include "error.h"
#include "nvic_defs.h"
#include "ringbuf.h"
#include "stk500v2.h"
#include "stm32f4xx_hal.h"
#include "usbd_cdc.h"
#include "usbd_cdc_cb.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include <stdint.h>

USBD_StatusTypeDef USB_Init() {
  USBD_StatusTypeDef usb_err;
  if ((usb_err = USBD_Init(&gAppState.husb.husbd, &FS_Desc, 0)) != USBD_OK) {
    return usb_err;
  }

  // Enable interrupts only after husbd is initialized to make sure pData is not
  // NULL
  HAL_NVIC_SetPriority(OTG_FS_IRQn, NVIC_PRIORITY_OTG_FS, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  if ((usb_err = USBD_RegisterClass(&gAppState.husb.husbd, &USBD_CDC)) !=
      USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_CDC_RegisterInterface(
           &gAppState.husb.husbd, &USBD_Interface_fops_FS)) != USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_Start(&gAppState.husb.husbd)) != USBD_OK) {
    return usb_err;
  }

  // Initialize ring buffer
  gAppState.husb.RxBuffer = ringbuf_new(USB_RX_RING_BUFFER_SIZE);
  if (gAppState.husb.RxBuffer == 0) {
    ERROR_Fatal();
    return USBD_FAIL;
  }

  return usb_err;
}

USBD_StatusTypeDef USB_SendData(uint8_t *Buf, uint16_t Len) {
  return CDC_Transmit_FS(Buf, Len);
}

void USB_MessageHandler() {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;
  STK500V2_CommandTypeDef cmd;
  while (1) {
    HAL_Delay(100);
    // Check whether there is USB connection
    if (gAppState.husb.husbd.dev_state != USBD_STATE_CONFIGURED)
      continue;

    status = STK500V2_ParseCmd(gAppState.husb.RxBuffer, &cmd);
    if (status == USB_COMMAND_OK) {
      status = STK500V2_HandleCmd(&cmd);
      if (status != USB_COMMAND_OK) {
        ERROR_Trigger(1000);
      }
    }
  }
}

// void CDC_CB_Connected() {
//   uint8_t test = 1;
//   (void)test;
// }
// void CDC_CB_Disconnected() {
//   uint8_t test = 1;
//   (void)test;
// }

void CDC_CB_DataReceived(uint8_t *Buf, uint32_t *Len) {
  if (*Len == 0)
    return;

  ringbuf_memcpy_into(gAppState.husb.RxBuffer, Buf, *Len);
}