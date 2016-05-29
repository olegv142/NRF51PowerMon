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
#include "nrf_drv_timer.h"
#include "nrf_drv_config.h"
#include "ads1220.h"
#include "history.h"
#include "clock.h"
#include "rtc.h"
#include "bug.h"
#include "math.h"
#include "radio.h"

#include <stdint.h>
#include <stdbool.h>

//----- Measuring --------------------------------------------

#define DATA_RDY_PIN 30

#define MEASURING_TICKS_INTERVAL (MEASURING_PERIOD*1024) // In ticks
#define CC_MEASURING 0   // RTC CC channel
#define SAMPLE_COUNT 32  // Samples per main power period
#define SAMPLING_PERIOD_US (20000/SAMPLE_COUNT) // In usec

#define SAMPLE_VCC       (-3)
#define SAMPLE_CONFIGURE (-2)
#define SAMPLE_START     (-1)

static const nrf_drv_timer_t g_timer = NRF_DRV_TIMER_INSTANCE(0);

static uint8_t  g_measure_req;
static uint8_t  g_data_collected;

uint32_t g_vcc_dmv;
int32_t  g_samples[SAMPLE_COUNT];
int      g_sample_idx;
float    g_amplitude_raw;
uint16_t g_amplitude;
uint32_t g_data_sn;

struct report_packet g_report;

static uint8_t g_vcc_cfg[] = {ADS_CFG0_VCC_4, ADS_CFG1_TURBO, 0, 0};

// IN0/IN1, 100uA current to REFP
static uint8_t g_sampling_cfg[] = {1, ADS_CFG1_TURBO, 3, 5 << 5};

// Sine/cosine tables
static double g_sin[SAMPLE_COUNT];
static double g_cos[SAMPLE_COUNT];

//----- Logging --------------------------------------------

// For 1:1000 transformer and 33 Om resistor we have full scale of 10kW
// corresponding to the amplitude of 2^23. Use 0.2W units to fit in 16 bit.
#define AMPL_SCALING (50000./(1<<23))

#pragma data_alignment=DATA_PAGE_SZ
static const struct data_page g_hist_pages[DATA_PAGES];

// Used pages bitmap
static uint8_t g_page_bmap[DATA_PG_BITMAP_SZ];

static struct data_history g_history[dom_count];

static struct data_history_param g_log_params[dom_count] = {
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = 0,
            .npages = FAST_PW_PAGES,
            .domain = dom_fast_pw
        },
        .item_samples = FAST_PW_PERIOD / MEASURING_PERIOD,
    },
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = FAST_PW_PAGES,
            .npages = SLOW_PW_PAGES,
            .domain = dom_slow_pw
        },
        .item_samples = SLOW_PW_PERIOD / MEASURING_PERIOD,
    },
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = FAST_PW_PAGES + SLOW_PW_PAGES,
            .npages = VCC_PAGES,
            .domain = dom_vcc
        },
        .item_samples = VCC_PERIOD / MEASURING_PERIOD,
    }
};

//---------------------------------------------------------

static void init_tables(void)
{
#define PI 3.141592653589793
    int i;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        double ph = PI*i/(SAMPLE_COUNT/2);
        g_sin[i] = sin(ph);
        g_cos[i] = cos(ph);
    }
}

static float detect_amplitude(void)
{
    double sin_sum = 0;
    double cos_sum = 0;
    int i;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        sin_sum += g_sin[i] * g_samples[i];
        cos_sum += g_cos[i] * g_samples[i];
    }
    sin_sum /= (SAMPLE_COUNT/2);
    cos_sum /= (SAMPLE_COUNT/2);
    return sqrt(sin_sum*sin_sum + cos_sum*cos_sum);
}

static void rtc_handler(nrf_drv_rtc_int_type_t int_type)
{
    BUG_ON(int_type != CC_MEASURING);
    BUG_ON(g_measure_req);
    g_measure_req = 1;
    rtc_cc_reschedule(CC_MEASURING, MEASURING_TICKS_INTERVAL);
}

