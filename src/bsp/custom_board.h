#pragma once
 
#define LED_0         21
#define LED_PWR       LED_0
#define BSP_LED_0     LED_0

#define BUTTON_0      22
#define BUTTON_USER   BUTTON_0
#define BSP_BUTTON_0  BUTTON_0
#define BUTTON_PULL   NRF_GPIO_PIN_PULLUP

#define BUTTONS_NUMBER 1
#define LEDS_NUMBER    1
#define LEDS_LIST    { LED_0 }
#define BUTTONS_LIST { BUTTON_0 }

#define BSP_BUTTON_0_MASK (1<<BUTTON_0)
#define BSP_LED_0_MASK    (1<<LED_0)
#define LEDS_MASK         BSP_LED_0_MASK
#define LEDS_INV_MASK     0

#define RX_PIN_NUMBER  11
#define TX_PIN_NUMBER  9
#define CTS_PIN_NUMBER 10
#define RTS_PIN_NUMBER 8
#define HWFC           true

// Low frequency clock source to be used by the SoftDevice
#define NRF_CLOCK_LFCLKSRC      NRF_CLOCK_LFCLKSRC_XTAL_20_PPM

