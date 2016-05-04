#include "role.h"
#include "utils.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#define FINISH_PIN 23
#define CHAN0_PIN  24
#define CHAN1_PIN  25
#define GR_SELECT_PIN 0

#define ROLE_READ_DELAY 10

struct role_inp {
	unsigned pin;
	unsigned bit;
};

static struct role_inp s_role_map[] = {
	{FINISH_PIN, 1},
	{CHAN0_PIN, 1<<1},
	{CHAN1_PIN, 1<<2},
	{GR_SELECT_PIN, ROLE_GR_SELECT}
};

#define ROLE_PINS ARRAY_SZ(s_role_map)

unsigned role_get(void)
{
	unsigned i, role = 0;
	for (i = 0; i < ROLE_PINS; ++i) {
		nrf_gpio_cfg_input(s_role_map[i].pin, NRF_GPIO_PIN_PULLUP);
	}
	nrf_delay_us(ROLE_READ_DELAY);
	for (i = 0; i < ROLE_PINS; ++i) {
		if (!(NRF_GPIO->IN & (1<<s_role_map[i].pin))) {
			role |= s_role_map[i].bit;
		}
	}
	for (i = 0; i < ROLE_PINS; ++i) {
		nrf_gpio_cfg_default(s_role_map[i].pin);
	}
	return role;
}
