#include "display.h"
#include "nrf_drv_spi.h"
#include "app_error.h"
#include "app_util_platform.h"

/*
 * SH1106 based OLED display driver
 */

#define SPI0_CONFIG_CS_PIN DISP_CS_PIN

/*
 * The SH1106 supports 132x64 resolution while the display has only 128x64 pixels.
 * So the first and last 2 columns are unused.
 */
#define COL_OFFSET 2

static const nrf_drv_spi_t s_displ_spi = NRF_DRV_SPI_INSTANCE(0);

static uint8_t s_displ_init_cmds[] = {
	0xae,       // off
	0xa1,       // segments remap
	0xc8,       // reversed scan
	0xda, 0x12, // common pads alternative mode
	0x81, 0x9f, // contrast
	0xdb, 0x40, // vcom deselect level
	0xaf,       // on
};

static inline void displ_mode_data(void)
{
	nrf_gpio_pin_set(DISP_DC_PIN);
}

static inline void displ_mode_cmd(void)
{
	nrf_gpio_pin_clear(DISP_DC_PIN);
}

static inline void displ_spi_initialize(void)
{
	nrf_drv_spi_config_t config = {
		.sck_pin  = SPI0_CONFIG_SCK_PIN,
		.mosi_pin = SPI0_CONFIG_MOSI_PIN,
		.miso_pin = SPI0_CONFIG_MISO_PIN,
		.ss_pin   = SPI0_CONFIG_CS_PIN,
		.irq_priority = SPI0_CONFIG_IRQ_PRIORITY,
		.frequency = NRF_DRV_SPI_FREQ_1M,
		.mode = NRF_DRV_SPI_MODE_3,
		.bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
	};
	ret_code_t err_code = nrf_drv_spi_init(&s_displ_spi, &config, 0);
	APP_ERROR_CHECK(err_code);
}

static inline void displ_wr_cmds(uint8_t const* cmds, unsigned ncmds)
{
	displ_mode_cmd();
	ret_code_t err_code = nrf_drv_spi_transfer(&s_displ_spi, cmds, ncmds, 0, 0);
	APP_ERROR_CHECK(err_code);
}

static inline void displ_wr_data(uint8_t const* data, unsigned len)
{
	displ_mode_data();
	ret_code_t err_code = nrf_drv_spi_transfer(&s_displ_spi, data, len, 0, 0);
	APP_ERROR_CHECK(err_code);	
}

static inline void displ_wr_start(uint8_t col, uint8_t pg)
{
	col += COL_OFFSET;
	uint8_t cmds[] = {
		col & 0xf, 0x10 | ((col >> 4) & 0xf), 0xb0 | (pg & 0xf)
	};
	displ_wr_cmds(cmds, sizeof(cmds));
}

void displ_init(void)
{
	displ_spi_initialize();
	nrf_gpio_cfg_output(DISP_RST_PIN);
	nrf_gpio_cfg_output(DISP_DC_PIN);
	nrf_gpio_pin_clear(DISP_RST_PIN);
	nrf_delay_us(1000);
	nrf_gpio_pin_set(DISP_RST_PIN);
	nrf_delay_us(100);
	displ_clear();
	displ_wr_cmds(s_displ_init_cmds, sizeof(s_displ_init_cmds));	
}

void displ_write(uint8_t col, uint8_t pg, uint8_t const* data, unsigned len)
{
	displ_wr_start(col, pg);
	displ_wr_data(data, len);
}

void displ_clear(void)
{
	int p; 
	uint8_t pg_buff[DISP_W] = {0};
	for (p = 0; p < DISP_H/8; ++p) {
		displ_write(0, p, pg_buff, sizeof(pg_buff));
	}
}

