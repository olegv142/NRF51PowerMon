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

//---- data transfer context -----------------

typedef enum {
    x_none,
    x_starting,
    x_reading_meta,
    x_reading_data,
    x_completed,
    x_failed,
} x_status_t;

static x_status_t g_x_status = x_none;
static uint32_t   g_x_start_sn;

typedef enum {
    x_pg_unknown = 0,
    x_pg_unused,
    x_pg_reading_meta,
    x_pg_has_meta,
    x_pg_reading_data,
    x_pg_has_data,
    x_pg_aborted,
} x_pg_status_t;

static x_pg_status_t        g_pg_status [DATA_PAGES];
static struct data_page_hdr g_pg_headers[DATA_PAGES];
static int                  g_pg_buff   [DATA_PAGES];
static unsigned             g_pg_pending;
static unsigned             g_pg_next;

#define BUFF_PAGES 16
#define BUFF_RD_TOUT (10*RTC_HZ) // 10 sec

typedef enum {
    x_buff_unused = 0,
    x_buff_reading,
    x_buff_ready
} x_buff_status_t;

static x_buff_status_t g_buff_status[BUFF_PAGES];
static union data_page_fragmented g_buff[BUFF_PAGES];

static uint8_t g_fragments_required[DATA_PAGES];

static unsigned g_get_pg_tout_ts;

//--------------------------------------------------------

static inline void x_set_status(x_status_t sta)
{
    g_x_status = sta;
}

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

static inline unsigned last_report_age(void)
{
    return (rtc_current() - g_last_report_ts) / RTC_HZ;
}

