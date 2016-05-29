#pragma once

#include "data_log.h"

typedef union data_hist_item {
    uint16_t    data[2];
    uint32_t    item;
} data_hist_item_t;

struct data_history_param {
    struct data_log_param  storage;
    unsigned               item_samples;
};

struct data_history {
    struct data_log           storage;
    struct data_history_param param;
    data_hist_item_t          item_buff;
    uint32_t                  item_sn;
    int                       data_idx;
    uint32_t                  samples_sum;
    unsigned                  samples_cnt;
};

void data_hist_initialize(struct data_history* h, struct data_history_param const* param);
void data_hist_put_sample(struct data_history* h, uint16_t sample, uint32_t sn);
