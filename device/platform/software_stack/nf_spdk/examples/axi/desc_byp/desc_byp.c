#include "spdk/axi_dma.h"
#include "spdk/qdma.h"

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_zone.h"

#define IOVCNT 1
#define BLK_SIZE 512
#define BUF_ALIGN 4096
#define NUM_DMA_DEVS 2

#define CONT_BD_NUM 1

struct hello_context_t;
struct hello_channel;

struct channel_io {
	struct spdk_axi_dma_iovec *iovs;
	int iovcnt;
	struct hello_channel *ch;
};

struct hello_channel {
	struct spdk_axi_dma_ch *data_tx_ch;
	struct spdk_axi_dma_ch *data_rx_ch;
	struct spdk_axi_dma_ch *byp_tx_ch;
	struct spdk_axi_dma_ch *byp_rx_ch;
	struct channel_io *byp_tx_io;
	struct channel_io *data_tx_io;
	struct channel_io *data_ios;
	struct channel_io *byp_ios;
	uint64_t tx_cnt;
	uint64_t rx_cnt;
	uint64_t dropped;
	uint64_t last_idx;
	struct hello_context_t *ctx;
	struct spdk_axi_dma_ch_stat stat;
	bool is_byp;
	bool byp_tx_sending;
	bool data_tx_sending;
};

/*
 * We'll use this struct to gather housekeeping hello_context to pass between
 * our events and callbacks.
 */
struct hello_context_t {
	struct spdk_axi_dma_dev *dev;
	struct hello_channel ch;
	uint64_t total_time;
	uint64_t idx;
	// uint64_t max_time;
	// uint64_t min_time;
	// uint64_t dma_total_time;
};

static char *g_dma_dev_names[] = {
	"b0000000.dma",
};

static struct spdk_axi_dma_dev *devs[NUM_DMA_DEVS] = {0};
static struct hello_context_t ctxs[16] = {0};
static int g_num_channels = 0;

static uint32_t g_num_bds = 2048;
static uint32_t g_qd = 1;
static uint32_t g_burst_size = 1;
static bool g_use_ar_format = true;

static uint32_t max_count = 128;

static uint32_t data_tx_tid = UINT32_MAX;
static uint32_t data_rx_tid = UINT32_MAX;
static uint32_t byp_tx_tid = UINT32_MAX;
static uint32_t byp_rx_tid = UINT32_MAX;

static struct spdk_ring *byp_out_rx_ring;
static struct spdk_ring *byp_in_tx_ring;

static void desc_byp_in_cmpl_cb(struct spdk_axi_dma_io *io, int status);

/*
 * This function is called to parse the parameters that are specific to this application
 */
static int hello_bdev_parse_arg(int ch, char *arg)
{
	// printf("ch is %c\n", (char)ch);
	switch ((char)ch) {
	case 'I':
		data_rx_tid = atoi(arg);
		break;
	case 'O':
		data_tx_tid = atoi(arg);
		break;
	case 'D':
		byp_rx_tid = atoi(arg);
		break;
	case 'T':
		byp_tx_tid = atoi(arg);
		break;
	case 'S':
		g_num_bds = atoi(arg);
		break;
	case 'q':
		g_qd = atoi(arg);
		break;
	case 'b':
		g_burst_size = atoi(arg);
		break;
	case 'a':
		g_use_ar_format = true;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Usage function for printing parameters that are specific to this application
 */
static void
hello_bdev_usage(void)
{
	printf(" -I <dma_addr>                 name of the TX axi dma device to use\n");
	printf(" -O <dma_addr>                 name of the RX axi dma device to use\n");
}

static void tx_cmpl_cb(struct spdk_axi_dma_io *io, int status);
static void rx_cmpl_cb(struct spdk_axi_dma_io *io, int status);

static void tx_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;
	struct spdk_axi_dma_ch *ch = io->ch;

	hello_ch->data_tx_sending = false;

	fprintf(stderr, "cmpl TX: %p\n", io);
	
	hello_ch->tx_cnt += io->iovcnt;
	hello_ch->ctx->total_time += (io->end - io->start);

	spdk_axi_dma_io_free(io);
}

static void desc_byp_out_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;

	hello_ch->byp_tx_sending = false;

	fprintf(stderr, "cmpl desc_byp_out: %p\n", io);
	
	hello_ch->tx_cnt += io->iovcnt;
	hello_ch->ctx->total_time += (io->end - io->start);

	spdk_axi_dma_io_free(io);
}

