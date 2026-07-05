#include "usb.h"
#include "app_state.h"
#include "error.h"
#include "nvic_defs.h"
#include "ringbuf.h"
#include "stk500v2.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_cortex.h"
#include "usbd_cdc.h"
#include "usbd_cdc_cb.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include <stdint.h>

#define COMMAND_HANDLER_COUNT 1

static USB_CommandHandlerTypeDef gCommandHandlers[COMMAND_HANDLER_COUNT];

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

  // Add command handlers
  gCommandHandlers[0] = STK500V2_GetHandler();

  return usb_err;
}

USBD_StatusTypeDef USB_SendData(uint8_t *Buf, uint16_t Len) {
  return CDC_Transmit_FS(Buf, Len);
}

void USB_MessageHandler() {
  USB_CommandStatusTypeDef status = USB_COMMAND_OK;
  STK500V2_CommandTypeDef cmd;
  while (1) {
    HAL_Delay(10);

    // Check whether there is USB connection
    if (gAppState.husb.husbd.dev_state != USBD_STATE_CONFIGURED)
      continue;

    for (uint8_t i = 0; i < COMMAND_HANDLER_COUNT; i++) {
      status = gCommandHandlers[i].ParseCmd(gAppState.husb.RxBuffer, &cmd);
      if (status == USB_COMMAND_OK) {
        uint8_t resp[STK500V2_MAX_COMMAND_SIZE];
        uint8_t resp_len = 0;
        gCommandHandlers[i].HandleCmd(&cmd, resp, &resp_len);

        USBD_StatusTypeDef usbd_err;
        if ((usbd_err = USB_SendData(resp, resp_len)) != USBD_OK) {
          ERROR_Trigger(1000);
        }

        break;
      }
    }
  }
}

// void CDC_CB_Connected() {}
// void CDC_CB_Disconnected() {}

void CDC_CB_DataReceived(uint8_t *Buf, uint32_t *Len) {
  if (*Len == 0)
    return;

  ringbuf_memcpy_into(gAppState.husb.RxBuffer, Buf, *Len);
}