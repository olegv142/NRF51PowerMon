#include "data_log.h"
#include "ble_flash.h"
#include "bmap.h"
#include "bug.h"

#include <stddef.h>

#define OFFSETOF(T, member) ((size_t)((&((T *)0)->member)))

BUILD_BUG_ON(sizeof(struct data_page) != DATA_PAGE_SZ);
BUILD_BUG_ON(sizeof(union data_page_fragmented) != DATA_PAGE_SZ);

static inline unsigned data_log_pg_index(struct data_log const* dl, struct data_page const* pg)
{
    return pg - (struct data_page const*)dl->param->buff;
}

static inline struct data_page const* data_log_pg(struct data_log const* dl, unsigned idx)
{
    return (struct data_page const*)dl->param->buff + idx;
}

static inline struct data_page const* data_log_pg_next(struct data_log const* dl, struct data_page const* pg)
{
    struct data_page const* pg_next = pg + 1;
    struct data_page const* pg_start = data_log_pg(dl, dl->param->pfirst);
    if (pg_next >= pg_start + dl->param->npages) {
        pg_next = pg_start;
    }
    return pg_next;
}

static inline void data_log_pg_init(struct data_log const* dl, struct data_page const* pg, uint32_t sn)
{
    struct data_page_hdr h = {
        .domain = dl->param->domain,
        .page_idx = data_log_pg_index(dl, pg),
        .unused_fragments = ~1,
        .fragment_ = ~0,
        .sn = sn
    };
    ble_flash_page_erase((unsigned)pg / DATA_PAGE_SZ);
    BUG_ON(~pg->h.sn);
    ble_flash_block_write((uint32_t*)&pg->h, (uint32_t*)&h, sizeof(h)/sizeof(uint32_t));
    BUG_ON(pg->h.sn != sn);
}

static inline void data_log_pg_mark_fragment_used(struct data_page const* pg, int fragment)
{
    struct data_page_hdr h = pg->h;
    bmap_clr_bit(&h.unused_fragments, fragment);
    ble_flash_block_write((uint32_t*)&pg->h, (uint32_t*)&h, sizeof(h)/sizeof(uint32_t));
    BUG_ON(bmap_get_bit(&pg->h.unused_fragments, fragment));
}

void data_log_put_item(struct data_log* dl, uint32_t item, uint32_t sn)
{
    int fragment = 0;
    if (!dl->next_item || dl->next_item >= DATA_PAGE_ITEMS || dl->suspended)
    {
        if (!dl->last_pg) {
            BUG_ON(dl->next_item);
            dl->first_pg = dl->last_pg = data_log_pg(dl, dl->param->pfirst);
        } else {
            dl->last_pg = data_log_pg_next(dl, dl->last_pg);
            if (dl->last_pg == dl->first_pg) {
                dl->first_pg = data_log_pg_next(dl, dl->first_pg);
            }
        }
        dl->next_item = 0;
        dl->suspended = 0;
        data_log_pg_init(dl, dl->last_pg, sn);
        bmap_set_bit(dl->param->pmap, data_log_pg_index(dl, dl->last_pg));
    }
    ble_flash_word_write((uint32_t*)&dl->last_pg->items[dl->next_item], item);
    ++dl->next_item;
    fragment = (OFFSETOF(struct data_page, items[dl->next_item]) + DATA_FRAG_SZ - 1) / DATA_FRAG_SZ - 1;
    if (bmap_get_bit(&dl->last_pg->h.unused_fragments, fragment)) {
        data_log_pg_mark_fragment_used(dl->last_pg, fragment);
    }
}

void data_log_initialize(
        struct data_log* dl,
        struct data_log_param const* param
    )
{
    int i;
    for (i = 0; i < param->npages; ++i) {
        bmap_clr_bit(param->pmap, param->pfirst + i);
    }
    dl->param = param;
    dl->first_pg = 0;
    dl->last_pg = 0;
    dl->next_item = 0;
    dl->suspended = 0;
}
