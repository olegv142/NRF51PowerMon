#pragma once

#include <stdint.h>

#define PROTOCOL_MAGIC 0 // FIXME!

// System status flags
#define STATUS_CONNECTED 1
#define STATUS_CHARGED   0x80
#define STATUS_LOW_BATT  0x100

#ifndef TEST
// Measuring period
#define MEASURING_PERIOD 12
// Data storing period
#define FAST_PW_PERIOD MEASURING_PERIOD
#define SLOW_PW_PERIOD 3600 // every hour
#define VCC_PERIOD     600  // every 10 minutes
#else
// Fast mode for testing
#define MEASURING_PERIOD 1
#define FAST_PW_PERIOD MEASURING_PERIOD
#define SLOW_PW_PERIOD 3
#define VCC_PERIOD     2
#endif

// Data domain identifiers
typedef enum {
    dom_fast_pw = 0,
    dom_slow_pw,
    dom_vcc,
    dom_count,
} data_domain_t;

#define DATA_PAGE_SZ 1024
#define DATA_FRAG_SZ (DATA_PAGE_SZ/8)

#define FAST_PW_PAGES 200 // ~ 2 weeks
#define SLOW_PW_PAGES 20  // ~ 1 year
#define VCC_PAGES     4   // ~ 2 weeks

#define DATA_PAGES (FAST_PW_PAGES+SLOW_PW_PAGES+VCC_PAGES)
#define DATA_PG_BITMAP_SZ   (DATA_PAGES/8)        // 28  bytes to store bits per every page
#define DATA_FRAG_BITMAP_SZ (DATA_PG_BITMAP_SZ*8) // 224 bytes to store bits per every fragment

// Data page header
struct data_page_hdr {
	uint8_t  domain;
	uint8_t  unused_fragments;
	uint8_t  fragment;
	uint8_t  reserved;
	uint32_t sn;
};

#define DATA_PAGE_HDR_SZ sizeof(struct data_page_hdr)
#define DATA_PAGE_ITEMS ((DATA_PAGE_SZ-DATA_PAGE_HDR_SZ)/4)

// Data page
struct data_page {
    struct data_page_hdr h;
    union {
        uint32_t items[DATA_PAGE_ITEMS];
        uint16_t ishort[2*DATA_PAGE_ITEMS];
        uint8_t  ibytes[4*DATA_PAGE_ITEMS];
    };
};

// Data page split onto fragments
union data_page_fragmented {
    struct data_page data;
    uint8_t          fragments[8][DATA_FRAG_SZ];
};

// Packet types
typedef enum {
	packet_report,
	packet_data_req,
	packet_data,
} packet_type_t;

// Common packet header
struct packet_hdr {
	uint32_t magic;
	uint8_t  version;
	uint8_t  type;
	uint16_t status;
};

// Periodic report
struct report_packet {
	struct packet_hdr hdr;
	uint32_t sn;
	uint16_t power;
	uint16_t vcc;
    uint8_t  measuring_period;
	uint8_t  page_bitmap[DATA_PG_BITMAP_SZ];
};

// Data page fragment
struct data_req_packet {
	struct packet_hdr hdr;
	uint8_t fragment_bitmap[DATA_FRAG_BITMAP_SZ];
};

// Required fragments bitmap from the client
struct data_packet {
	struct packet_hdr    hdr;
	struct data_page_hdr pg_hdr;
	uint8_t              data[DATA_FRAG_SZ];
};
