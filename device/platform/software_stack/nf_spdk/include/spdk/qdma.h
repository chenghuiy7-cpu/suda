#include "spdk/stdinc.h"

#define AR_BURST_SZ 4
#define QDMA_BYP_RING_SIZE 256

#define MCDMA_R_BRIDGE_RX_TID 0x26
#define MCDMA_R_BRIDGE_TX_TID 0x1e
#define MCDMA_QDMA_H2C_BYP_OUT_RX_TID 0x1e
#define MCDMA_AR_BRIDGE_RX_TID 0x1f
#define MCDMA_AR_BRIDGE_TX_TID 0x1f

#define MCDMA_W_BRIDGE_RX_MIN_TID 0x16
#define MCDMA_W_BRIDGE_RX_MAX_TID 0x1d
#define MCDMA_AW_BRIDGE_RX_TID 0x21
#define MCDMA_AW_BRIDGE_TX_TID 0x21
#define MCDMA_QDMA_C2H_BYP_OUT_RX_TID 0x22
#define MCDMA_QDMA_CRDT_RX_TID 0x32

#define PFCH_TAG_LSB_ADDR 0xB1030000
#define PFCH_TAG_MSB_ADDR 0xB1030008

struct qdma_h2c_desc {
	uint32_t metadata;
	uint16_t len;
	uint16_t rsvd;
	uint64_t addr;
};

struct qdma_c2h_desc {
	uint64_t addr;
};

struct qdma_h2c_byp_in {
	struct qdma_h2c_desc dsc;
	uint32_t _rsvd[4];
	uint32_t st_mm : 1;
	uint32_t error : 1;
	uint32_t port_id : 3;
	uint32_t fmt : 3;
	uint32_t qid : 11;
	uint32_t dsc_sz : 2;
	uint32_t _padding : 11;
	// uint32_t flags;
	uint32_t cidx : 16;
	uint32_t func : 8;
	uint32_t cur_cidx_lsb : 8;
	uint32_t cur_cidx_msb : 2;
};

struct qdma_c2h_byp_in {
	struct qdma_c2h_desc dsc;
	uint32_t _rsvd[6];
	uint32_t st_mm : 1;
	uint32_t error : 1;
	uint32_t port_id : 3;
	uint32_t fmt : 3;
	uint32_t qid : 11;
	uint32_t dsc_sz : 2;
	uint32_t _padding : 11;
	// uint32_t flags;
	uint32_t cidx : 16;
	uint32_t func : 8;
	uint32_t cur_cidx_lsb : 8;
	uint32_t cur_cidx_msb : 2;
	uint32_t pfch_tag : 7;
};

struct qdma_h2c_byp_out {
	uint64_t addr;
	uint32_t qid : 11;
	uint32_t port_id : 3;
	uint32_t len : 16;
	uint32_t padding : 2;
	// uint16_t cidx;
	// uint32_t func : 8;
};

struct qdma_ar_req {
	uint64_t addr : 48;
	uint64_t cid : 10;
	uint64_t qid : 5;
	uint64_t _one : 1;
	uint32_t burst_size : 3;
	uint32_t burst_len : 8;
	uint32_t arid : 3;
	uint32_t _rsvd : 18;
};

struct qdma_crdt {
	uint64_t qid : 11;
	uint64_t avl : 16;
	uint64_t pidx : 14;
	uint64_t port_id : 3;
	uint64_t byp : 1;
	uint64_t dir : 1;
	uint64_t mm : 1;
	uint64_t qinv : 1;
	uint64_t qen : 1;
	uint64_t irq_arm : 1;
	uint64_t error : 1;
};