#pragma once

#include "role.h"
#include "packet.h"

void stat_init(unsigned group);

// Update stat on new report arrival
void stat_update(struct report_packet const* r);

// Print stat to UART
void stat_dump(void);

// Print stat  help to UART
void stat_help(void);
