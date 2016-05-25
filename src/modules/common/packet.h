#pragma once

#include <stdint.h>

struct report_packet {
    uint16_t power;
    uint16_t vcc;
    uint32_t sn;
};
