/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef USB_INCLUDE_USBD_CDC_IF
#define USB_INCLUDE_USBD_CDC_IF

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

/* Define size for the receive and transmit buffer over CDC */
#define APP_RX_DATA_SIZE  1024
#define APP_TX_DATA_SIZE  1024

/** CDC Interface callback. */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

__weak void CDC_DataReceived_CB(uint8_t* Buf, uint32_t *Len);

#ifdef __cplusplus
}
#endif

#endif /* USB_INCLUDE_USBD_CDC_IF */

