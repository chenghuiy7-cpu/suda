#ifndef _COMPUTE
#define _COMPUTE

#include "spdk/stdinc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPDK_COMPUTE_NUM_RINGS 4
#define SPDK_COMPUTE_SQ_SIZE 16
#define SPDK_COMPUTE_CQ_SIZE 16
#define SPDK_COMPUTE_BAR_PHYS_ADDR 0xB2000000

struct spdk_compute_stream {
    uint64_t ops : 32;
    uint64_t len_blocks : 16;
    uint64_t input1 : 8;
    uint64_t input2 : 8;
    uint64_t prp1; // PS DDR address is always physically contiguous, so only PRP1 is used
    uint64_t prp2; // x86 address can be non-contiguous, so PRP2 is used to specify the PRP list
};

struct spdk_compute_sqe {
    uint64_t pf_id : 8;
    uint64_t csqe_id : 8;
    uint64_t num_streams : 8;
    uint64_t sqid : 8;
    uint64_t rsvd : 32;
    struct spdk_compute_stream streams[5];
    // uint64_t padding;
};

struct spdk_compute_cqe {
    uint64_t pf_id : 8;
    uint64_t csqe_id : 8;
    uint64_t status : 8;
    uint64_t rsvd : 40;
    uint64_t phase : 1;
    uint64_t padding : 63;
};

struct spdk_compute_bar {
    volatile uint64_t ring_sq_phys_addr[SPDK_COMPUTE_NUM_RINGS];
    volatile uint64_t ring_cq_phys_addr[SPDK_COMPUTE_NUM_RINGS];
    volatile uint64_t ring_tail_db[SPDK_COMPUTE_NUM_RINGS];
};

typedef void (*spdk_compute_cb_fn)(struct spdk_compute_sqe *sqe, uint32_t status, void *cb_arg);

struct spdk_compute_qpair {
    struct spdk_compute_sqe *sq;
    struct spdk_compute_cqe *cq;
    spdk_compute_cb_fn *cb_fns;
    void **cb_args;
    struct spdk_compute_dev *dev;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t ring_id;
    uint8_t phase;
};

struct spdk_compute_dev {
    struct spdk_compute_bar *bar;
    struct spdk_compute_qpair *qpairs[SPDK_COMPUTE_NUM_RINGS];
};

int spdk_compute_dev_init(struct spdk_compute_dev *dev, uint64_t bar_phys_addr);

int spdk_compute_qpair_create(struct spdk_compute_dev *dev, struct spdk_compute_qpair *qpair, uint32_t ring_id);

struct spdk_compute_sqe *spdk_compute_get_sqe(struct spdk_compute_qpair *qpair);

int spdk_compute_submit_sqe(struct spdk_compute_qpair *qpair, struct spdk_compute_sqe *sqe, spdk_compute_cb_fn cb_fn, void *cb_arg);

int spdk_compute_poll_cq(struct spdk_compute_qpair *qpair);

#ifdef __cplusplus
}
#endif

#endif
