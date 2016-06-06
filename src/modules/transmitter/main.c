#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_wdt.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_config.h"
#include "ads1220.h"
#include "history.h"
#include "clock.h"
#include "rtc.h"
#include "bug.h"
#include "bmap.h"
#include "radio.h"

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define TICKS_HZ RTC0_CONFIG_FREQUENCY
#define MS_TICKS(ms) (1+(ms)*TICKS_HZ/1000)

//----- Measuring --------------------------------------------

#define DATA_RDY_PIN 30

// RTC CC channels
#define CC_MEASURING 0
#define CC_RX_TOUT   1

#define MEASURING_TICKS_INTERVAL (MEASURING_PERIOD*TICKS_HZ) // In ticks
#define SAMPLE_COUNT 32  // Samples per main power period
#define SAMPLING_PERIOD_US (20000/SAMPLE_COUNT) // In usec

#define SAMPLE_BATT      (-3)
#define SAMPLE_CONFIGURE (-2)
#define SAMPLE_START     (-1)

static const nrf_drv_timer_t g_timer = NRF_DRV_TIMER_INSTANCE(0);

// Events
static union {
    struct {
        uint8_t measure_req:1;
        uint8_t measure_done:1;
        uint8_t rx_complete:1;
    };
    uint8_t any;
} g_evt;

static nrf_drv_wdt_channel_id g_wdt_channel;

static inline void wait_events(void)
{
    __disable_interrupt();
    if (!g_evt.any)
        __WFI();
    __enable_interrupt();
}

static uint16_t g_vbatt_dmv;
static int32_t  g_samples[SAMPLE_COUNT];
static int      g_sample_idx;
static int      g_samples_collected;
static float    g_amplitude_raw;
static uint16_t g_amplitude;
static uint32_t g_data_sn;

static uint8_t g_batt_cfg[] = {ADS_CFG0_VCC_4, ADS_CFG1_TURBO, 0, 0};

// IN0/IN1, 100uA current to REFP connected to 15k resistor
static uint8_t g_sampling_cfg[] = {1, ADS_CFG1_TURBO, 3, 5 << 5};

// Sine/cosine tables
static double g_sin[SAMPLE_COUNT];
static double g_cos[SAMPLE_COUNT];

//----- Logging --------------------------------------------

// For 1:1000 transformer and 2x15 Om resistors we have full scale of 10kW
// corresponding to the amplitude of 2^23. Use 0.2W units to fit in 16 bit.
#define AMPL_SCALING (50000./(1<<23))

// The multiplier to correct readings
#define AMPL_CALIB 1.10

#pragma data_alignment=DATA_PAGE_SZ
static const struct data_page g_hist_pages[DATA_PAGES];

// Used pages bitmap
static uint8_t g_page_bmap[DATA_PG_BITMAP_SZ];

static struct data_history g_history[dom_count];

static struct data_history_param g_hist_params[dom_count] = {
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = 0,
            .npages = FAST_PW_PAGES,
            .domain = dom_fast_pw
        },
        .item_samples = FAST_PW_PERIOD / MEASURING_PERIOD,
    },
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = FAST_PW_PAGES,
            .npages = SLOW_PW_PAGES,
            .domain = dom_slow_pw
        },
        .item_samples = SLOW_PW_PERIOD / MEASURING_PERIOD,
    },
    {
        .storage = {
            .buff = g_hist_pages,
            .pmap = g_page_bmap,
            .pfirst = FAST_PW_PAGES + SLOW_PW_PAGES,
            .npages = VBATT_PAGES,
            .domain = dom_vbatt
        },
        .item_samples = VBATT_PERIOD / MEASURING_PERIOD,
    }
};

//------ System status & communications -------------------

// Threshold voltages
#define VBATT_CHARGED   41000
#define VBATT_LOW       34000
#define VBATT_SILENT    33000
#define VBATT_HIBERNATE 31000

#define CHARGING_STOP_PIN 0

#define RX_ADDR_TOUT_TICKS      MS_TICKS(1) // 1 msec
#define RX_ADDR_TOUT_TICKS_CONN MS_TICKS(4) // 4 msec
#define RX_TOUT_TICKS           MS_TICKS(9) // 9 msec

#define RX_RETRY_CNT 4

static uint8_t g_batt_status;

