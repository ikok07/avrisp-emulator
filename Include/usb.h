#ifndef INCLUDE_USB
#define INCLUDE_USB

#include "ringbuf.h"
#include "usbd_def.h"
#include <stdint.h>

#define USB_RX_RING_BUFFER_SIZE                     2048
typedef enum {
    USB_COMMAND_OK,
    USB_COMMAND_PARSE_ERR,
    USB_COMMAND_RESPONSE_ERR
} USB_CommandStatusTypeDef;
typedef struct {
    ringbuf_t RxBuffer;
    USBD_HandleTypeDef husbd;
} USB_HandleTypeDef;

USBD_StatusTypeDef USB_Init();
USBD_StatusTypeDef USB_SendData(uint8_t *Buf, uint16_t Len);
void USB_MessageHandler();

#endif /* INCLUDE_USB */