static inline void print_sqe(void *data)
{
	uint64_t *p = data;
	printf("SQE %p\n", p);
	for (int i = 0; i < 4; i++) {
		printf("%d %016lX %016lX\n", i, p[2 * i], p[2 * i + 1]);
	}
}

static void h2c_desc_byp_in_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;
	struct qdma_h2c_byp_in *byp_in = (struct qdma_h2c_byp_in *)io->iovs[0].iov_base;
	struct spdk_axi_dma_ctrl ctrl;

	fprintf(stderr, "Got byp: Addr %lX len %lu port_id %u qid %u cidx %u cur_cidx %u\n", byp_in->dsc.addr, byp_in->dsc.len, byp_in->port_id, byp_in->qid, byp_in->cidx, byp_in->cur_cidx_lsb + (byp_in->cur_cidx_msb << 8));

	for (int i = 0; i < CONT_BD_NUM; i++) {
		struct spdk_axi_dma_iovec *iov = &hello_ch->byp_tx_io->iovs[i];
		if (g_use_ar_format) {
			struct qdma_ar_req *ar_req = iov->iov_base;
			ar_req->addr = byp_in->dsc.addr;
			ar_req->cid = 0;
			ar_req->qid = 1;
			ar_req->arid = spdk_get_ticks() % 8;
			ar_req->burst_size = AR_BURST_SZ;
			ar_req->burst_len = (byp_in->dsc.len - 1) >> AR_BURST_SZ;

			printf("Send ar_req: addr %lX cid %u qid %u arid %u burst_size %u burst_len %u\n",
				ar_req->addr, ar_req->cid, ar_req->qid, ar_req->arid, ar_req->burst_size, ar_req->burst_len);
		} else {
			struct qdma_h2c_byp_out *byp_out = iov->iov_base;

			byp_out->addr = byp_in->dsc.addr;
			byp_out->len = byp_in->dsc.len;
			// byp_out->func = byp_in->func;
			byp_out->qid = byp_in->qid;
			// byp_out->cidx = byp_in->cidx;
			byp_out->port_id = spdk_get_ticks() % 8;
			// byp_out->port_id = byp_in->port_id;
			printf("Send byp: port_id %u\n", byp_out->port_id);
		}
	}

	ctrl.tid = MCDMA_AW_BRIDGE_TX_TID;
	spdk_axi_dma_tx_channel_send(hello_ch->byp_tx_ch, hello_ch->byp_tx_io->iovs, hello_ch->byp_tx_io->iovcnt, desc_byp_out_cmpl_cb, hello_ch->byp_tx_io, &ctrl);

	spdk_axi_dma_rx_channel_recv(hello_ch->byp_rx_ch, io->iovs, io->iovcnt, desc_byp_in_cmpl_cb, cio);

	spdk_axi_dma_io_free(io);
}

