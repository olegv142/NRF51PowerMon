#pragma once

#define ROLE_GR_SHIFT 8

// Group select bit
#define ROLE_GR_SELECT (1<<ROLE_GR_SHIFT)

// The max number of role within group
#define MAX_GR_ROLES 8

#define ROLE_MASK (MAX_GR_ROLES-1)

unsigned role_get(void);
