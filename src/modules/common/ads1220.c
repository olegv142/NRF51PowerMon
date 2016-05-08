#include "nrf.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_config.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "ads1220.h"

#include <string.h>

#define SPI0_CONFIG_CS_PIN 1

static const nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

static inline void spi_initialize(void)
{
    nrf_drv_spi_config_t config = {
        .sck_pin  = SPI0_CONFIG_SCK_PIN,
        .mosi_pin = SPI0_CONFIG_MOSI_PIN,
        .miso_pin = SPI0_CONFIG_MISO_PIN,
        .ss_pin   = SPI0_CONFIG_CS_PIN,
        .irq_priority = SPI0_CONFIG_IRQ_PRIORITY,
        .frequency = NRF_DRV_SPI_FREQ_8M,
        .mode = NRF_DRV_SPI_MODE_1,
        .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
    };
    ret_code_t err_code = nrf_drv_spi_init(&g_spi, &config, 0);
    APP_ERROR_CHECK(err_code);
}

void ads_initialize(void)
{
    spi_initialize();
}

void ads_configure(uint8_t cfg[4])
{
    uint8_t cmd[5] = {0x43};
    memcpy(cmd + 1, cfg, 4);
    ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, cmd, 5, 0, 0);
    APP_ERROR_CHECK(err_code);
}

void ads_send_cmd(uint8_t cmd)
{
    ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &cmd, 1, 0, 0);
    APP_ERROR_CHECK(err_code);
}

int32_t ads_transfer(uint8_t cmd)
{
    uint8_t data[3];
    ret_code_t err_code = nrf_drv_spi_transfer(&g_spi, &cmd, 1, data, sizeof(data));
    APP_ERROR_CHECK(err_code);
    uint32_t ext = (data[0] & 0x80) ? (0xff << 24) : 0;
    return (int32_t)(ext | (data[0] << 16) | (data[1] << 8) | data[2]);
}


