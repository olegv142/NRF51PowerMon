#pragma once

#include "nrf_drv_rtc.h"

extern const nrf_drv_rtc_t g_rtc;

void rtc_initialize(nrf_drv_rtc_handler_t handler);

void rtc_dummy_handler(nrf_drv_rtc_int_type_t int_type);

unsigned rtc_current(void);
