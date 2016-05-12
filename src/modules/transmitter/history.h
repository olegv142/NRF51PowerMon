#pragma once

#include "data_log.h"

#define HIST_PG_ITEMS (2*DATA_LOG_PG_ITEMS)

typedef union data_hist_item {
    uint16_t    data[2];
    uint32_t    item;
} data_hist_item_t;

struct data_history {
    struct data_log  storage;
    unsigned         item_samples;
    data_hist_item_t item_buff;
    uint32_t         item_sn;
    int              data_idx;
    uint32_t         samples_sum;
    unsigned         samples_cnt;
};

void data_hist_initialize(struct data_history* h, void const* buff, unsigned npages, unsigned item_samples);
void data_hist_put_sample(struct data_history* h, uint16_t sample, uint32_t sn);
