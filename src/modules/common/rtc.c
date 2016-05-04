#include "rtc.h"
#include "clock.h"
#include "app_error.h"

#define RTC_OVERFLOW (RTC_COUNTER_COUNTER_Msk+1)
#define RTC_OFFSET(v) (v & ~RTC_COUNTER_COUNTER_Msk)

// The instance of nrf_drv_rtc for RTC0.
const nrf_drv_rtc_t g_rtc = NRF_DRV_RTC_INSTANCE(0);

static unsigned volatile g_rtc_last;

void rtc_initialize(nrf_drv_rtc_handler_t handler)
{
    uint32_t err_code;

    //Initialize RTC instance
    err_code = nrf_drv_rtc_init(&g_rtc, NULL, handler);
    APP_ERROR_CHECK(err_code);

    //Start 32768Hz crystal clock
    lf_osc_start();

    //Power on RTC instance
    nrf_drv_rtc_enable(&g_rtc);
}

unsigned rtc_current(void)
{
    unsigned last = g_rtc_last;
    unsigned ticks = nrf_drv_rtc_counter_get(&g_rtc);
    unsigned offset = RTC_OFFSET(last);
    unsigned last_ticks = RTC_WRAP(last);
    /*
     * The NFR tick counter is only 3 bytes wide. So we are going to take overflow into account
     * The code must work correctly in case its executed concurrently in normal and IRQ context.
     */
    if (ticks < RTC_OVERFLOW/4 && last_ticks > 3*RTC_OVERFLOW/4) {
        offset += RTC_OVERFLOW;
    }
    return (g_rtc_last = ticks + offset);
}

void rtc_dummy_handler(nrf_drv_rtc_int_type_t int_type)
{
}

