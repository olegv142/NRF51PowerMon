#pragma once

#include "nrf_clock.h"
#include "bug.h"

static inline int hf_osc_active(void)
{
    return nrf_clock_hf_src_get() == NRF_CLOCK_HF_SRC_HIGH_ACCURACY;
}

static inline void hf_osc_start(void)
{
    /* Start 16 MHz crystal oscillator */
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1;

    /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // wait
    }
    
    BUG_ON(!hf_osc_active());
}

static inline void hf_osc_stop(void)
{
    /* Stop 16 MHz crystal oscillator */
    NRF_CLOCK->TASKS_HFCLKSTOP = 1;

    while (hf_osc_active())
    {
        // wait
    }
}

static inline void lf_osc_start(void)
{
    /* Start low frequency crystal oscillator */
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // wait
    }
}

