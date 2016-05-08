#pragma once

#include "nrf_drv_rtc.h"
#include "app_error.h"

extern const nrf_drv_rtc_t g_rtc;

void rtc_initialize(nrf_drv_rtc_handler_t handler);

void rtc_dummy_handler(nrf_drv_rtc_int_type_t int_type);

unsigned rtc_current(void);

static inline void rtc_cc_schedule(unsigned chan, unsigned time)
{
    ret_code_t err_code = nrf_drv_rtc_cc_set(&g_rtc, chan, rtc_current() + time, true);
    APP_ERROR_CHECK(err_code);
}

static inline void rtc_cc_reschedule(unsigned chan, unsigned time)
{
    ret_code_t err_code = nrf_drv_rtc_cc_set(&g_rtc, chan, nrf_rtc_cc_get(g_rtc.p_reg, chan) + time, true);
    APP_ERROR_CHECK(err_code);
}

static inline void rtc_cc_disable(unsigned chan)
{
    ret_code_t err_code = nrf_drv_rtc_cc_disable(&g_rtc, chan);
    APP_ERROR_CHECK(err_code);
}
