#pragma once

#include "nrf_gpio.h"
#include "nrf_delay.h"

#define DISP_RST_PIN 23
#define DISP_DC_PIN  24
#define DISP_CS_PIN  25

#define DISP_W 128
#define DISP_H 64

void displ_init(void);
void displ_clear(void);
void displ_write(uint8_t col, uint8_t pg, uint8_t const* data, unsigned len);

extern uint8_t g_displ_pg_buff[DISP_W];
