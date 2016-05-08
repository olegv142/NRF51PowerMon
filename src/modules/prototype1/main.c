/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */


#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_config.h"
#include "ads1220.h"
#include "bug.h"

#include <stdint.h>
#include <stdbool.h>

#define DATA_RDY_PIN 30

static inline int is_data_rdy(void)
{
    return !nrf_gpio_pin_read(DATA_RDY_PIN);
}

uint32_t g_vcc_dmv;

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    ads_initialize();
    nrf_gpio_cfg_input(DATA_RDY_PIN, NRF_GPIO_PIN_NOPULL);
    nrf_delay_us(1000);
    BUG_ON(is_data_rdy());

    uint8_t vcc_cfg[] = {ADS_CFG0_VCC_4, ADS_CFG1_TURBO, 0, 0};
    ads_configure(vcc_cfg);

    ads_start();

    while (!is_data_rdy()) {}

    g_vcc_dmv = ads_vcc_dmv(ads_result());

    while (true)
    {
        __WFI();
    }
}

