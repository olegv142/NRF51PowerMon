#pragma once

#include <stdint.h>

#define DATA_LOG_PG_SZ 1024
#define DATA_LOG_PG_ITEMS ((DATA_LOG_PG_SZ-4)/4)

struct data_log_pg {
    uint32_t sn;
    union {
        uint32_t items[DATA_LOG_PG_ITEMS];
        uint16_t ishort[2*DATA_LOG_PG_ITEMS];
        uint8_t  ibytes[4*DATA_LOG_PG_ITEMS];
    };
};

struct data_log {
    void const*               buff;
    unsigned                  npages;
    struct data_log_pg const* first_pg;
    struct data_log_pg const* last_pg;
    unsigned                  last_pg_items;
    unsigned                  total_items;
};

static inline struct data_log_pg const* data_log_pg_next(struct data_log const* dl, struct data_log_pg const* pg)
{
    struct data_log_pg const* pg_next = pg + 1, *pg_start = dl->buff;
    if (pg_next >= pg_start + dl->npages) {
        pg_next = pg_start;
    }
    return pg_next;
}

void data_log_initialize(struct data_log* dl, void const* buff, unsigned npages);
void data_log_put_item(struct data_log* dl, uint32_t item, uint32_t sn);

