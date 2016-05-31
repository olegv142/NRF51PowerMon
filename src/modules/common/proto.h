#pragma once

#include <stdint.h>

#define PROTOCOL_VERSION 1
#define PROTOCOL_MAGIC   0x766f7661
#define PROTOCOL_CHANNEL 0

// System status flags
#define STATUS_CONNECTED 1    // connection mode
#define STATUS_CHARGED   0x10 // Vbatt >= 4.1V, charging stopped
#define STATUS_LOW_BATT  0x40 // Vbatt <= 3.5V, connection mode disabled
#define STATUS_HIBERNATE 0x80 // Vbatt <= 3.2V, power measurement disabled, slowly monitoring battery

#ifndef TEST
// Measuring period
#define MEASURING_PERIOD 12
// Data storing period
#define FAST_PW_PERIOD MEASURING_PERIOD
#define SLOW_PW_PERIOD 3600 // every hour
#define VBATT_PERIOD   600  // every 10 minutes
#else
// Fast mode for testing
#define MEASURING_PERIOD 1
#define FAST_PW_PERIOD   MEASURING_PERIOD
#define SLOW_PW_PERIOD   3
#define VBATT_PERIOD     2
#endif

// Data domain identifiers
typedef enum {
    dom_fast_pw = 0,
    dom_slow_pw,
    dom_vbatt,
    dom_count,
} data_domain_t;

#define DATA_PG_FRAGMENTS 8
#define DATA_PAGE_SZ 1024
#define DATA_FRAG_SZ (DATA_PAGE_SZ/DATA_PG_FRAGMENTS)

#define FAST_PW_PAGES 200 // ~ 2 weeks
#define SLOW_PW_PAGES 20  // ~ 1 year
#define VBATT_PAGES   4   // ~ 2 weeks

#define DATA_PAGES (FAST_PW_PAGES+SLOW_PW_PAGES+VBATT_PAGES)
#define DATA_PG_BITMAP_SZ   (DATA_PAGES/8) // 28  bytes to store bits per every page

// Data page header
struct data_page_hdr {
	uint8_t  domain;           // data domain 
	uint8_t  page_idx;         // page index
	uint8_t  unused_fragments; // bitmap of unused fragments
	uint8_t  fragment_;        // fragment bit (used only in struct data_packet)
	uint32_t sn;               // first data item sequence number
};

#define DATA_PAGE_HDR_SZ sizeof(struct data_page_hdr)
#define DATA_PAGE_ITEMS ((DATA_PAGE_SZ-DATA_PAGE_HDR_SZ)/4)

// Data page
struct data_page {
    struct data_page_hdr h;
    union {
        uint32_t items [DATA_PAGE_ITEMS];
        uint16_t ishort[2*DATA_PAGE_ITEMS];
        uint8_t  ibytes[4*DATA_PAGE_ITEMS];
    };
};

// Data page split onto fragments
typedef union data_page_fragmented {
    struct data_page data;
    uint8_t          fragment[DATA_PG_FRAGMENTS][DATA_FRAG_SZ];
} data_page_fragmented_t;

// Packet types
typedef enum {
	packet_report,
	packet_data_req,
	packet_data,
} packet_type_t;

// Common packet header
struct packet_hdr {
    uint8_t  sz;
	uint8_t  version;
	uint8_t  type;
	uint8_t  status;
	uint32_t magic;
};

// Periodic report
struct report_packet {
	struct packet_hdr hdr;
	uint32_t          sn;
	uint16_t          power;
	uint16_t          vbatt;
	uint8_t           page_bitmap[DATA_PG_BITMAP_SZ];
};

// Data page fragment
struct data_req_packet {
	struct packet_hdr hdr;
    uint32_t          cookie;
	uint8_t           fragment_bitmap[DATA_PAGES];
};

// Required fragments bitmap from the client
struct data_packet {
	struct packet_hdr    hdr;
    uint32_t             cookie;
	struct data_page_hdr pg_hdr;
	uint8_t              fragment[DATA_FRAG_SZ];
};