static void c2h_desc_byp_in_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;
	struct qdma_c2h_byp_in *byp_in = calloc(1, sizeof(struct qdma_c2h_byp_in));
	// struct spdk_axi_dma_ctrl ctrl;

	memcpy(byp_in, io->iovs[0].iov_base, sizeof(struct qdma_c2h_byp_in));

	fprintf(stderr, "Got byp: Addr %lX port_id %u qid %u cidx %u cur_cidx %u pfch_tag %X\n", byp_in->dsc.addr, byp_in->port_id, byp_in->qid, byp_in->cidx, byp_in->cur_cidx_lsb + (byp_in->cur_cidx_msb << 8), byp_in->pfch_tag);

	spdk_ring_enqueue(byp_out_rx_ring, (void **)&byp_in, 1, NULL);

	// for (int i = 0; i < CONT_BD_NUM; i++) {
	// 	struct spdk_axi_dma_iovec *iov = &hello_ch->byp_tx_io->iovs[i];
	// 	if (g_use_ar_format) {
	// 		struct qdma_ar_req *ar_req = iov->iov_base;
	// 		ar_req->addr = byp_in->dsc.addr;
	// 		ar_req->cid = 0;
	// 		ar_req->qid = byp_in->qid + 1;
	// 		ar_req->arid = byp_in->port_id;
	// 		ar_req->burst_size = AR_BURST_SZ;
	// 		ar_req->burst_len = 7;

	// 		printf("Send aw_req: addr %lX cid %u qid %u arid %u burst_size %u burst_len %u\n",
	// 			ar_req->addr, ar_req->cid, ar_req->qid, ar_req->arid, ar_req->burst_size, ar_req->burst_len);
	// 	} else {
	// 		struct qdma_h2c_byp_out *byp_out = iov->iov_base;

	// 		byp_out->addr = byp_in->dsc.addr;
	// 		byp_out->len = 128;
	// 		// byp_out->func = byp_in->func;
	// 		byp_out->qid = byp_in->qid;
	// 		// byp_out->cidx = byp_in->cidx;
	// 		byp_out->port_id = spdk_get_ticks() % 8;
	// 		// byp_out->port_id = byp_in->port_id;
	// 		printf("Send byp: port_id %u\n", byp_out->port_id);
	// 	}
	// }

	// ctrl.tid = MCDMA_AW_BRIDGE_TX_TID;
	// spdk_axi_dma_tx_channel_send(hello_ch->byp_tx_ch, hello_ch->byp_tx_io->iovs, hello_ch->byp_tx_io->iovcnt, desc_byp_out_cmpl_cb, hello_ch->byp_tx_io, &ctrl);

	// ctrl.tid = 22 + byp_in->qid;
	// ctrl.tuser = BLK_SIZE;
	// spdk_axi_dma_tx_channel_send(hello_ch->data_tx_ch, hello_ch->data_tx_io->iovs, hello_ch->data_tx_io->iovcnt, tx_cmpl_cb, hello_ch->data_tx_io, &ctrl);

	spdk_axi_dma_rx_channel_recv(hello_ch->byp_rx_ch, io->iovs, io->iovcnt, desc_byp_in_cmpl_cb, cio);

	spdk_axi_dma_io_free(io);
}

static void desc_byp_in_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	if (io->ctrl.tid == MCDMA_QDMA_C2H_BYP_OUT_RX_TID) {
		c2h_desc_byp_in_cmpl_cb(io, status);
	} else if (io->ctrl.tid == MCDMA_QDMA_H2C_BYP_OUT_RX_TID) {
		h2c_desc_byp_in_cmpl_cb(io, status);
	} else {
		SPDK_ERRLOG("Invalid tid: %u\n", io->ctrl.tid);
	}
}

// static int stat_poller(void *ctx)
// {
// 	struct hello_context_t *hello_context = ctx;
// 	spdk_axi_dma_ch_get_stat(hello_context->ch.tx_ch, &hello_context->ch.stat);
// 	printf("Processed %u\n", hello_context->ch.stat.pkt_processed);
// }

static void rx_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;
	struct spdk_axi_dma_ch *ch = io->ch;

	// printf("%p\n", ch);
	
	hello_ch->rx_cnt += io->iovcnt;
	hello_ch->ctx->total_time += (io->end - io->start);

	uint64_t first = *(uint64_t *)io->iovs[0].iov_base;

	printf("Received idx %lu, port_id %u\n", first, io->ctrl.tuser);
	// // print_sqe(io->iovs[0].iov_base);

	// if (first > 0 && first != hello_ch->last_idx + 1) {
	// 	hello_ch->dropped += first - hello_ch->last_idx - 1;
	// 	// printf("Jump from %lu to %lu\n", hello_ch->last_idx, first);
	// }

	// hello_ch->last_idx = first;

	spdk_axi_dma_rx_channel_recv(ch, io->iovs, io->iovcnt, rx_cmpl_cb, cio);

	spdk_axi_dma_io_free(io);
}

