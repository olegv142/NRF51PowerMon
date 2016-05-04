#include "stat.h"
#include "uart.h"
#include "rtc.h"
#include "bug.h"
#include "channels.h"

// Statistic collected for each transmitter
struct stat_entry {
    // Epoch incremented every time the reporting module restarted, 0 means it was not started at all
    unsigned epoch; 
    // Reports stat for current epoch
    unsigned first_report_ts;
    unsigned last_report_ts;
    unsigned reports_total;
    unsigned reports_received;
    // Last report received
    struct report_packet last_report;
};

struct stat_buffer {
    struct stat_entry e[2];
    unsigned          ei;
};

static struct stat_buffer g_stat_buff[MAX_GR_ROLES];
static unsigned g_stat_group;

void stat_init(unsigned group)
{
    g_stat_group = group;
}

// Update stat on new report arrival
void stat_update(struct report_packet const* r)
{
    unsigned ts = rtc_current();
    BUG_ON(r->role >= MAX_GR_ROLES);
    struct stat_buffer* b = &g_stat_buff[r->role];
    unsigned next_i = 1 - b->ei;
    struct stat_entry *last_s = &b->e[b->ei], *s = &b->e[next_i];
    int new_epoch = !last_s->epoch || last_s->last_report.sn >= r->sn;
    if (new_epoch) {
        s->epoch = last_s->epoch + 1;
        s->first_report_ts = s->last_report_ts   = ts;
        s->reports_total   = s->reports_received = 1;
    } else {
        s->epoch = last_s->epoch;
        s->first_report_ts  = last_s->first_report_ts;
        s->last_report_ts   = ts;
        s->reports_total    = last_s->reports_total + (r->sn - last_s->last_report.sn);
        s->reports_received = last_s->reports_received + 1;
    }
    s->last_report = *r;
    b->ei = next_i;
}

// Print stat to UART
void stat_dump(void)
{
    unsigned i, ts = rtc_current();
    uart_printf("%u %u" UART_EOL, g_stat_group, ts);
    for (i = 0; i < MAX_GR_ROLES; ++i)
    {
        struct stat_buffer const* b = &g_stat_buff[i];
        struct stat_entry const* s = &b->e[b->ei];
        uart_printf("%u %u %u %u %u ", 
            s->epoch,
            s->reports_total,
            s->reports_received,
            s->first_report_ts,
            s->last_report_ts
        );
        struct report_packet const* r = &s->last_report;
        uart_printf("%u %u %u %u %u %u" UART_EOL, 
            r->sn,
            r->ts,
            r->bt_pressed,
            r->bt_pressed_ts,
            r->bt_released_ts,
            r->vcc_mv
        );
    }
    uart_tx_flush();
}

void stat_help(void)
{
    uart_printf(
        "Stat command (s) output consists of heading line followed by %u channel stat lines"                   UART_EOL
        "Heading line: group current_timestamp"                                                                UART_EOL
        "  group                   - the number identifying radio frequency being used for communications:"    UART_EOL
        "                            0 - 24%02uMHz, 1 - 24%02uMHz"                                             UART_EOL
        "  current_timestamp       - the current receiver timestamp"                                           UART_EOL
                                                                                                               UART_EOL
        "Stat line: epoch rep_total rep_received first_ts last_ts sn ts bt_pressed pressed_ts released_ts vcc" UART_EOL
        "  epoch                   - the number incremented every time the transmitting module restart"        UART_EOL
        "                            is detected, zero epoch means the corresponding module was not ever run"  UART_EOL
        "  rep_total rep_received  - the number of report messages in current epoch"                           UART_EOL
        "  first_ts last_ts        - timestamps of first and last report received in current epoch"            UART_EOL
        "  sn                      - the last report sequence number"                                          UART_EOL
        "  ts                      - the last report timestamp as reported by sender"                          UART_EOL
        "  bt_pressed              - the button status: 0 - released, 1 - pressed"                             UART_EOL
        "  pressed_ts released_ts  - the timestamps of the button first press / last release"                  UART_EOL
        "  vcc                     - the transmitter power voltage measured in millivolts"                     UART_EOL
                                                                                                               UART_EOL
        "All timestamps are measured in 1/1024 sec units."                                                     UART_EOL
        "The ts, pressed_ts, released_ts are measured by the transmitter clock."                               UART_EOL,
        MAX_GR_ROLES, GR0_CH, GR1_CH
    );
    uart_tx_flush();
}
