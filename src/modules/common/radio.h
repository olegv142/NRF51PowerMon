#pragma once

#include "nrf51.h"

// Setup radio given the packet buffer, packet size and frequency channel
// Actual frequency will be 2400+ch Mhz
void radio_configure(void* packet, unsigned sz, unsigned ch);

// Send packet
void send_packet(void);

typedef void (*receiver_cb_t)(void);

// Turn on receiver
void receiver_on(receiver_cb_t cb);

// Start receiving
void receive_start(void);

static inline int receive_done(void)
{
    return NRF_RADIO->EVENTS_END != 0;
}

static inline int receive_crc_ok(void)
{
    return NRF_RADIO->CRCSTATUS == 1U;
}
