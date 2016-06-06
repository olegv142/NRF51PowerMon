#pragma once

#define UART_RX_BUFF_SZ 8
#define UART_TX_BUFF_SZ 4096
#define UART_EOL "\r"

extern uint8_t  g_uart_rx_buff[UART_RX_BUFF_SZ];
extern uint8_t* g_uart_tx_buff;
extern unsigned g_uart_rx_len;
extern unsigned g_uart_tx_len;

void uart_init(void);
void uart_rx_process(void);
void uart_tx_flush(void);
void uart_tx_flush_binary(void);

void uart_printf(const char* fmt, ...);
void uart_put(const void* data, unsigned sz);
