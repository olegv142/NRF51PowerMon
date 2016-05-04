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

/** @file
 * @defgroup rtc_example_main main.c
 * @{
 * @ingroup rtc_example
 * @brief Real Time Counter Example Application main file.
 *
 * This file contains the source code for a sample application using the Real Time Counter (RTC).
 * 
 */

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_drv_config.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_ppi.h"
#include "nrf_adc.h"
#include "clock.h"
#include "boards.h"
#include "app_error.h"
#include "bug.h"
#include "app_util_platform.h"

#include <stdint.h>
#include <stdbool.h>

#define CONV_PERIOD 80

static const nrf_drv_timer_t g_timer = NRF_DRV_TIMER_INSTANCE(0);
static nrf_ppi_channel_t g_ppi_channel1;

unsigned g_adc_res[16];
unsigned g_adc_res_i;


/**
 * @brief ADC initialization.
 */
static void adc_initialize(void)
{
    const nrf_adc_config_t nrf_adc_config = { NRF_ADC_CONFIG_RES_10BIT,
        NRF_ADC_CONFIG_SCALING_INPUT_ONE_THIRD,
        NRF_ADC_CONFIG_REF_VBG
    };
    // Initialize and configure ADC
    nrf_adc_configure((nrf_adc_config_t *)&nrf_adc_config);
    nrf_adc_input_select(NRF_ADC_CONFIG_INPUT_2);
}

static void timer_event_handler(nrf_timer_event_t event_type, void* p_context)
{
    unsigned adc_sample;
    if (event_type != NRF_TIMER_EVENT_COMPARE0)
	    return;
    nrf_timer_cc_write(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0, nrf_timer_cc_read(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0) + CONV_PERIOD);
    adc_sample = nrf_adc_result_get();
    if (g_adc_res_i < 16)
        g_adc_res[g_adc_res_i++] = adc_sample;
}

static void timer_initialize(void)
{
    ret_code_t err_code = nrf_drv_timer_init(&g_timer, NULL, timer_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_compare(&g_timer, NRF_TIMER_CC_CHANNEL0, CONV_PERIOD, true);

    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_alloc(&g_ppi_channel1);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_assign(g_ppi_channel1,
                                          nrf_drv_timer_event_address_get(&g_timer, NRF_TIMER_EVENT_COMPARE0),
                                          (uint32_t)nrf_adc_task_address_get(NRF_ADC_TASK_START));
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_enable(g_ppi_channel1);
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_enable(&g_timer);
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    adc_initialize();
    timer_initialize();

    while (true)
    {
        __WFI();
    }
}


/**  @} */