static union {
    struct packet_hdr      hdr;
    struct report_packet   report;
    struct data_req_packet data_req;
    struct data_packet     data;
} g_pkt;

static int g_addr_received;
static int g_data_req_received;
static struct data_req_packet g_data_req_packet;

BUILD_BUG_ON(sizeof(g_pkt) > 255);

//------ Communication protocol implementation ------------------------

static inline void pkt_hdr_init(uint8_t type, uint8_t sz)
{
    g_pkt.hdr.sz      = sz - 1;
    g_pkt.hdr.version = PROTOCOL_VERSION;
    g_pkt.hdr.status  = g_batt_status;
    g_pkt.hdr.type    = type;
    g_pkt.hdr.magic   = PROTOCOL_MAGIC;
}

static void send_report(int new_sample)
{
    pkt_hdr_init(packet_report, sizeof(struct report_packet));
    if (new_sample) {
        g_pkt.hdr.status |= STATUS_NEW_SAMPLE;
    }
    g_pkt.report.power = g_amplitude;
    g_pkt.report.vbatt = g_vbatt_dmv;
    g_pkt.report.sn    = g_data_sn;
    memcpy(&g_pkt.report.page_bitmap, g_page_bmap, sizeof(g_page_bmap));
    transmitter_on_();
    radio_transmit_();
    radio_disable_();
}

static void upd_batt_status(uint16_t dmv)
{
    unsigned s = 0;
    g_vbatt_dmv = dmv;
    if (dmv >= VBATT_CHARGED) {
        s |= STATUS_CHARGED;
    }
    if (dmv < VBATT_LOW) {
        s |= STATUS_LOW_BATT;
    }
    if (dmv < VBATT_SILENT) {
        s |= STATUS_SILENT;
    }
    if (dmv < VBATT_HIBERNATE) {
        s |= STATUS_HIBERNATE;
    }
    if (s & STATUS_CHARGED) {
        nrf_gpio_pin_set(CHARGING_STOP_PIN);
    } else {
        nrf_gpio_pin_clear(CHARGING_STOP_PIN);
    }
    g_batt_status = s;
}

static void data_req_cb(void)
{
    if (
        receive_crc_ok() && 
        g_pkt.hdr.version == PROTOCOL_VERSION &&
        g_pkt.hdr.magic   == PROTOCOL_MAGIC   &&
        g_pkt.hdr.type    == packet_data_req  &&
        g_pkt.hdr.sz      == sizeof(struct data_req_packet) - 1
    ) {
        memcpy(&g_data_req_packet, &g_pkt.data_req, sizeof(g_data_req_packet));
        g_data_req_received = 1;
    }
}

static int receive_data_request(unsigned tout)
{
    receiver_on_(data_req_cb);
    receive_start();
    rtc_cc_schedule(CC_RX_TOUT, tout);
    while (!g_evt.rx_complete) {
        wait_events();
    }
    g_evt.rx_complete = 0;
    g_addr_received = 0;
    radio_disable_();
    if (g_data_req_received) {
        g_data_req_received = 0;
        return 1;
    } else {
        return 0;
    }
}

static int send_next_data_(void)
{
    int i, f;
    for (i = 0; i < DATA_PAGES; ++i) {
        if (g_data_req_packet.fragment_bitmap[i]) {
            if (!bmap_get_bit(g_page_bmap, i)) {
                // Page unused - clear corresponding bits in request
                g_data_req_packet.fragment_bitmap[i] = 0;
                continue;
            }
            data_page_fragmented_t const* pg = (data_page_fragmented_t const*)&g_hist_pages[i];
            for (f = 0; f < DATA_PG_FRAGMENTS; ++f) {
                unsigned fragment_bit = 1 << f;
                if (!(g_data_req_packet.fragment_bitmap[i] & fragment_bit)) {
                    continue;
                }
                if (pg->data.h.unused_fragments & fragment_bit) {
                    // No more fragments - clear page bits in request
                    g_data_req_packet.fragment_bitmap[i] = 0;
                    break;
                } else {
                    pkt_hdr_init(packet_data, sizeof(struct data_packet));
                    memcpy(&g_pkt.data.pg_hdr, &pg->data.h, sizeof(pg->data.h));
                    BUG_ON(g_pkt.data.pg_hdr.page_idx != i);
                    g_pkt.data.pg_hdr.fragment_ = f;
                    memcpy(&g_pkt.data.fragment, &pg->fragment[f], DATA_FRAG_SZ);
                    radio_transmit_();
                    // Fragment sent - clear corresponding bit in request
                    g_data_req_packet.fragment_bitmap[i] &= ~fragment_bit;
                    return 1;
                }
            }
        }
    }
    return 0;
}

