#include "spdk/hlsacccompute.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"
#include "spdk/rpc.h"
#include "spdk/jsonrpc.h"

static bool test_app_fin_code = false;
unsigned long long *in_data;
unsigned long long *out_data;
double start_time;
struct app_opts {
    void *mmap_addr;
    size_t mmap_size;
};

static struct app_opts g_opts = {};

static void
usage(const char *program_name)
{
    printf("Usage: %s -a [address] -s [size]\n", program_name);
    printf("Options:\n");
    printf(" -a addr  Virtual address to mmap (hex format)\n");
    printf(" -s size  Size to mmap (in bytes)\n");
}

static int
parse_arg(int ch, char *arg)
{
    switch (ch) {
    case 'a':
        // 将十六进制字符串转换为指针
        g_opts.mmap_addr = (void *)strtoul(arg, NULL, 16);
        break;
    case 's':
        // 解析大小参数
        g_opts.mmap_size = strtoul(arg, NULL, 0);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
static void spdk_hlsacccompute_rx_channel_cmpl(struct spdk_axi_dma_io *io, int status)
{
    SPDK_NOTICELOG("NICE FINISH RX CHANNEL SENDING!!!!!CHANNEL%d\n",io->ch->id);
    bool flag = true;
    for(int i=0;i<PAGE_SIZE;i++)
    {
        if(((char*)(io->iovs[1].iov_base))[i]!='A'){
            flag = false;
        }
    }
    if(flag){
        SPDK_NOTICELOG("TRANSFER SUCCESSFUL!!!\n");
    }else{
        SPDK_NOTICELOG("TRANSFER FAILED!!!\n");
    }
}

static void spdk_hlsacccompute_tx_channel_cmpl(struct spdk_axi_dma_io *io, int status)
{
    SPDK_NOTICELOG("NICE FINISH TX CHANNEL SENDING!!!!!\n");
}

static void compute_start(void *arg)
{

    struct spdk_hlsacccompute_dev *dev = (struct spdk_hlsacccompute_dev *)calloc(1, sizeof(struct spdk_hlsacccompute_dev));
    int ret;

    ret = spdk_hlsacccompute_dev_init(dev, SPDK_HLSACCCOMPUTE_BAR_PHYS_ADDR);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize compute device\n");
        return;
    }
    const char *g_mcdma_dev = "b0000000.dma";
    struct spdk_axi_dma_dev *mcdma = spdk_axi_dma_get_device(g_mcdma_dev);
  
    SPDK_NOTICELOG("Get AXI DMA ADDRESS%ld\n", mcdma);
    struct spdk_axi_dma_ch *ch1;
    /*
    for(int i=0;i<=5;i++){
        ch1 = spdk_zmalloc(sizeof(struct spdk_axi_dma_ch), 2, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
        ch1->env_ch = spdk_env_axi_dma_create_rx_channel(mcdma->env_dev, 32, i, i);
        if (!ch1->env_ch)
        {
            SPDK_ERRLOG("Failed to create RX channel\n");
            spdk_free(ch1);
            return NULL;
        }
        ch1->thread = spdk_get_thread();
        ch1->cmpl_poller = spdk_poller_register(spdk_axi_dma_poller, ch1, 0);
        ch1->id = i;
        if ((spdk_simple_pool_init(&ch1->io_pool, 32, sizeof(struct spdk_axi_dma_io)) != 0))
        {
            SPDK_NOTICELOG("Failed to allocate axi dma io pool for channel 5\n"); // 0xae0205000
        }
        struct spdk_axi_dma_iovec *iovs1 = calloc(32, sizeof(struct spdk_axi_dma_iovec));
       // iovs1[0].iov_base = spdk_zmalloc(PAGE_SIZE, PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
       // memset(iovs1[0].iov_base, 0, sizeof(PAGE_SIZE));
       // iovs1[0].iov_len = PAGE_SIZE;
        //iovs1[0].paddr = spdk_vtophys(iovs1[0].iov_base, NULL);
       // iovs1[0].paddr = 0x840000039c4fe000;
       // SPDK_NOTICELOG("PADDR GENERATED%llx\n", iovs1[0].paddr);
       // spdk_axi_dma_rx_channel_recv(ch1, iovs1, 1, spdk_hlsacccompute_rx_channel_cmpl, NULL);
        struct spdk_axi_dma_io *io = spdk_simple_pool_get(&ch1->io_pool);
        if (!io) {
            SPDK_ERRLOG("Failed to allocate spdk_axi_dma_io\n");
            return -ENOMEM;
        }

        io->ch = ch1;
        io->iovs = iovs1;
        io->iovcnt = 2;
        io->cb = spdk_hlsacccompute_rx_channel_cmpl;
        io->ctx = NULL;
        io->transfered_length = 0;
        io->iovs[0].paddr =  g_opts.mmap_addr;
        io->iovs[0].iov_len = PAGE_SIZE;
        io->iovs[0].iov_base = io;
        for (int i = 0; i < 1; i++) {
            uint64_t len = io->iovs[i].iov_len;
            io->transfered_length += len;
        }
        io->iovs[1].iov_len = PAGE_SIZE;
        io->iovs[1].iov_base = spdk_zmalloc(PAGE_SIZE,PAGE_SIZE,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_SHARE);
        io->iovs[1].paddr =  spdk_vtophys(io->iovs[1].iov_base,NULL);
        for (int i = 0; i < 1; i++) {
            uint64_t len = io->iovs[i].iov_len;
            io->transfered_length += len;
        }
        ret = spdk_env_axi_dma_rx_channel_recv(ch1->env_ch,iovs1,2,io);
    }*/
    /*
    struct spdk_axi_dma_ch *ch2 = spdk_malloc(sizeof(struct spdk_axi_dma_ch), 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
    ch2->env_ch = spdk_env_axi_dma_create_tx_channel(mcdma->env_dev, 32, 5, 5);
    if (!ch2->env_ch)
    {
        SPDK_ERRLOG("Failed to create TX channel\n");
        spdk_free(ch2);
        return NULL;
    }
    ch2->thread = spdk_get_thread();
    ch2->cmpl_poller = spdk_poller_register(spdk_axi_dma_poller, ch2, 0);
    ch2->id = 5;
    if ((spdk_simple_pool_init(&ch2->io_pool, 32, sizeof(struct spdk_axi_dma_io)) != 0))
    {
        SPDK_NOTICELOG("Failed to allocate axi dma io pool for channel 5\n");
    }
    struct spdk_axi_dma_iovec *iovs = calloc(32, sizeof(struct spdk_axi_dma_iovec));
    iovs[0].iov_base = spdk_zmalloc(PAGE_SIZE, PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    iovs[0].iov_len = PAGE_SIZE;
    iovs[0].paddr = spdk_vtophys(iovs[0].iov_base, NULL);
    memset(iovs[0].iov_base, 0, PAGE_SIZE);
    struct spdk_axi_dma_ctrl ctrl;
    ctrl.tdest = 5;
    ctrl.tid = 5;
    ctrl.tuser = 0;
    struct spdk_axi_dma_io *io = spdk_simple_pool_get(&ch2->io_pool);
    if (!io) {
        SPDK_ERRLOG("Failed to allocate spdk_axi_dma_io\n");
        return -ENOMEM;
    }

    io->ch = ch2;
    io->iovs = iovs;
    io->iovcnt = 2;
    io->cb = spdk_hlsacccompute_tx_channel_cmpl;
    io->ctx = NULL;
    io->transfered_length = 0;
    if (&ctrl != NULL) {
        memcpy(&io->ctrl, &ctrl, sizeof(struct spdk_axi_dma_ctrl));
    }
    
    io->iovs[0].iov_len = PAGE_SIZE;
    io->iovs[0].iov_base = spdk_zmalloc(PAGE_SIZE,PAGE_SIZE,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_DMA);
    io->iovs[0].paddr = spdk_vtophys(io->iovs[0].iov_base,NULL);
    for(int i=0;i<PAGE_SIZE;i++)
    {
        ((char*)(io->iovs[0].iov_base))[i] = 'Y';
    }
    for (int i = 0; i < 1; i++) {
        uint64_t len = io->iovs[i].iov_len;
        io->transfered_length += len;
    }
    io->iovs[1].iov_len = PAGE_SIZE;
    io->iovs[1].iov_base = spdk_zmalloc(PAGE_SIZE,PAGE_SIZE,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_DMA);
    io->iovs[1].paddr = g_opts.mmap_addr;
    for (int i = 0; i < 1; i++) {
        uint64_t len = io->iovs[i].iov_len;
        io->transfered_length += len;
    }

    ret = spdk_env_axi_dma_tx_channel_send_seg(ch2->env_ch, iovs, 2, io,false);*/

    struct spdk_axi_dma_ch *ch3 = spdk_malloc(sizeof(struct spdk_axi_dma_ch), 0, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);
    ch3->env_ch = spdk_env_axi_dma_create_tx_channel(mcdma->env_dev, 32, 0, 0);
    if (!ch3->env_ch)
    {
        SPDK_ERRLOG("Failed to create TX channel\n");
        spdk_free(ch3);
        return NULL;
    }
    ch3->thread = spdk_get_thread();
    ch3->cmpl_poller = spdk_poller_register(spdk_axi_dma_poller, ch3, 0);
    ch3->id = 0;
    if ((spdk_simple_pool_init(&ch3->io_pool, 32, sizeof(struct spdk_axi_dma_io)) != 0))
    {
        SPDK_NOTICELOG("Failed to allocate axi dma io pool for channel 5\n");
    }
    struct spdk_axi_dma_iovec *iovs = calloc(32, sizeof(struct spdk_axi_dma_iovec));
    iovs[0].iov_base = spdk_zmalloc(PAGE_SIZE, PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    iovs[0].iov_len = PAGE_SIZE;
    iovs[0].paddr = spdk_vtophys(iovs[0].iov_base, NULL);
    memset(iovs[0].iov_base, 0, PAGE_SIZE);
    struct spdk_axi_dma_ctrl ctrl;
    ctrl.tdest = 0;
    ctrl.tid = 0;
    ctrl.tuser = 0;
    struct spdk_axi_dma_io *io = spdk_simple_pool_get(&ch3->io_pool);
    if (!io) {
        SPDK_ERRLOG("Failed to allocate spdk_axi_dma_io\n");
        return -ENOMEM;
    }

    io->ch = ch3;
    io->iovs = iovs;
    io->iovcnt = 1;
    io->cb = spdk_hlsacccompute_tx_channel_cmpl;
    io->ctx = NULL;
    io->transfered_length = 0;
    if (&ctrl != NULL) {
        memcpy(&io->ctrl, &ctrl, sizeof(struct spdk_axi_dma_ctrl));
    }
    
    io->iovs[0].iov_len = PAGE_SIZE;
    io->iovs[0].iov_base = spdk_zmalloc(PAGE_SIZE,PAGE_SIZE,NULL,SPDK_ENV_SOCKET_ID_ANY,SPDK_MALLOC_DMA);
    io->iovs[0].paddr = spdk_vtophys(io->iovs[0].iov_base,NULL);
    for(int i=0;i<PAGE_SIZE;i++)
    {
        ((char*)(io->iovs[0].iov_base))[i] = 'Y';
    }
    for (int i = 0; i < 1; i++) {
        uint64_t len = io->iovs[i].iov_len;
        io->transfered_length += len;
    }
    ret = spdk_env_axi_dma_tx_channel_send_seg(ch3->env_ch, iovs, 1, io,false);

}



int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 0;

    /* Set default values in opts structure. */

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "hello_bdev";

    /*
     * spdk_app_start() will initialize the SPDK framework, call hello_start(),
     * and then block until spdk_app_stop() is called (or if an initialization
     * error occurs, spdk_app_start() will return with rc even without calling
     * hello_start().
     */
    // 解析命令行参数
    g_opts.mmap_addr = NULL;
    if ((rc = spdk_app_parse_args(argc, argv, &opts, "a:", NULL,
                                 parse_arg, usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
        exit(rc);
    }
    printf("RECV_ADDRESS%llx\n",g_opts.mmap_addr);
    if(g_opts.mmap_addr==NULL){
        printf("Failed To Get Address!\n");
    }
    rc = spdk_app_start(&opts, compute_start, NULL);
    if (rc)
    {
        SPDK_ERRLOG("ERROR starting application\n");
    }

    spdk_app_fini();
    return rc;
    return 0;
}
