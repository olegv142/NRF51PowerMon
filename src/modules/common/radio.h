#pragma once

#include "nrf51.h"

// Setup radio given the packet buffer, packet size and frequency channel
// Actual frequency will be 2400+ch Mhz
void radio_configure(void* packet, unsigned sz, unsigned ch);

// Configure clock, send packet and return back to original clock
void send_packet(void);

typedef void (*receiver_cb_t)(void);

// Configure clock and turn on receiver
void receiver_on(receiver_cb_t cb);

// Start receiving
void receive_start(void);

static inline int radio_address_ok(void)
{
    return NRF_RADIO->EVENTS_ADDRESS != 0;
}

static inline int radio_tx_end(void)
{
    return NRF_RADIO->EVENTS_END != 0;
}

static inline int receive_crc_ok(void)
{
    return NRF_RADIO->CRCSTATUS == 1U;
}

//
// Low level routines, no automatic clock control
//

// Turn on transmitter
void transmitter_on_(void);

// Transmit packet
void radio_transmit_(void);

// Turn on receiver
void receiver_on_(receiver_cb_t cb);

// Disable radio
void radio_disable_(void);
