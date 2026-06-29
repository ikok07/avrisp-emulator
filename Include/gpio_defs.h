#ifndef INCLUDE_GPIO_DEFS
#define INCLUDE_GPIO_DEFS

#include "stm32f4xx_hal.h"


#define GPIO_PORT_AVR_RESET                 GPIOA
#define GPIO_PIN_AVR_RESET                  GPIO_PIN_4

#define GPIO_PORT_SPI_SCK                   GPIOA
#define GPIO_PIN_SPI_SCK                    GPIO_PIN_5

#define GPIO_PORT_SPI_MISO                  GPIOA
#define GPIO_PIN_SPI_MISO                   GPIO_PIN_6

#define GPIO_PORT_SPI_MOSI                  GPIOA
#define GPIO_PIN_SPI_MOSI                   GPIO_PIN_7

#define GPIO_PORT_USB_VBUS                  GPIOA
#define GPIO_PIN_USB_VBUS                   GPIO_PIN_9

#define GPIO_PORT_USB_ID                    GPIOA
#define GPIO_PIN_USB_ID                     GPIO_PIN_10

#define GPIO_PORT_USB_DM                    GPIOA
#define GPIO_PIN_USB_DM                     GPIO_PIN_11

#define GPIO_PORT_USB_DP                    GPIOA
#define GPIO_PIN_USB_DP                     GPIO_PIN_12

#define GPIO_PORT_ADC_VTARGET               GPIOB
#define GPIO_PIN_ADC_VTARGET                GPIO_PIN_1

#endif /* INCLUDE_GPIO_DEFS */