//------ Data acquisition / processing -----------------------------------

static void init_tables(void)
{
#define PI 3.141592653589793
    int i;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        double ph = PI*i/(SAMPLE_COUNT/2);
        g_sin[i] = sin(ph);
        g_cos[i] = cos(ph);
    }
}

static float detect_amplitude(void)
{
    double sin_sum = 0;
    double cos_sum = 0;
    int i;
    for (i = 0; i < SAMPLE_COUNT; ++i) {
        sin_sum += g_sin[i] * g_samples[i];
        cos_sum += g_cos[i] * g_samples[i];
    }
    sin_sum /= (SAMPLE_COUNT/2);
    cos_sum /= (SAMPLE_COUNT/2);
    return sqrt(sin_sum*sin_sum + cos_sum*cos_sum);
}

static void rtc_handler(nrf_drv_rtc_int_type_t int_type)
{
    switch (int_type) {
    case CC_MEASURING:
        g_evt.measure_req = 1;
        rtc_cc_reschedule(CC_MEASURING, MEASURING_TICKS_INTERVAL);
        break;
    case CC_RX_TOUT:
        if (!g_data_req_received && !g_addr_received && radio_address_ok()) {
            g_addr_received = 1;
            rtc_cc_reschedule(CC_RX_TOUT, RX_TOUT_TICKS);
        } else {
            g_evt.rx_complete = 1;
            rtc_cc_disable(CC_RX_TOUT);
        }
        break;
    default:
        BUG();
    }
}

static inline int is_data_rdy(void)
{
    return !nrf_gpio_pin_read(DATA_RDY_PIN);
}

static inline void sampling_done(void)
{
    BUG_ON(g_evt.measure_done);
    g_evt.measure_done = 1;
    g_sample_idx = SAMPLE_COUNT;
}

static void timer_event_handler(nrf_timer_event_t event_type, void* p_context)
{
    int rdy;
    int32_t res;
    if (event_type != NRF_TIMER_EVENT_COMPARE0)
	    return;
    switch (g_sample_idx) {
    case SAMPLE_BATT:
        ads_configure(g_batt_cfg);
        ads_start(); 
        break;
    case SAMPLE_CONFIGURE:
        BUG_ON(!is_data_rdy());
        upd_batt_status(ads_vcc_dmv(ads_result()));
        if (g_batt_status & STATUS_HIBERNATE) {
            sampling_done();
            return;
        }
        ads_configure(g_sampling_cfg);
        break;
    default:
        rdy = is_data_rdy();
        res = ads_start();
        BUG_ON(g_sample_idx >= SAMPLE_COUNT);
        if (g_sample_idx >= 0) {
            BUG_ON(!rdy);
            g_samples[g_sample_idx] = res;
        }
        break;
    case SAMPLE_COUNT-1:
        BUG_ON(!is_data_rdy());
        res = ads_result();
        g_samples[g_sample_idx] = res;
        g_samples_collected = 1;
        sampling_done();
        return;
    case SAMPLE_COUNT:
        return;
    }
    ++g_sample_idx;
    nrf_timer_cc_write(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0, nrf_timer_cc_read(g_timer.p_reg, NRF_TIMER_CC_CHANNEL0) + SAMPLING_PERIOD_US);
}

static void wdt_event_handler(void)
{
}

static void wdt_initialize(void)
{
    ret_code_t err_code = nrf_drv_wdt_init(0, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&g_wdt_channel);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();
}

static void timer_initialize(void)
{
    ret_code_t err_code = nrf_drv_timer_init(&g_timer, NULL, timer_event_handler);
    APP_ERROR_CHECK(err_code);
}

static void sampling_start(void)
{
    g_sample_idx = SAMPLE_BATT;
    nrf_drv_timer_compare(&g_timer, NRF_TIMER_CC_CHANNEL0, SAMPLING_PERIOD_US, true);
    nrf_drv_timer_enable(&g_timer);
}

