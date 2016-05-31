#include "radio.h"
#include "radio.h"
#include "bug.h"
#include "clock.h"
#include "nrf51_bitfields.h"

#define PACKET_BASE_ADDRESS_LENGTH 4
#define WHITEIV 0

#define ADDR_REFIXES 0x76767676
#define ADDR_BASE    0x6f6c6567

static receiver_cb_t g_receive_cb;

void radio_configure(void* packet, unsigned sz, unsigned ch)
{
    // Radio config
    NRF_RADIO->TXPOWER   = (RADIO_TXPOWER_TXPOWER_Pos4dBm << RADIO_TXPOWER_TXPOWER_Pos);
    NRF_RADIO->MODE      = (RADIO_MODE_MODE_Nrf_250Kbit << RADIO_MODE_MODE_Pos);
    NRF_RADIO->FREQUENCY = ch;

    // Radio address config
    NRF_RADIO->PREFIX0 = ADDR_REFIXES;  
    NRF_RADIO->BASE0   = ADDR_BASE;

    NRF_RADIO->TXADDRESS   = 0x00UL;  // Set device address 0 to use when transmitting
    NRF_RADIO->RXADDRESSES = 0x01UL;  // Enable device address 0 to use to select which addresses to receive

    // Packet configuration
    NRF_RADIO->PCNF0 = sz ? 0 : 8; // Use length prefix if sz = 0
    NRF_RADIO->PCNF1 = (RADIO_PCNF1_WHITEEN_Enabled  << RADIO_PCNF1_WHITEEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Little    << RADIO_PCNF1_ENDIAN_Pos)  |
                       (PACKET_BASE_ADDRESS_LENGTH   << RADIO_PCNF1_BALEN_Pos)   |
                       (sz                           << RADIO_PCNF1_STATLEN_Pos) |
                       ((sz ? sz : 255)              << RADIO_PCNF1_MAXLEN_Pos);

    // CRC Config
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos); // Number of checksum bits
    NRF_RADIO->CRCINIT = 0xFFFFUL;   // Initial value      
    NRF_RADIO->CRCPOLY = 0x11021UL;  // CRC poly: x^16+x^12^x^5+1    

    // Set payload pointer
    NRF_RADIO->PACKETPTR = (uint32_t)packet;
}

void transmitter_on_(void)
{
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->TASKS_TXEN   = 1;

    while (NRF_RADIO->EVENTS_READY == 0)
    {
        // wait
    }
}

void radio_transmit_(void)
{
    NRF_RADIO->DATAWHITEIV = WHITEIV;
    NRF_RADIO->EVENTS_ADDRESS = 0;
    NRF_RADIO->EVENTS_END  = 0;
    NRF_RADIO->TASKS_START = 1;

    while (NRF_RADIO->EVENTS_END == 0)
    {
        // wait
    }
    NRF_RADIO->EVENTS_END  = 0;
}

void radio_disable_(void)
{
    NRF_RADIO->EVENTS_DISABLED = 0;
    // Disable radio
    NRF_RADIO->TASKS_DISABLE = 1;

    while (NRF_RADIO->EVENTS_DISABLED == 0)
    {
        // wait
    }

    NRF_RADIO->INTENCLR = ~0;
    NVIC_DisableIRQ(RADIO_IRQn);
}

void send_packet(void)
{
    int hf_clk_active = hf_osc_active();
    if (!hf_clk_active)
    {
        hf_osc_start();
    }

    transmitter_on_();
    radio_transmit_();
    radio_disable_();

    if (!hf_clk_active)
    {
        hf_osc_stop();
    }
}

void receiver_on_(receiver_cb_t cb)
{
    if (cb)
    {
        g_receive_cb = cb;
        NVIC_EnableIRQ(RADIO_IRQn);
        NRF_RADIO->INTENSET = RADIO_INTENSET_END_Set << RADIO_INTENSET_END_Pos;
    }
    // Enable radio and wait for ready
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->TASKS_RXEN = 1;
    while (NRF_RADIO->EVENTS_READY == 0)
    {
        // wait
    }
}

void receiver_on(receiver_cb_t cb)
{
    int hf_clk_active = hf_osc_active();
    if (!hf_clk_active)
    {
        hf_osc_start();
    }
    receiver_on_(cb);
}

void receive_start(void)
{
    BUG_ON(!hf_osc_active());

    NRF_RADIO->DATAWHITEIV = WHITEIV;
    NRF_RADIO->EVENTS_ADDRESS = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->TASKS_START = 1;
}

void RADIO_IRQHandler(void)
{
    BUG_ON(!g_receive_cb);
    g_receive_cb();
    NRF_RADIO->EVENTS_END = 0;
}
