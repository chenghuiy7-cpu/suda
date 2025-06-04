#include "spdk/env.h"
#include "spdk/compute.h"

int spdk_compute_dev_init(struct spdk_compute_dev *dev, uint64_t bar_phys_addr)
{
    // Map physical address `bar_phys_addr` to virtual address `dev->bar`
    // using sizeof(struct spdk_compute_bar) as mapping size.
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        fprintf(stderr, "Failed to open /dev/mem\n");
        return -1;
    }

    dev->bar = mmap(NULL, sizeof(struct spdk_compute_bar), PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, bar_phys_addr);
    if (dev->bar == MAP_FAILED) {
        fprintf(stderr, "Failed to map BAR\n");
        close(mem_fd);
        return -1;
    }

    memset(dev->bar, 0, sizeof(struct spdk_compute_bar));

    // dev->bar->user_reset = 1;
    // spdk_delay_us(10);
    // dev->bar->user_reset = 0;

    return 0;
}

int spdk_compute_qpair_create(struct spdk_compute_dev *dev, struct spdk_compute_qpair *qpair, uint32_t ring_id)
{
    uint64_t phys_addr;

    qpair->sq = spdk_dma_zmalloc(sizeof(struct spdk_compute_sqe) * SPDK_COMPUTE_SQ_SIZE, PAGE_SIZE, &phys_addr);
    if (qpair->sq == NULL) {
        fprintf(stderr, "Failed to allocate SQ\n");
        return -1;
    }

    // printf("SQ %u phys_addr: 0x%lX\n", ring_id, phys_addr);

    dev->bar->ring_sq_phys_addr[ring_id] = phys_addr;

    for (int i = 0; i < SPDK_COMPUTE_SQ_SIZE; i++) {
        qpair->sq[i].csqe_id = i;
    }

    qpair->cq = spdk_dma_zmalloc(sizeof(struct spdk_compute_cqe) * SPDK_COMPUTE_CQ_SIZE, PAGE_SIZE, &phys_addr);
    if (qpair->cq == NULL) {
        fprintf(stderr, "Failed to allocate CQ\n");
        spdk_dma_free(qpair->sq);
        return -1;
    }
    // printf("CQ %u phys_addr: 0x%lX\n", ring_id, phys_addr);

    dev->bar->ring_cq_phys_addr[ring_id] = phys_addr;

    qpair->cb_fns = calloc(SPDK_COMPUTE_CQ_SIZE, sizeof(spdk_compute_cb_fn));
    qpair->cb_args = calloc(SPDK_COMPUTE_CQ_SIZE, sizeof(void *));
    qpair->sq_tail = qpair->cq_head = 0;
    qpair->dev = dev;
    qpair->ring_id = ring_id;
    qpair->phase = 1;

    return 0;
}

struct spdk_compute_sqe *spdk_compute_get_sqe(struct spdk_compute_qpair *qpair)
{
    return &qpair->sq[qpair->sq_tail];
}

int spdk_compute_submit_sqe(struct spdk_compute_qpair *qpair, struct spdk_compute_sqe *sqe, spdk_compute_cb_fn cb_fn, void *cb_arg)
{
    uint32_t tail = qpair->sq_tail;

    qpair->cb_fns[tail] = cb_fn;
    qpair->cb_args[tail] = cb_arg;

    qpair->sq_tail = (tail + 1) % SPDK_COMPUTE_SQ_SIZE;

    qpair->dev->bar->ring_tail_db[qpair->ring_id] = qpair->sq_tail;
    
    // printf("Submit SQE: ring_id %u tail %u\n", qpair->ring_id, qpair->sq_tail);

    return 0;
}

int spdk_compute_poll_cq(struct spdk_compute_qpair *qpair)
{
    uint32_t head = qpair->cq_head;
    struct spdk_compute_cqe cqe = qpair->cq[head];

    if (cqe.phase == qpair->phase) {
        uint32_t csqe_id = cqe.csqe_id;
        // printf("CQE: pfid %u csqe_id %u status %u phase %u\n", cqe.pf_id, cqe.csqe_id, cqe.status, cqe.phase);
        qpair->cq_head = (head + 1) % SPDK_COMPUTE_CQ_SIZE;
        if (qpair->cq_head == 0) {
            qpair->phase = 1 - qpair->phase;
        }
        qpair->cb_fns[csqe_id](&qpair->sq[csqe_id], cqe.status, qpair->cb_args[csqe_id]);
        return 1;
    } else {
        return 0;
    }
}