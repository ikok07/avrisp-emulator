#ifndef INCLUDE_APP_STATE
#define INCLUDE_APP_STATE

#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_spi.h"
#include "usbd_def.h"
#include "usb.h"

typedef struct {
    USB_HandleTypeDef husb;
    SPI_HandleTypeDef hspi1;
    ADC_HandleTypeDef hadc1;
} APP_State;

extern APP_State gAppState;

#endif /* INCLUDE_APP_STATE */