static int byp_in_sub_poller_fn(void *arg)
{
	int bd_cnt = spdk_ring_count(byp_out_rx_ring);
	struct hello_channel *hello_ch = arg;
	struct spdk_axi_dma_ctrl ctrl;

	fprintf(stderr, "Has %d packets\n", bd_cnt);

	if (bd_cnt > 0 && !hello_ch->byp_tx_sending && !hello_ch->data_tx_sending) {
		struct spdk_axi_dma_iovec *iov = &hello_ch->byp_tx_io->iovs[0];
		struct qdma_ar_req *ar_req = iov->iov_base;
		struct qdma_c2h_byp_in *byp_in;
		spdk_ring_dequeue(byp_out_rx_ring, (void **)&byp_in, 1);

		ar_req->addr = byp_in->dsc.addr;
		ar_req->cid = 0;
		ar_req->qid = byp_in->qid + 1;
		ar_req->arid = byp_in->port_id;
		ar_req->burst_size = AR_BURST_SZ;
		ar_req->burst_len = 7;

		fprintf(stderr, "Send aw_req: addr %lX cid %u qid %u arid %u burst_size %u burst_len %u\n",
			ar_req->addr, ar_req->cid, ar_req->qid, ar_req->arid, ar_req->burst_size, ar_req->burst_len);

		hello_ch->byp_tx_sending = hello_ch->data_tx_sending = true;

		ctrl.tid = MCDMA_AW_BRIDGE_TX_TID;
		spdk_axi_dma_tx_channel_send(hello_ch->byp_tx_ch, hello_ch->byp_tx_io->iovs, hello_ch->byp_tx_io->iovcnt, desc_byp_out_cmpl_cb, hello_ch->byp_tx_io, &ctrl);

		ctrl.tid = 22 + byp_in->qid;
		ctrl.tuser = BLK_SIZE;
		spdk_axi_dma_tx_channel_send(hello_ch->data_tx_ch, hello_ch->data_tx_io->iovs, hello_ch->data_tx_io->iovcnt, tx_cmpl_cb, hello_ch->data_tx_io, &ctrl);

		free(byp_in);
	}
}

/*
 * Our initial event that kicks off everything from main().
 */
