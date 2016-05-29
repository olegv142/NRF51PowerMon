#pragma once

#include <stdint.h>

static inline int bmap_get_bit(uint8_t const* bm, int bit)
{
    return bm[bit/8] & (1 << (bit%8));
}

static inline void bmap_set_bit(uint8_t* bm, int bit)
{
    bm[bit/8] |= (1 << (bit%8));
}

static inline void bmap_clr_bit(uint8_t* bm, int bit)
{
    bm[bit/8] &= ~(1 << (bit%8));
}

