#include "usb.h"
#include "app_state.h"
#include "nvic_defs.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "usbd_desc.h"

USBD_StatusTypeDef USB_Init() {
  USBD_StatusTypeDef usb_err;
  if ((usb_err = USBD_Init(&gAppState.husbd, &FS_Desc, 0)) != USBD_OK) {
    return usb_err;
  }

  // Enable interrupts only after uusbd is initialized to make sure pData is not
  // NULL
  HAL_NVIC_SetPriority(OTG_FS_IRQn, NVIC_PRIORITY_OTG_FS, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  if ((usb_err = USBD_RegisterClass(&gAppState.husbd, &USBD_CDC)) != USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_CDC_RegisterInterface(
           &gAppState.husbd, &USBD_Interface_fops_FS)) != USBD_OK) {
    return usb_err;
  }

  if ((usb_err = USBD_Start(&gAppState.husbd)) != USBD_OK) {
    return usb_err;
  }

  return usb_err;
}