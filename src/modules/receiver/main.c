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
#include "clock.h"
#include "radio.h"
#include "proto.h"
#include "bug.h"
#include "uart.h"
#include "bmap.h"
#include "rtc.h"
#include "app_error.h"

#include <stdio.h>
#include <string.h>

#define PW_SCALE .2
#define VCC_SCALE .0001

// Packet buffer
static union {
    struct packet_hdr      hdr;
    struct report_packet   report;
    struct data_req_packet data_req;
    struct data_packet     data;
} g_pkt;

static unsigned g_total_packets;
static unsigned g_good_packets;

static unsigned             g_report_packets;
static struct report_packet g_last_report;
static unsigned             g_last_report_ts;

//---- Sync data -----------------------------

typedef enum {
    sync_none,
    sync_in_progress,
    sync_done
} sync_status_t;

static sync_status_t g_sync_status = sync_none;

#pragma data_alignment=DATA_PAGE_SZ
static const struct data_page g_pages[DATA_PAGES];

static uint8_t g_pages_valid[DATA_PG_BITMAP_SZ];
static uint8_t g_fragments_required[DATA_PAGES];
static uint8_t g_fragments_received[DATA_PAGES];

static inline int pkt_hdr_valid(void)
{
    if (g_pkt.hdr.version != PROTOCOL_VERSION)
        return 0;
    if (g_pkt.hdr.magic != PROTOCOL_MAGIC)
        return 0;
    switch (g_pkt.hdr.type) {
	case packet_report:
        if (g_pkt.hdr.sz != sizeof(struct report_packet) - 1)
            return 0;
        break;
	case packet_data_req:
        if (g_pkt.hdr.sz != sizeof(struct data_req_packet) - 1)
            return 0;
        break;
	case packet_data:
        if (g_pkt.hdr.sz != sizeof(struct data_packet) - 1)
            return 0;
        break;
    default:
        return 0;
    }
    return 1;
}

static inline void get_help(void)
{
    uart_printf(" r  - print last report and receiption stat" UART_EOL);
    uart_printf(" q  - query sync status (n - not synced, i - in progress, s - synced)" UART_EOL);
    uart_printf(" s  - start syncing" UART_EOL);
    uart_printf(" pm - get pages bitmap" UART_EOL);
    uart_printf(" pN - get page number N (zero-based)" UART_EOL);    
    uart_printf(" u  - get transmitter uptime in seconds" UART_EOL);
    uart_printf(" ?  - this help" UART_EOL);
    uart_tx_flush();
}

static inline unsigned last_report_age(void)
{
    return (rtc_current() - g_last_report_ts) / RTC_HZ;
}

static void get_tx_uptime(void)
{
    if (!g_report_packets) {
        uart_printf(UART_EOL);
    } else {
        uart_printf("%u" UART_EOL, last_report_age() + g_last_report.sn * MEASURING_PERIOD);
    }
    uart_tx_flush();
}

static void get_sync_status(void)
{
    static char sync_status_symbol[] = {
        [sync_none]        = 'n',
        [sync_in_progress] = 'i',
        [sync_done]        = 's'
    };
    uart_printf("%c" UART_EOL, sync_status_symbol[g_sync_status]);
    uart_tx_flush();
}

static void get_pg_bmap(void)
{
    memcpy(g_uart_tx_buff, g_pages_valid, sizeof(g_pages_valid));
    g_uart_tx_len = sizeof(g_pages_valid);
    uart_tx_flush_binarty();
}

static void get_pg(unsigned pg)
{
    memcpy(g_uart_tx_buff, &g_pages[pg], DATA_PAGE_SZ);
    g_uart_tx_len = DATA_PAGE_SZ;
    uart_tx_flush_binarty();
}