static void sampling_stop(void)
{
    BUG_ON(g_sample_idx != SAMPLE_COUNT);
    nrf_drv_timer_disable(&g_timer);
    ads_shutdown();
}

// Avoid using all ones code since it matches to erased flash content
#define MAX_AMPL (((uint16_t)~0)-1)

static uint16_t scale_amplitude(float raw_ampl)
{
    int32_t a = (int32_t)(raw_ampl * AMPL_SCALING * AMPL_CALIB);
    if (a < 0)
        return 0;
    if (a > MAX_AMPL)
        return MAX_AMPL;
    return a;
}

static void process_data(void)
{
    g_amplitude_raw = detect_amplitude();
    g_amplitude = scale_amplitude(g_amplitude_raw);
}

//------ Data logging ----------------------------------------

static void init_history(void)
{
    int d;
    for (d = 0; d < dom_count; ++d) {
        data_hist_initialize(&g_history[d], &g_hist_params[d]);
    }
}

static void history_suspend(void)
{
    int d;
    for (d = 0; d < dom_count; ++d) {
        data_hist_suspend(&g_history[d]);
    }
}

static void history_update(void)
{
    data_hist_put_sample(&g_history[dom_fast_pw], g_amplitude, g_data_sn);
    data_hist_put_sample(&g_history[dom_slow_pw], g_amplitude, g_data_sn);
    data_hist_put_sample(&g_history[dom_vbatt],   g_vbatt_dmv, g_data_sn);
}

static int connection_loop(void)
{
    for (;;)
    {
        int i, data_req;
        transmitter_on_();
        for (;;) {
            if (!send_next_data_() || g_evt.any)
                break;
        }
        radio_disable_();
        if (g_evt.any) {
            return 1;
        }
        for (i = 0; i < RX_RETRY_CNT; ++i)
        {
            send_report(0);
            data_req = receive_data_request(RX_ADDR_TOUT_TICKS_CONN);
            if (data_req) {
                break;
            }
        }
        if (!data_req || g_evt.any) {
            return data_req;
        }
    }
}

// Measure Vbatt once per 5 min in hibernate mode
#define HIBERNATE_MEASURING_PERIOD 300
#define HIBERNATE_SKIP (HIBERNATE_MEASURING_PERIOD/MEASURING_PERIOD)

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    int measuring = 0, connected = 0, hibernate = 0;
    int hibernate_skip = HIBERNATE_SKIP;

    wdt_initialize();
    init_tables();
    init_history();
    timer_initialize();
    ads_initialize();
    nrf_gpio_cfg_input(DATA_RDY_PIN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(CHARGING_STOP_PIN);
    nrf_gpio_pin_clear(CHARGING_STOP_PIN);

    rtc_initialize(rtc_handler);
    rtc_cc_schedule(CC_MEASURING, MEASURING_TICKS_INTERVAL);

    radio_configure(&g_pkt, 0, PROTOCOL_CHANNEL);

    for (;;)
    {
        nrf_drv_wdt_channel_feed(g_wdt_channel);
        if (!connected || measuring) {
            wait_events();
        } else {
            connected = connection_loop();
        }
        if (g_evt.measure_req) {
            g_evt.measure_req = 0;
            ++g_data_sn;
            if (!hibernate || !--hibernate_skip) {
                if (!hf_osc_active()) {
                    hf_osc_start();
                }
                measuring = 1;
                sampling_start();
                hibernate_skip = HIBERNATE_SKIP;
            }
        }
        if (g_evt.measure_done) {
            g_evt.measure_done = 0;
            measuring = 0;
            sampling_stop();
            if (g_samples_collected) {
                g_samples_collected = 0;
                BUG_ON(g_batt_status & STATUS_HIBERNATE);
                hibernate = 0;
                process_data();
                history_update();
                if (connected) {
                    continue;
                }
                if (!(g_batt_status & STATUS_SILENT)) {
                    send_report(1);
                    if (!(g_batt_status & STATUS_LOW_BATT)) {
                        if (receive_data_request(RX_ADDR_TOUT_TICKS)) {
                            connected = 1;
                            continue;
                        }
                    }
                }
            } else {
                BUG_ON(!(g_batt_status & STATUS_HIBERNATE));
                if (!hibernate) {
                    history_suspend();
                    hibernate = 1;
                    connected = 0;
                }
            }
            hf_osc_stop();
        }
    }
}

