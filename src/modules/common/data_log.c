#include "data_log.h"
#include "ble_flash.h"
#include "bug.h"

BUILD_BUG_ON(sizeof(struct data_log_pg) != DATA_LOG_PG_SZ);

static inline void data_log_pg_init(struct data_log_pg const* pg, uint32_t sn)
{
    ble_flash_page_erase((unsigned)pg / DATA_LOG_PG_SZ);
    BUG_ON(~pg->sn);
    ble_flash_word_write((uint32_t*)&pg->sn, sn);
    BUG_ON(pg->sn != sn);
}

void data_log_put_item(struct data_log* dl, uint32_t item, uint32_t sn)
{
    if (!dl->last_pg_items || dl->last_pg_items >= DATA_LOG_PG_ITEMS)
    {
        if (!dl->last_pg) {
            BUG_ON(dl->last_pg_items);
            dl->first_pg = dl->last_pg = dl->buff;
        } else {
            BUG_ON(dl->last_pg_items != DATA_LOG_PG_ITEMS);
            dl->last_pg = data_log_pg_next(dl, dl->last_pg);
            if (dl->last_pg == dl->first_pg) {
                dl->first_pg = data_log_pg_next(dl, dl->first_pg);
                BUG_ON(dl->total_items < DATA_LOG_PG_ITEMS);
                dl->total_items -= DATA_LOG_PG_ITEMS;
            }
        }
        dl->last_pg_items = 0;
        data_log_pg_init(dl->last_pg, sn);
    }
    ble_flash_word_write((uint32_t*)&dl->last_pg->items[dl->last_pg_items], item);
    ++dl->last_pg_items;
    ++dl->total_items;
}

void data_log_initialize(struct data_log* dl, void const* buff, unsigned npages)
{
    int i;
    unsigned pg = (unsigned)buff / DATA_LOG_PG_SZ;
    for (i = 0; i < npages; ++i, ++pg) {
        ble_flash_page_erase(pg);
    }
    dl->buff = buff;
    dl->npages = npages;
    dl->first_pg = 0;
    dl->last_pg = 0;
    dl->last_pg_items = 0;
    dl->total_items = 0;
}
