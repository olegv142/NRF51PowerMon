#include "nrf.h"
#include "uart.h"
#include "bug.h"
#include "app_error.h"
#include "nrf_drv_uart.h"
#include "app_util_platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define LEN_PREFIX_LEN 6
#define LEN_PREFIX_FMT "~%04x"
#define TX_CHUNK_SZ 0xff

uint8_t  g_uart_rx_buff[UART_RX_BUFF_SZ];
static uint8_t g_uart_tx_buff_[LEN_PREFIX_LEN+UART_TX_BUFF_SZ];
uint8_t* g_uart_tx_buff = g_uart_tx_buff_ + LEN_PREFIX_LEN;
unsigned g_uart_rx_len;
unsigned g_uart_tx_len;
static int g_uart_tx_len_;

static void uart_tx_next(void)
{
    unsigned remaining = g_uart_tx_len - g_uart_tx_len_;
    if (remaining > TX_CHUNK_SZ)
        remaining = TX_CHUNK_SZ;
    ret_code_t err_code = nrf_drv_uart_tx(g_uart_tx_buff + g_uart_tx_len_, remaining);
    APP_ERROR_CHECK(err_code);
}

void uart_tx_flush_(char sep)
{
    BUG_ON(g_uart_tx_len > UART_TX_BUFF_SZ);
    snprintf((char*)g_uart_tx_buff_, LEN_PREFIX_LEN, LEN_PREFIX_FMT, g_uart_tx_len);
    g_uart_tx_buff_[LEN_PREFIX_LEN-1] = sep;
    g_uart_tx_len_ = -LEN_PREFIX_LEN;
    uart_tx_next();
}

void uart_tx_flush(void)
{
    uart_tx_flush_(*UART_EOL);
}

void uart_tx_flush_binary(void)
{
    uart_tx_flush_('B');
}

static void uart_rx_next(void)
{
    ret_code_t err_code = nrf_drv_uart_rx(&g_uart_rx_buff[g_uart_rx_len], 1);
    APP_ERROR_CHECK(err_code);
}

static void uart_event_handler(nrf_drv_uart_event_t * p_event, void * p_context)
{
    if (p_event->type == NRF_DRV_UART_EVT_RX_DONE)
    {
        if (p_event->data.rxtx.bytes) {
            // byte received
            if (g_uart_rx_len < UART_RX_BUFF_SZ-1) {
                ++g_uart_rx_len;
            }
            BUG_ON(p_event->data.rxtx.bytes != 1);
            if (p_event->data.rxtx.p_data[0] == *UART_EOL) {
                g_uart_rx_buff[g_uart_rx_len] = 0;
                uart_rx_process();
                g_uart_rx_len = 0;
            }
        } // otherwise it was timeout
        uart_rx_next();
    }
    else if (p_event->type == NRF_DRV_UART_EVT_TX_DONE)
    {
        g_uart_tx_len_ += p_event->data.rxtx.bytes;
        if (g_uart_tx_len_ < (int)g_uart_tx_len) {
            uart_tx_next();
        } else {
            g_uart_tx_len = 0;
        }
    }
}

void uart_init(void)
{
    nrf_drv_uart_config_t g_uart_cfg = NRF_DRV_UART_DEFAULT_CONFIG;
    ret_code_t err_code = nrf_drv_uart_init(&g_uart_cfg, uart_event_handler);
    APP_ERROR_CHECK(err_code);
    uart_rx_next();
}

void uart_printf(const char* fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    unsigned avail = UART_TX_BUFF_SZ - g_uart_tx_len;
    int r = vsnprintf((char*)g_uart_tx_buff + g_uart_tx_len, avail, fmt, v);
    BUG_ON((unsigned)r >= avail);
    if (r > 0) {
        if (r < avail) {
            g_uart_tx_len += r;
        } else {
            g_uart_tx_len += avail;
        }
    }
    va_end(v);
}

void uart_put(const void* data, unsigned sz)
{
    BUG_ON(g_uart_tx_len + sz > UART_TX_BUFF_SZ);
    memcpy(g_uart_tx_buff + g_uart_tx_len, data, sz);
    g_uart_tx_len += sz;
}
