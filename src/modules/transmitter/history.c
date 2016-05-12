#include "history.h"

void data_hist_initialize(struct data_history* h, void const* buff, unsigned npages, unsigned item_samples)
{
    data_log_initialize(&h->storage, buff, npages);
    h->item_samples = item_samples;
    h->data_idx = -1;
    h->samples_sum = 0;
    h->samples_cnt = 0;
}

void data_hist_put_sample(struct data_history* h, uint16_t sample, uint32_t sn)
{
    if (h->data_idx < 0)
    {
        h->data_idx = 0;
        h->item_sn = sn;
    }
    h->samples_sum += sample;
    if (++h->samples_cnt >= h->item_samples)
    {
        h->item_buff.data[h->data_idx] = h->samples_sum / h->samples_cnt;
        h->samples_sum = 0;
        h->samples_cnt = 0;
        if (++h->data_idx >= 2)
        {
            data_log_put_item(&h->storage, h->item_buff.item, h->item_sn);
            h->data_idx = -1;
        }
    }
}
