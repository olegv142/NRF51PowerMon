#pragma once

void ads_initialize(void);
void ads_configure(uint8_t cfg[4]);
void ads_send_cmd(uint8_t cmd);
int32_t ads_transfer(uint8_t cmd);

static inline int32_t ads_result(void)
{
    return ads_transfer(0);
}

static inline int32_t ads_start(void)
{
    return ads_transfer(8);
}

static inline void ads_shutdown(void)
{
    ads_send_cmd(2);
}

static inline uint32_t ads_vcc_dmv(uint32_t v)
{
    return (81920 * (v >> 9)) >> 14;
}

#define ADS_CFG0_VCC_4 ((13<<4)+1)
#define ADS_CFG1_TURBO ((6<<5)+(2<<3))