static void
hello_thread_start(void *arg1)
{
	uint32_t num_cores = spdk_env_get_core_count();
	uint32_t cur_core = spdk_env_get_current_core();
	struct hello_context_t *hello_context = &ctxs[0];
	struct hello_channel *ch = &hello_context->ch;

	hello_context->dev = devs[0];
	ch->ctx = hello_context;

	if (data_tx_tid != UINT32_MAX) {
		printf("Channel %u running as data TX\n", data_tx_tid);
		ch->data_tx_ch = spdk_axi_dma_create_tx_channel(hello_context->dev, g_num_bds, data_tx_tid % 4, data_tx_tid);

		if (!ch->data_tx_ch) {
			SPDK_ERRLOG("Failed to create channel %d on device %p\n", data_tx_tid, hello_context->dev);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}

		ch->data_tx_io = calloc(1, sizeof(struct channel_io));
		ch->data_tx_io->ch = ch;
		ch->data_tx_io->iovcnt = CONT_BD_NUM;
		ch->data_tx_io->iovs = calloc(CONT_BD_NUM, sizeof(struct spdk_axi_dma_iovec));
		for (int i = 0; i < CONT_BD_NUM; i++) {
			struct spdk_axi_dma_iovec *iov = &ch->data_tx_io->iovs[i];
			iov->iov_base = spdk_zmalloc(BLK_SIZE, PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			iov->iov_len = BLK_SIZE;
			
			// snprintf(iov->iov_base, BLK_SIZE, "%s", "Hello World!\n");
			uint8_t *p = iov->iov_base;
			for (int k = 0; k < BLK_SIZE; k++) {
				p[k] = k % UINT8_MAX;
			}
		}

		spdk_poller_register(spdk_axi_dma_poller, ch->data_tx_ch, 0);
	}

	if (data_rx_tid != UINT32_MAX) {
		printf("Channel %u running as data RX\n", data_rx_tid);
		ch->data_rx_ch = spdk_axi_dma_create_rx_channel(hello_context->dev, g_num_bds, data_rx_tid % 4, data_rx_tid);

		if (!ch->data_rx_ch) {
			SPDK_ERRLOG("Failed to create channel %d on device %p\n", data_rx_tid, hello_context->dev);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}

		ch->data_ios = spdk_zmalloc(g_qd * sizeof(struct channel_io), PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);

		printf("qd %u\n", g_qd);

		for (uint32_t i = 0; i < g_qd; i++) {
			struct channel_io *io = &ch->data_ios[i];
			io->ch = ch;
			io->iovcnt = g_burst_size;
			io->iovs = calloc(io->iovcnt, sizeof(struct spdk_axi_dma_iovec));

			for (int j = 0; j < io->iovcnt; j++) {
				io->iovs[j].iov_base = spdk_dma_zmalloc(BLK_SIZE, BUF_ALIGN, NULL);
				io->iovs[j].iov_len = BLK_SIZE;
				if (!io->iovs[j].iov_base) {
					SPDK_ERRLOG("Failed to allocate buffer %d\n", j);
					// spdk_put_io_channel(hello_context->bdev_io_channel);
					// spdk_bdev_close(hello_context->bdev_desc);
					spdk_app_stop(-1);
					return;
				}
			}

			spdk_axi_dma_rx_channel_recv(ch->data_rx_ch, io->iovs, io->iovcnt, rx_cmpl_cb, io);
		}

		spdk_poller_register(spdk_axi_dma_poller, ch->data_rx_ch, 0);
	}

	if (byp_tx_tid != UINT32_MAX) {
		printf("Channel %u running as byp TX\n", byp_tx_tid);
        ch->byp_tx_ch = spdk_axi_dma_create_tx_channel(hello_context->dev, g_num_bds, byp_tx_tid % 4, byp_tx_tid);

		if (!ch->byp_tx_ch) {
			SPDK_ERRLOG("Failed to create channel %d on device %p\n", byp_tx_tid, hello_context->dev);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}

		ch->byp_tx_io = calloc(1, sizeof(struct channel_io));
		ch->byp_tx_io->ch = ch;
		ch->byp_tx_io->iovcnt = CONT_BD_NUM;
		ch->byp_tx_io->iovs = calloc(CONT_BD_NUM, sizeof(struct spdk_axi_dma_iovec));
		for (int i = 0; i < CONT_BD_NUM; i++) {
			struct spdk_axi_dma_iovec *iov = &ch->byp_tx_io->iovs[i];
			if (g_use_ar_format) {
				iov->iov_base = spdk_zmalloc(sizeof(struct qdma_ar_req),
																PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
				iov->iov_len = 12;
			} else {
				iov->iov_base = spdk_zmalloc(sizeof(struct qdma_h2c_byp_out),
																PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
				iov->iov_len = sizeof(struct qdma_h2c_byp_out);
			}
		}

		spdk_poller_register(spdk_axi_dma_poller, ch->byp_tx_ch, 0);
	}

	if (byp_rx_tid != UINT32_MAX) {
		printf("Channel %u running as byp RX\n", byp_rx_tid);
        ch->byp_rx_ch = spdk_axi_dma_create_rx_channel(hello_context->dev, g_num_bds, byp_rx_tid % 4, byp_rx_tid);

		if (!ch->byp_rx_ch) {
			SPDK_ERRLOG("Failed to create channel %d on device %p\n", byp_rx_tid, hello_context->dev);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}

		ch->byp_ios = spdk_zmalloc(g_qd * sizeof(struct channel_io), PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);

		printf("qd %u\n", g_qd);

		for (uint32_t i = 0; i < g_qd; i++) {
			struct channel_io *io = &ch->byp_ios[i];
			io->ch = ch;
			io->iovcnt = g_burst_size;
			io->iovs = calloc(io->iovcnt, sizeof(struct spdk_axi_dma_iovec));

			for (int j = 0; j < io->iovcnt; j++) {
				io->iovs[j].iov_base = spdk_dma_zmalloc(BLK_SIZE, BUF_ALIGN, NULL);
				io->iovs[j].iov_len = BLK_SIZE;
				if (!io->iovs[j].iov_base) {
					SPDK_ERRLOG("Failed to allocate buffer %d\n", j);
					// spdk_put_io_channel(hello_context->bdev_io_channel);
					// spdk_bdev_close(hello_context->bdev_desc);
					spdk_app_stop(-1);
					return;
				}
			}

			spdk_axi_dma_rx_channel_recv(ch->byp_rx_ch, io->iovs, io->iovcnt, desc_byp_in_cmpl_cb, io);
		}

		spdk_poller_register(spdk_axi_dma_poller, ch->byp_rx_ch, 0);
	}

	spdk_poller_register(byp_in_sub_poller_fn, ch, 1000000);
}

// static int user_input_poller_fn(void *arg)
// {
// 	int n, bd_cnt;

// 	scanf("%d", &n);

// 	bd_cnt = spdk_ring_count(byp_out_rx_ring);

// 	fprintf(stderr, "Has %d packets, send %d\n", bd_cnt, n);
// 	if (n > bd_cnt) {
// 		SPDK_ERRLOG("Send more packets than available!\n");
// 	}


// }

static void user_input_thread_start(void *arg)
{

}

static void perf_start(void *arg)
{
	uint32_t i;
	uint64_t idx = 0;
	int rc;

	for (i = 0; i < 1; i++) {
		devs[i] = spdk_axi_dma_get_device(g_dma_dev_names[i]);
		if (!devs[i]) {
			SPDK_ERRLOG("Failed to open device %s\n", g_dma_dev_names[i]);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}
	}

	byp_out_rx_ring = spdk_ring_create(SPDK_RING_TYPE_SP_SC, 1024, SPDK_ENV_SOCKET_ID_ANY);
	byp_in_tx_ring = spdk_ring_create(SPDK_RING_TYPE_SP_SC, 1024, SPDK_ENV_SOCKET_ID_ANY);

	SPDK_ENV_FOREACH_CORE(i) {
		// hello_context = calloc(1, sizeof(struct hello_context_t));
		// hello_context->dev = dev;
		// hello_context->idx = idx;
		struct spdk_cpuset cpuset;
		spdk_cpuset_set_cpu(&cpuset, i, true);
		struct spdk_thread *trd = spdk_thread_create("perf worker", &cpuset);
		printf("call event on core %u\n", i);
		spdk_thread_send_msg(trd, hello_thread_start, NULL);
		// spdk_event_call(event);

		idx++;
	}
}

int main(int argc, char **argv)
{
	struct spdk_app_opts opts = {};
	int rc = 0;
	struct hello_context_t hello_context = {};

	/* Set default values in opts structure. */
	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "hello_bdev";

	/*
	 * Parse built-in SPDK command line parameters as well
	 * as our custom one(s).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "I:O:D:S:T:q:b:a", NULL, hello_bdev_parse_arg,
				      hello_bdev_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}

	if (g_qd % g_burst_size != 0) {
		SPDK_ERRLOG("QD %u is not a multiple of burst size %u!\n", g_qd, g_burst_size);
		exit(-1);
	}

	g_qd = g_qd / g_burst_size;

	/*
	 * spdk_app_start() will initialize the SPDK framework, call hello_start(),
	 * and then block until spdk_app_stop() is called (or if an initialization
	 * error occurs, spdk_app_start() will return with rc even without calling
	 * hello_start().
	 */
	rc = spdk_app_start(&opts, perf_start, &hello_context);
	if (rc) {
		SPDK_ERRLOG("ERROR starting application\n");
	}

	/* At this point either spdk_app_stop() was called, or spdk_app_start()
	 * failed because of internal error.
	 */

	/* When the app stops, free up memory that we allocated. */
	// for (int i = 0; i < g_num_channels; i++) {
	// 	spdk_dma_free(hello_context.bufs[i]);
	// }
	// spdk_dma_free(hello_context.rx_buf);

	/* Gracefully close out all of the SPDK subsystems. */
	spdk_app_fini();
	return rc;
}