static void get_stat(void)
{
    if (!g_report_packets) {
        uart_printf("no valid reports received" UART_EOL);
    } else {
        int i, pg_cnt = 0;
        for (i = 0; i < DATA_PAGES; ++i) {
               if (bmap_get_bit(g_last_report.page_bitmap, i))
                   ++pg_cnt;
        }
        uart_printf("status = %#x"  UART_EOL, g_last_report.hdr.status);
        uart_printf("PW     = %.1f" UART_EOL, PW_SCALE * g_last_report.power);
        uart_printf("Vbatt  = %.4f" UART_EOL, VCC_SCALE * g_last_report.vbatt);
        uart_printf("SN     = %u"   UART_EOL, g_last_report.sn);
        uart_printf("%u pages used" UART_EOL, pg_cnt);
        uart_printf("%u report packets received" UART_EOL, g_report_packets);
        uart_printf("last packet was received %u sec ago" UART_EOL, last_report_age());        
    }
    if (g_total_packets) {
        uart_printf("total packets received: %u (%u%% good)" UART_EOL,
            g_total_packets, 100 * g_good_packets / g_total_packets);
    }
    uart_tx_flush();
}

static void sync_start()
{
    g_sync_status = sync_in_progress;
    memset(g_fragments_required, 0, sizeof(g_fragments_required));
    memset(g_fragments_received, 0, sizeof(g_fragments_received));
    uart_printf(UART_EOL);
    uart_tx_flush();
}

void uart_rx_process(void)
{
    switch (g_uart_rx_buff[0]) {
    case 'r':
        get_stat();
        break;
    case 'u':
        get_tx_uptime();
        break;
    case 'q':
        get_sync_status();
        break;
    case 's':
        sync_start();
        break;
    case '?':
        get_help();
        break;
    case 'p':
        if (g_uart_rx_buff[1] == 'm') {
            get_pg_bmap();
            break;
        } else {
            unsigned pg;
            int r = sscanf((const char*)&g_uart_rx_buff[1], "%u", &pg);
            if (r == 1) {
                if (pg < DATA_PAGES) {
                    get_pg(pg);
                    break;
                } else {
                    uart_printf("invalid page number" UART_EOL);
                    uart_tx_flush();
                    break;
                }
            }
        }
    default:
        uart_printf("invalid command, send ? to get help" UART_EOL);
        uart_tx_flush();
    }
}

static inline int is_syncing(void)
{
    return g_sync_status == sync_in_progress;
}

static inline void require_pg_headers(void)
{
    int i;
    for (i = 0; i < DATA_PAGES; ++i) {
        if (bmap_get_bit(g_pkt.report.page_bitmap, i)) {
            if (!(g_fragments_received[i] & 1)) {
                g_fragments_required[i] = 1;
            }
        } else {
            g_fragments_required[i] = 0;
            g_fragments_received[i] = 0;
            bmap_clr_bit(g_pages_valid, i);
        }
    }
}

static inline void pkt_hdr_init(uint8_t type, uint8_t sz)
{
    g_pkt.hdr.sz      = sz - 1;
    g_pkt.hdr.version = PROTOCOL_VERSION;
    g_pkt.hdr.status  = 0;
    g_pkt.hdr.type    = type;
    g_pkt.hdr.magic   = PROTOCOL_MAGIC;
}

static void send_data_request(void)
{
    radio_disable_();
    pkt_hdr_init(packet_data_req, sizeof(struct data_req_packet));
    memcpy(g_pkt.data_req.fragment_bitmap, g_fragments_required, DATA_PAGES);
    transmitter_on_();
    radio_transmit_();
    radio_disable_();
    receiver_on_(0);    
}

static void sync_report(void)
{
    require_pg_headers();
    send_data_request();
}

static void sync_data(void)
{
}

static void on_packet_received(void)
{
    ++g_total_packets;
    if (receive_crc_ok() && pkt_hdr_valid())
    {
        ++g_good_packets;
        switch (g_pkt.hdr.type) {
        case packet_report:
            if (g_pkt.hdr.status & STATUS_NEW_SAMPLE) {
                g_last_report    = g_pkt.report;
                g_last_report_ts = rtc_current();
                ++g_report_packets;
            }
            if (is_syncing()) {
                sync_report();
            }
            break;
        case packet_data:
            if (is_syncing()) {
                sync_data();
            }
            break;
        }
    }
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    rtc_initialize(rtc_dummy_handler);
    radio_configure(&g_pkt, 0, PROTOCOL_CHANNEL);
    uart_init();

    hf_osc_start();
    receiver_on_(0);
    receive_start();

    while (true)
    {
        if (radio_tx_end()) {
            on_packet_received();
            receive_start();
        }
    }
}

/**  @} */
