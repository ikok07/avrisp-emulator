#ifndef USB_INCLUDE_USBD_DESC
#define USB_INCLUDE_USBD_DESC

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_DESC__C__
#define __USBD_DESC__C__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"

/*
 * User to provide a unique ID to define the USB device serial number
 * The use of UID_BASE register can be considered as an example
 */
#define         DEVICE_ID1          (UID_BASE)
#define         DEVICE_ID2          (UID_BASE + 0x4)
#define         DEVICE_ID3          (UID_BASE + 0x8)

#define  USB_SIZ_STRING_SERIAL       0x1A

/** Descriptor for the Usb device. */
extern USBD_DescriptorsTypeDef FS_Desc;

#ifdef __cplusplus
}
#endif

#endif /* __USBD_DESC__C__ */
#endif /* USB_INCLUDE_USBD_DESC */
