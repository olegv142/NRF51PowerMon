#pragma once

#include <stdint.h>

struct report_packet {
    uint32_t sn;             // sequence number
    uint32_t ts;             // sent timestamp (in 1/1024 sec units, overflows at 2^24)
    uint8_t  role;           // role - LSB bit is set to 1 for finish gate, other bits indicate channel number
    uint8_t  bt_pressed;     // button status - 1 if pressed, 0 otherwise
    uint16_t vcc_mv;         // power voltage in millivolts
    uint32_t bt_pressed_ts;  // button pressed timestamp
    uint32_t bt_released_ts; // button released timestamp
};