static void get_transmitter_uptime(void)
{
    if (!g_report_packets) {
        uart_printf(UART_EOL);
    } else {
        uart_printf("%u" UART_EOL, last_report_age() + g_last_report.sn * MEASURING_PERIOD);
    }
    uart_tx_flush();
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

static inline int x_is_active(void)
{
    return  g_x_status != x_none &&
            g_x_status != x_completed &&
            g_x_status != x_failed;
}

static inline void x_upd_tout(void)
{
    g_get_pg_tout_ts = rtc_current() + BUFF_RD_TOUT;
}

static void x_start()
{
    x_upd_tout();
    x_set_status(x_starting);
    uart_printf(UART_EOL);
    uart_tx_flush();
}

static void x_get_status(void)
{
    uart_put(&g_x_status, 1);
    uart_tx_flush_binary();
}

static void x_get_page(void)
{
    x_upd_tout();
    uart_put(&g_x_status, 1);
    if (g_x_status == x_reading_data || g_x_status == x_completed)
    {
        int b;
        for (b = 0; b < BUFF_PAGES; ++b)
        {
            if (g_buff_status[b] == x_buff_ready)
            {
                unsigned pg = g_buff[b].data.h.page_idx;
                BUG_ON(pg >= DATA_PAGES);
                BUG_ON(g_pg_status[pg] != x_pg_has_data);
                g_buff[b].data.h = g_pg_headers[pg];
                uart_put(&g_buff[b], DATA_PAGE_SZ);
                g_buff_status[b] = x_buff_unused;
                break;
            }
        }
    }
    uart_tx_flush_binary();
}

static inline void get_help(void)
{
    uart_printf(" r  - print last report and reception stat" UART_EOL);
    uart_printf(" u  - get transmitter uptime in seconds" UART_EOL);
    uart_printf(" s  - start data transfer" UART_EOL);
    uart_printf(" q  - query data transfer status" UART_EOL);
    uart_printf(" qd - query data transfer status and data page if available" UART_EOL);
    uart_printf(" ?  - this help" UART_EOL);
    uart_tx_flush();
}

void uart_rx_process(void)
{
    switch (g_uart_rx_buff[0]) {
    case 'r':
        get_stat();
        break;
    case 'u':
        get_transmitter_uptime();
        break;
    case 's':
        x_start();
        break;
    case 'q':
        if (g_uart_rx_buff[1] == 'd') {
            x_get_page();
        } else {
            x_get_status();
        }
        break;
    case '?':
        get_help();
        break;
    default:
        uart_printf("invalid command, send ? to get help" UART_EOL);
        uart_tx_flush();
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

static inline void send_data_request(void)
{
    radio_disable_();
    pkt_hdr_init(packet_data_req, sizeof(struct data_req_packet));
    memcpy(g_pkt.data_req.fragment_bitmap, g_fragments_required, DATA_PAGES);
    transmitter_on_();
    radio_transmit_();
    radio_disable_();
    receiver_on_(0);    
}

static inline void require_pg_headers(void)
{
    int i;
    g_pg_pending = 0;
    for (i = 0; i < DATA_PAGES; ++i) {
        if (bmap_get_bit(g_pkt.report.page_bitmap, i)) {
            g_fragments_required[i] = 1;
            g_pg_status[i] = x_pg_reading_meta;
            ++g_pg_pending;
        } else {
            g_fragments_required[i] = 0;
            g_pg_status[i] = x_pg_unused;
        }
    }
}

static inline void x_request_meta(void)
{
    BUG_ON(!g_pg_pending);
    send_data_request();
}

static inline void x_start_read_meta(void)
{
    require_pg_headers();
    if (g_pg_pending) {
        x_set_status(x_reading_meta);
        x_request_meta();
    } else {
        x_set_status(x_failed);
    }
}

static inline void x_pg_bind_buff(int pg, int b)
{
    g_pg_buff[pg] = b;
    g_fragments_required[pg] = ~g_pg_headers[pg].unused_fragments;
    BUG_ON(!g_fragments_required[pg]);
    ++g_pg_pending;
    g_pg_status[pg] = x_pg_reading_data;
    g_buff_status[b] = x_buff_reading;
}

static inline int x_buff_attach(int b)
{
    unsigned pg;
    for (pg = g_pg_next; pg < DATA_PAGES; ++pg) {
        if (g_pg_status[pg] == x_pg_has_meta) {
            x_pg_bind_buff(pg, b);
            g_pg_next = pg + 1;
            return 1;
        }
    }
    g_pg_next = DATA_PAGES;
    return 0;
}

static void x_request_data(void)
{
    if (g_pg_next >= DATA_PAGES) {
        if (!g_pg_pending) {
            x_set_status(x_completed);
            return;
        }
    } else {
        int b, has_free_buff = 0;
        for (b = 0; b < BUFF_PAGES; ++b) {
            if (g_buff_status[b] == x_buff_unused) {
                has_free_buff = 1;
                if (!x_buff_attach(b)) {
                    break;
                }
            }
        }
        if (!has_free_buff) {
            if ((int)(rtc_current() - g_get_pg_tout_ts) > 0) {
                x_set_status(x_failed);
                return;
            }
        }
    }
    send_data_request();
}

static void x_got_report(void)
{
    if (g_x_status == x_starting) {
        if (g_pkt.report.hdr.status & STATUS_LOW_BATT) {
            x_set_status(x_failed);
            return;
        }
        g_x_start_sn = g_pkt.report.sn;
        x_start_read_meta();
        return;
    }
    if (g_pkt.report.sn < g_x_start_sn) {
        x_set_status(x_failed);
        return;
    }
    switch (g_x_status) {
    case x_reading_meta:
        x_request_meta();
        return;
    case x_reading_data:
        x_request_data();
        return;
    }
}

static void x_start_reading_data(void)
{
    g_pg_next = 0;
    memset(g_buff_status, 0, sizeof(g_buff_status));
    x_set_status(x_reading_data);
}

static void x_got_data(void)
{
    unsigned pg = g_pkt.data.pg_hdr.page_idx;
    unsigned f  = g_pkt.data.pg_hdr.fragment_;
    if (pg >= DATA_PAGES || f >= DATA_PG_FRAGMENTS) {
        x_set_status(x_failed);
        return;
    }
    switch (g_x_status) {
    case x_reading_meta:
        if (g_pg_status[pg] == x_pg_reading_meta)
        {
            g_fragments_required[pg] = 0;
            g_pg_headers[pg] = g_pkt.data.pg_hdr;
            g_pg_status[pg] = x_pg_has_meta;
            BUG_ON(!g_pg_pending);
            if (!--g_pg_pending) {
                x_start_reading_data();
            }
        }
        break;
    case x_reading_data:
        if (g_pg_status[pg] == x_pg_reading_data)
        {
            int b = g_pg_buff[pg];
            unsigned fragment_bit = 1 << f;
            BUG_ON(g_buff_status[b] != x_buff_reading);
            if (g_fragments_required[pg] & fragment_bit) {
                if (g_pg_headers[pg].sn != g_pkt.data.pg_hdr.sn) {
                    g_fragments_required[pg] = 0;
                    BUG_ON(!g_pg_pending);
                    --g_pg_pending;
                    g_pg_status[pg]  = x_pg_aborted;
                    g_buff_status[b] = x_buff_unused;
                    return;
                }
                memcpy(&g_buff[b].fragment[f], &g_pkt.data.fragment, DATA_FRAG_SZ);
                g_fragments_required[pg] &= ~fragment_bit;
                if (!g_fragments_required[pg]) {
                    BUG_ON(!g_pg_pending);
                    --g_pg_pending;
                    g_pg_status[pg]  = x_pg_has_data;
                    g_buff_status[b] = x_buff_ready;
                }
            }
        }
        break;
    }
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
            if (x_is_active()) {
                x_got_report();
            }
            break;
        case packet_data:
            if (x_is_active()) {
                x_got_data();
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
