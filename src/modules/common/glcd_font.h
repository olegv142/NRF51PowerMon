#pragma once

#include <stdint.h>

#define MONO_SPACING (-1)

/*
 * The purpose of this module is to provide support for the GLCD lib fonts since they can be
 * created by freely available tools like MicoElectronica GLCD fonts creator.
 */

struct glcd_font {
	uint8_t w;
	uint8_t h;
	uint8_t code_off;
	uint8_t code_num;
	uint8_t const* data;
};

/* Print string starting from the specified position. If spacing < 0 the font will be treated as mono spacing,
 * otherwise the specified spacing will be used for variable spacing print. Note that y is in 8 pixel groups.
 */
void glcd_print_str(unsigned x, unsigned y, const char* str, struct glcd_font const* font, int spacing);

/* Calculate printed text length */
unsigned glcd_printed_len(const char* str, struct glcd_font const* font, int spacing);

/* Print string right aligned */
static inline void glcd_print_str_r(unsigned x, unsigned y, const char* str, struct glcd_font const* font, int spacing)
{
	glcd_print_str(x - glcd_printed_len(str, font, spacing), y, str, font, spacing);
}

