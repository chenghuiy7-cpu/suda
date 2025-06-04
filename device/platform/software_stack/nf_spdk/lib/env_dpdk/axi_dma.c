#include <rte_axi_dma.h>
#include <rte_malloc.h>

#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/thread.h"

struct spdk_env_axi_dma_dev {
    struct rte_axi_dma_device dev;
};

struct spdk_env_axi_dma_ch {
    struct rte_axi_dma_channel ch;
};

struct spdk_env_axi_dma_ch_stat {
    struct rte_axi_dma_channel_stat stat;
};

struct spdk_env_axi_dma_dev *spdk_env_axi_dma_get_device(const char *name)
{
    return (struct spdk_env_axi_dma_dev *)rte_axi_dma_get_device(name);
}

struct spdk_env_axi_dma_ch *spdk_env_axi_dma_create_tx_channel(struct spdk_env_axi_dma_dev *dev, uint32_t num_bds, int id, uint8_t tid)
{
    return (struct spdk_env_axi_dma_ch *)rte_axi_dma_create_tx_channel((struct rte_axi_dma_device *)dev, id, num_bds, tid);
}

struct spdk_env_axi_dma_ch *spdk_env_axi_dma_create_rx_channel(struct spdk_env_axi_dma_dev *dev, uint32_t num_bds, int id, uint8_t tid)
{
    return (struct spdk_env_axi_dma_ch *)rte_axi_dma_create_rx_channel((struct rte_axi_dma_device *)dev, id, num_bds, tid);
}

/* Expects physical addresses in iovs */
int spdk_env_axi_dma_tx_channel_send(struct spdk_env_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, void *ctx)
{
    return rte_axi_dma_send((struct rte_axi_dma_channel *)ch, iovs, iovcnt, ctx);
}

/* Expects physical addresses in iovs */
int spdk_env_axi_dma_rx_channel_recv(struct spdk_env_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, void *ctx)
{
    return rte_axi_dma_recv((struct rte_axi_dma_channel *)ch, iovs, iovcnt, ctx);
}

void *spdk_env_axi_dma_poll_complete(struct spdk_env_axi_dma_ch *ch)
{
    void *ret = rte_axi_dma_poll_complete((struct rte_axi_dma_channel *)ch);
    // if (ret) {
    //     printf("ret is %llX\n", ret);
    // }
    return ret;
}

void spdk_env_axi_dma_channel_get_stat(struct spdk_env_axi_dma_ch *ch, struct spdk_env_axi_dma_channel_stat *stat)
{
    rte_axi_dma_channel_get_stat((struct rte_axi_dma_channel *)ch, (struct rte_axi_dma_channel_stat *)stat);
}

void spdk_env_axi_dma_stop_channel(struct spdk_env_axi_dma_ch *ch){
    rte_axi_dma_stop_channel((struct rte_axi_dma_channel *)ch);
}

void spdk_env_axi_dma_enable_channel(struct spdk_env_axi_dma_ch *ch){
    rte_axi_dma_enable_channel((struct rte_axi_dma_channel *)ch);
}

int spdk_env_axi_dma_tx_channel_send_seg(struct spdk_env_axi_dma_ch *ch, struct spdk_axi_dma_iovec *iovs, int iovcnt, void *ctx,bool last_data){
    return rte_axi_dma_send_seg((struct rte_axi_dma_channel *)ch,iovs,iovcnt,ctx,last_data);
}