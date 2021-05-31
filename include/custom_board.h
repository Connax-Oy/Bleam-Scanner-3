#if defined (BOARD_IBKS_PLUS)

#ifndef BOARD_IBKS_H
#define BOARD_IBKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for IBKS Plus
#define LEDS_NUMBER    2

#define LED_START      8
#define LED_1          8
#define LED_2          9
#define LED_STOP       9

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_1
#define BSP_LED_3      LED_1

#define BUTTONS_NUMBER 1

#define BUTTON_START   7
#define BUTTON_1       7
#define BUTTON_STOP    7
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_3   BUTTON_1
/*
#define RX_PIN_NUMBER  18
#define TX_PIN_NUMBER  19
#define CTS_PIN_NUMBER 10
#define RTS_PIN_NUMBER 8
#define HWFC           true

#define SPIS_MISO_PIN  20    // SPI MISO signal.
#define SPIS_CSN_PIN   21    // SPI CSN signal.
#define SPIS_MOSI_PIN  22    // SPI MOSI signal.
#define SPIS_SCK_PIN   23    // SPI SCK signal.

#define SPIM0_SCK_PIN       23u     // SPI clock GPIO pin number.
#define SPIM0_MOSI_PIN      20u     // SPI Master Out Slave In GPIO pin number.
#define SPIM0_MISO_PIN      22u     // SPI Master In Slave Out GPIO pin number.
#define SPIM0_SS_PIN        21u     // SPI Slave Select GPIO pin number.

#define SPIM1_SCK_PIN       16u     // SPI clock GPIO pin number.
#define SPIM1_MOSI_PIN      18u     // SPI Master Out Slave In GPIO pin number.
#define SPIM1_MISO_PIN      17u     // SPI Master In Slave Out GPIO pin number.
#define SPIM1_SS_PIN        19u     // SPI Slave Select GPIO pin number.
*/
// Low frequency clock source to be used by the SoftDevice

#define NRF_CLOCK_LFCLKSRC      {.source        = NRF_CLOCK_LF_SRC_XTAL,            \
                                 .rc_ctiv       = 0,                                \
                                 .rc_temp_ctiv  = 0,                                \
                                 .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}

#ifdef __cplusplus
}
#endif
#endif // BOARD_IBKS_H

#endif // BOARD_IBKS_PLUS