#include "usb.h"
#include "app_state.h"
#include "error.h"
#include "gpio_defs.h"
#include "nvic_defs.h"
#include "ringbuf.h"
#include "stk500v2.h"
#include "stm32f401xc.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_gpio.h"
#include "usbd_cdc.h"
#include "usbd_cdc_cb.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include <stdint.h>
#include <string.h>

#define COMMAND_HANDLER_COUNT 1

#define DWEN_BIT_POS 6
static uint8_t ENTER_PROG_MODE_PAYLOAD[] = {
    CMD_ENTER_PROGMODE_ISP,
    0xC8,
    0x64,
    0x19,
    0x20,
    0x00,
    0x53,
    0x03,
    0xAC,
    0x53,
    0x00,
    0x00,
};
static uint8_t LEAVE_PROG_MODE_PAYLOAD[] = {
    CMD_LEAVE_PROGMODE_ISP,
    0x0A,
    0x0A,
};
static uint8_t DWEN_FUSE_READ_BYTE_CMDS[] = {0x58, 0x08, 0x00, 0x00};
static uint8_t DWEN_FUSE_WRITE_BYTE_CMDS[] = {0xAC, 0xA8, 0x00};

static USB_CommandHandlerTypeDef gCommandHandlers[COMMAND_HANDLER_COUNT];
static uint8_t gDebugEnabled = 0;

// Query the debug enabled GPIO at start
static uint8_t gDebugToggleRequested = 1;

static uint8_t toggle_dwen(uint8_t Enable);

USBD_StatusTypeDef USB_Init() {
  USBD_StatusTypeDef usb_err;
  if ((usb_err = USBD_Init(&gAppState.husb.husbd, &FS_Desc, 0)) != USBD_OK) {
    return usb_err;
  }

  // Initialize debugWire pins
  GPIO_InitTypeDef gpio_conf = {
      .Mode = GPIO_MODE_IT_RISING_FALLING,
      .Pull = GPIO_PULLDOWN,
      .Speed = GPIO_SPEED_FREQ_LOW,
      .Pin = GPIO_PIN_DEBUG_ENABLE,
  };

  HAL_GPIO_Init(GPIO_PORT_DEBUG_ENABLE, &gpio_conf);

  // Enable interrupts only after husbd is initialized to make sure pData is not
  // NULL
  HAL_NVIC_SetPriority(OTG_FS_IRQn, NVIC_PRIORITY_OTG_FS, 0);
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, NVIC_PRIORITY_DEBUG_ENABLED, 0);

  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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

    // Set debug mode if changed
    if (gDebugToggleRequested) {
      gDebugToggleRequested = 0;
      gDebugEnabled =
          HAL_GPIO_ReadPin(GPIO_PORT_DEBUG_ENABLE, GPIO_PIN_DEBUG_ENABLE);
      toggle_dwen(gDebugEnabled);
    }

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

uint8_t toggle_dwen(uint8_t Enable) {
  if (Enable) {
    uint8_t success = 0;
    // DWEN was disabled, send ISP command to enable it
    STK500V2_CommandTypeDef isp_cmd = {
        .SequenceNumber = 0x00,
        .Token = 0x0E,
    };

    uint8_t resp[STK500V2_MAX_COMMAND_SIZE];
    uint8_t resp_len;

    do {
      // Enter programming mode
      isp_cmd.MessageSize = 12;
      memcpy(isp_cmd.MessageBody, ENTER_PROG_MODE_PAYLOAD,
             sizeof(ENTER_PROG_MODE_PAYLOAD));
      STK500V2_HandleCmd(&isp_cmd, resp, &resp_len);

      // Check if operation is successful
      if (resp_len < STK500V2_MIN_COMMAND_SIZE + 2 ||
          resp[STK500V2_MIN_COMMAND_SIZE + 1] != STATUS_CMD_OK)
        break;

      // Fetch current fuse
      memset(isp_cmd.MessageBody, 0, sizeof(isp_cmd.MessageBody));
      memset(resp, 0, sizeof(resp));
      isp_cmd.SequenceNumber++;
      isp_cmd.MessageSize = sizeof(DWEN_FUSE_READ_BYTE_CMDS) + 2;
      isp_cmd.MessageBody[0] = CMD_READ_FUSE_ISP;
      isp_cmd.MessageBody[1] = 4; // Poll Index
      memcpy(isp_cmd.MessageBody + 2, DWEN_FUSE_READ_BYTE_CMDS,
             sizeof(DWEN_FUSE_READ_BYTE_CMDS));

      STK500V2_HandleCmd(&isp_cmd, resp, &resp_len);

      // Check if operation is successful
      if (resp_len < STK500V2_MIN_COMMAND_SIZE + 3 ||
          resp[STK500V2_MIN_COMMAND_SIZE + 1] != STATUS_CMD_OK)
        break;

      // Enable DWEN by reseting its bit
      uint8_t fuse = resp[STK500V2_MIN_COMMAND_SIZE + 2];
      fuse &= ~(1 << DWEN_BIT_POS);

      // Send modified fuse byte
      memset(isp_cmd.MessageBody, 0, sizeof(isp_cmd.MessageBody));
      memset(resp, 0, sizeof(resp));
      isp_cmd.SequenceNumber++;
      isp_cmd.MessageSize = sizeof(DWEN_FUSE_WRITE_BYTE_CMDS) + 2;
      isp_cmd.MessageBody[0] = CMD_PROGRAM_FUSE_ISP;
      memcpy(isp_cmd.MessageBody + 1, DWEN_FUSE_WRITE_BYTE_CMDS,
             sizeof(DWEN_FUSE_WRITE_BYTE_CMDS));

      isp_cmd.MessageBody[4] = fuse;

      STK500V2_HandleCmd(&isp_cmd, resp, &resp_len);

      // Check if operation is successful
      if (resp_len < STK500V2_MIN_COMMAND_SIZE + 2 ||
          resp[STK500V2_MIN_COMMAND_SIZE + 1] != STATUS_CMD_OK)
        break;

      success = 1;
    } while (0);

    // Exit prog mode
    memset(isp_cmd.MessageBody, 0, sizeof(isp_cmd.MessageBody));
    memset(resp, 0, sizeof(resp));
    isp_cmd.SequenceNumber++;
    isp_cmd.MessageSize = 3;
    memcpy(isp_cmd.MessageBody, LEAVE_PROG_MODE_PAYLOAD,
           sizeof(LEAVE_PROG_MODE_PAYLOAD));

    STK500V2_HandleCmd(&isp_cmd, resp, &resp_len);

    // Check if operation is successful
    if (resp_len < STK500V2_MIN_COMMAND_SIZE + 2 ||
        resp[STK500V2_MIN_COMMAND_SIZE + 1] != STATUS_CMD_OK)
      return 1;

    return success ? 0 : 1;
  } else {
    // DWEN was enabled, send serial command to disable it
    return 0;
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

void HAL_GPIO_EXTI_Callback(uint16_t Pin) {
  if (Pin == GPIO_PIN_DEBUG_ENABLE) {
    gDebugToggleRequested = 1;
  }
}