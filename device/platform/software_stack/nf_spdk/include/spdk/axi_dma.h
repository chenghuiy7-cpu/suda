#include "spdk/env.h"
#include "spdk/memory.h"
#pragma once

struct spdk_axi_dma_io;

struct spdk_axi_dma_dev {
    struct spdk_env_axi_dma_dev *env_dev;
};

struct spdk_axi_dma_ch {
    struct spdk_env_axi_dma_ch *env_ch;
    struct spdk_simple_pool io_pool;
    struct spdk_thread *thread;
    struct spdk_poller *cmpl_poller;
    uint32_t id;
};

struct spdk_axi_dma_ch_stat {
	uint32_t pkt_dropped;
	uint32_t pkt_processed;
};

typedef void (*spdk_axi_dma_cb_fn)(struct spdk_axi_dma_io *io, int status);

struct spdk_axi_dma_ctrl {
    uint32_t tuser : 16;
    uint32_t tdest : 4;
    uint32_t rsvd : 4;
    uint32_t tid : 8;
};

struct spdk_axi_dma_status {
	uint32_t transfered_bytes : 26;
	uint32_t rxeof : 1;
	uint32_t rxsof : 1;
	uint32_t int_err : 1;
	uint32_t slv_err : 1;
	uint32_t dec_err : 1;
	uint32_t completed : 1;
};

struct spdk_axi_dma_io {
    uint64_t start;
    uint64_t end;
    struct spdk_axi_dma_ctrl ctrl;
    struct spdk_axi_dma_status status;
    uint32_t transfered_length;
    int iovcnt;
    struct spdk_axi_dma_ch *ch;
    struct spdk_axi_dma_iovec *iovs;
    spdk_axi_dma_cb_fn cb;
    void *ctx;
    int used_iovcnt;
};

struct spdk_axi_dma_dev *spdk_axi_dma_get_device(const char *name);

struct spdk_axi_dma_ch *spdk_axi_dma_create_tx_channel(struct spdk_axi_dma_dev *dev, uint32_t num_bds, int id, uint8_t tid);

struct spdk_axi_dma_ch *spdk_axi_dma_create_rx_channel(struct spdk_axi_dma_dev *dev, uint32_t num_bds, int id, uint8_t tid);

int spdk_axi_dma_tx_channel_send(struct spdk_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, spdk_axi_dma_cb_fn cb, void *ctx, struct spdk_axi_dma_ctrl *ctrl);

int spdk_axi_dma_rx_channel_recv(struct spdk_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, spdk_axi_dma_cb_fn cb, void *ctx);

void spdk_axi_dma_io_free(struct spdk_axi_dma_io *io);

void spdk_axi_dma_ch_get_stat(struct spdk_axi_dma_ch *ch, struct spdk_axi_dma_ch_stat *stat);

int spdk_axi_dma_poller(void *ctx);

void spdk_axi_dma_stop_channel(struct spdk_axi_dma_ch *ch);

void spdk_axi_dma_enable_channel(struct spdk_axi_dma_ch *ch);

int spdk_axi_dma_tx_channel_send_seg(struct spdk_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, spdk_axi_dma_cb_fn cb, void *ctx, struct spdk_axi_dma_ctrl *ctrl,bool last_data);

struct spdk_axi_dma_io* spdk_axi_dma_poller_for_compute(void *ctx);