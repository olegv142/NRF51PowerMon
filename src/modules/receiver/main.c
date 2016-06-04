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
#include "radio.h"
#include "proto.h"
#include "bug.h"
#include "uart.h"
#include "bmap.h"
#include "app_error.h"

#define PW_SCALE .2
#define VCC_SCALE .0001

// Report packet
union {
    struct packet_hdr      hdr;
    struct report_packet   report;
    struct data_req_packet data_req;
    struct data_packet     data;
} g_pkt;

struct report_packet g_last_report;

unsigned g_good_packets;
unsigned g_bad_packets;

int g_stat_request;

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

static inline void help(void)
{
    uart_printf(" s - print current readings and receiption stat" UART_EOL);
    uart_printf(" ? - this help" UART_EOL);
    uart_tx_flush();
}

void uart_rx_process(void)
{
    switch (g_uart_rx_buff[0]) {
    case 's':
        g_stat_request = 1;
        break;
    case '?':
        help();
        break;
    default:
        uart_printf("valid commands are: s, ?" UART_EOL);
        uart_tx_flush();
    }
}

static void stat_dump(void)
{
    if (!g_good_packets) {
        uart_printf("no valid packets received" UART_EOL);
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
        uart_printf("%u packets received (%u%% good)" UART_EOL,
            g_good_packets + g_bad_packets, 100 * g_good_packets / (g_good_packets + g_bad_packets));
    }
    uart_tx_flush();
}

static void on_packet_received(void)
{
    if (receive_crc_ok() && pkt_hdr_valid()) {
        if (g_pkt.hdr.type == packet_report) {
            g_last_report = g_pkt.report;
        }
        ++g_good_packets;
    } else {
        ++g_bad_packets;
    }
    receive_start();
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uart_init();
 
    radio_configure(&g_pkt, 0, PROTOCOL_CHANNEL);

    receiver_on(on_packet_received);
    receive_start();

    while (true)
    {
        __WFI();
        if (g_stat_request) {
            g_stat_request = 0;
            stat_dump();
        }
    }
}

/**  @} */
