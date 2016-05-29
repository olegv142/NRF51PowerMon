#pragma once

#include "proto.h"

struct data_log_param {
    void const*             buff;
    uint8_t*                pmap;
    unsigned short          pfirst;
    unsigned short          npages;
    uint8_t                 domain;
};

struct data_log {
    struct data_log_param const* param;
    struct data_page const*      first_pg;
    struct data_page const*      last_pg;
    unsigned short               next_item;
    unsigned short               suspended;
};


void data_log_initialize(
        struct data_log* dl,
        struct data_log_param const* param
    );

void data_log_put_item(struct data_log* dl, uint32_t item, uint32_t sn);

static inline void data_log_suspend(struct data_log* dl)
{
    dl->suspended = 1;
}

