#pragma once

#include "nrf_assert.h"

#define BUG() ASSERT(0)
#define BUG_ON(cond) ASSERT(!(cond))
#define BUILD_BUG_ON(cond) extern void __build_bug_on_dummy(char a[1 - 2*!!(cond)])
