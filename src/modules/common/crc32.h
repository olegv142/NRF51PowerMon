#pragma once

#include <stdint.h>

uint32_t crc32up(uint32_t crc, uint8_t const *s, unsigned len);

static inline uint32_t crc32(uint8_t const *s, unsigned len)
{
	return crc32up(0, s, len);
}