static inline int is_data_rdy(void)
{
    return !nrf_gpio_pin_read(DATA_RDY_PIN);
}

static void timer_event_handler(nrf_timer_event_t event_type, void* p_context)
{
    int rdy;
    int32_t res;
    if (event_type != NRF_TIMER_EVENT_COMPARE0)
	    return;
    switch (g_sample_idx) {
    case SAMPLE_VCC:
        ads_configure(g_vcc_cfg);
        ads_start(); 
        break;
    case SAMPLE_CONFIGURE:
        BUG_ON(!is_data_rdy());
        g_vcc_dmv = ads_vcc_dmv(ads_result());
        ads_configure(g_sampling_cfg);
        break;
    default:
        rdy = is_data_rdy();
        res = ads_start();
        BUG_ON(g_sample_idx >= SAMPLE_COUNT);
        if (g_sample_idx >= 0) {
            BUG_ON(!rdy);
            g_samples[g_sample_idx] = res;
        }
        break;
    case SAMPLE_COUNT-1:
        BUG_ON(!is_data_rdy());
        res = ads_result();
        g_samples[g_sample_idx] = res;
        BUG_ON(g_data_collected);
        g_data_collected = 1;
        g_sample_idx = SAMPLE_COUNT;
    case SAMPLE_COUNT:
        return;
    }
    ++g_sample_idx;
    nrf_timer_cc_write(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0, nrf_timer_cc_read(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0) + SAMPLING_PERIOD_US);
}

static void timer_initialize(void)
{
    ret_code_t err_code = nrf_drv_timer_init(&g_timer, NULL, timer_event_handler);
    APP_ERROR_CHECK(err_code);
}

static void sampling_start(void)
{
    hf_osc_start();
    g_sample_idx = SAMPLE_VCC;
    nrf_drv_timer_compare(&g_timer, NRF_TIMER_CC_CHANNEL0, SAMPLING_PERIOD_US, true);
    nrf_drv_timer_enable(&g_timer);
}

static void sampling_stop(void)
{
    BUG_ON(g_sample_idx != SAMPLE_COUNT);
    nrf_drv_timer_disable(&g_timer);
    hf_osc_stop();
    ads_shutdown();
}

// Avoid using all ones code since it matches to erased flash content
#define MAX_AMPL (((uint16_t)~0)-1)

static uint16_t scale_amplitude(float raw_ampl)
{
    int32_t a = (int32_t)(raw_ampl * AMPL_SCALING);
    if (a < 0)
        return 0;
    if (a > MAX_AMPL)
        return MAX_AMPL;
    return a;
}

static void process_data(void)
{
    g_amplitude_raw = detect_amplitude();
    g_amplitude = scale_amplitude(g_amplitude_raw);
    ++g_data_sn;
}

static void init_history(void)
{
    int d;
    for (d = 0; d < dom_count; ++d) {
        data_hist_initialize(&g_history[d], &g_log_params[d]);
    }
}

static void history_update(void)
{
    data_hist_put_sample(&g_history[dom_fast_pw], g_amplitude, g_data_sn);
    data_hist_put_sample(&g_history[dom_slow_pw], g_amplitude, g_data_sn);
    data_hist_put_sample(&g_history[dom_vcc],     g_vcc_dmv,   g_data_sn);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    init_tables();
    init_history();
    timer_initialize();
    ads_initialize();
    nrf_gpio_cfg_input(DATA_RDY_PIN, NRF_GPIO_PIN_NOPULL);

    rtc_initialize(rtc_handler);
    rtc_cc_schedule(CC_MEASURING, MEASURING_TICKS_INTERVAL);
   
    radio_configure(&g_report, sizeof(g_report), 0);

    while (true)
    {
        __WFI();
        if (g_measure_req) {
            g_measure_req = 0;
            sampling_start();
        }
        if (g_data_collected) {
            g_data_collected = 0;
            sampling_stop();
            process_data();
            history_update();
            // send results
            g_report.power = g_amplitude;
            g_report.vcc   = g_vcc_dmv;
            g_report.sn    = g_data_sn;
            send_packet();
        }
    }
}

