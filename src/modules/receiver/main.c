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
#include "app_error.h"

#define PW_SCALE .2
#define VCC_SCALE .0001

// Report packet
struct report_packet g_report;
struct report_packet g_last_report;

unsigned g_good_packets;
unsigned g_bad_packets;

int g_stat_request;

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
        uart_printf("PW  = %.1f" UART_EOL, PW_SCALE * g_last_report.power);
        uart_printf("Vbatt = %.4f" UART_EOL, VCC_SCALE * g_last_report.vbatt);
        uart_printf("%u%% good packets" UART_EOL, 100 * g_good_packets / (g_good_packets + g_bad_packets));
    }
    uart_tx_flush();
}

static void on_packet_received(void)
{
    if (receive_crc_ok()) {
        g_last_report = g_report;
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
 
    radio_configure(&g_report, sizeof(g_report), 0);

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
