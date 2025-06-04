#include "spdk/axi_dma.h"

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_zone.h"

#define IOVCNT 1
#define BLK_SIZE 64
#define BUF_ALIGN 4096

enum q_dir {
	Q_DIR_C2H,
	Q_DIR_H2C,
	Q_DIR_BI
};

static const char *dir_names[] = {"C2H", "H2C", "BI"};

static char *g_dma_tx_name = NULL, *g_dma_rx_name = NULL;
// static char *g_dma_tx_name_1 = "b0010000.dma", *g_dma_tx_name_2 = "b0020000.dma";
// static char *g_dma_tx_name = NULL, *g_dma_rx_name = NULL;
static char *g_dev_name;
static enum q_dir g_dir;
static int g_num_channels = 1;
static uint32_t g_num_bds = 1024;
static uint32_t g_qd = 1;
static uint32_t g_burst_size = 1;

static uint32_t max_count = 128;

struct hello_context_t;
struct hello_channel;

struct channel_io {
	struct spdk_axi_dma_iovec *iovs;
	int iovcnt;
	struct hello_channel *ch;
};

struct hello_channel {
	struct spdk_axi_dma_ch *tx_ch;
	struct spdk_axi_dma_ch *rx_ch;
	struct channel_io *ios;
	uint64_t tx_cnt;
	uint64_t rx_cnt;
	uint64_t dropped;
	uint64_t last_idx;
	struct hello_context_t *ctx;
	struct spdk_axi_dma_ch_stat stat;
};

/*
 * We'll use this struct to gather housekeeping hello_context to pass between
 * our events and callbacks.
 */
struct hello_context_t {
	struct spdk_axi_dma_dev *dev;
	struct hello_channel *chs;
	uint64_t total_time;
	uint64_t idx;
	// uint64_t max_time;
	// uint64_t min_time;
	// uint64_t dma_total_time;
};

static inline const char *dir_name(enum q_dir dir)
{
	return dir_names[dir];
}

/*
 * This function is called to parse the parameters that are specific to this application
 */
static int hello_bdev_parse_arg(int ch, char *arg)
{
	// printf("ch is %c\n", (char)ch);
	switch ((char)ch) {
	case 'I':
		g_dir = Q_DIR_H2C;
		g_dma_rx_name = arg;
		break;
	case 'O':
		g_dir = Q_DIR_C2H;
		g_dma_tx_name = arg;
		break;
	case 'D':
		g_dir = Q_DIR_BI;
		g_dma_tx_name = g_dma_rx_name = arg;
		break;
	case 'C':
		g_num_channels = atoi(arg);
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

	// printf("cmpl: %p\n", io);
	
	hello_ch->tx_cnt += io->iovcnt;
	hello_ch->ctx->total_time += (io->end - io->start);

	// uint64_t first = *(uint64_t *)io->buf;

	// if (first > 0 && first != hello_ch->last_idx + 1) {
	// 	hello_ch->dropped += first - hello_ch->last_idx - 1;
	// 	// printf("Jump from %lu to %lu\n", hello_ch->last_idx, first);
	// }

	// hello_ch->last_idx = first;

	// if (hello_ch->tx_cnt < max_count) {
	if (true) {
		if (g_dir == Q_DIR_BI) {
			spdk_axi_dma_rx_channel_recv(hello_ch->rx_ch, io->iovs, io->iovcnt, rx_cmpl_cb, cio);
		} else {
			spdk_axi_dma_tx_channel_send(ch, io->iovs, io->iovcnt, tx_cmpl_cb, cio, &io->ctrl);
		}
	}

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

static void rx_cmpl_cb(struct spdk_axi_dma_io *io, int status)
{
	struct channel_io *cio = io->ctx;
	struct hello_channel *hello_ch = cio->ch;
	struct spdk_axi_dma_ch *ch = io->ch;

	// printf("%p\n", ch);
	
	hello_ch->rx_cnt += io->iovcnt;
	hello_ch->ctx->total_time += (io->end - io->start);

	uint64_t first = *(uint64_t *)io->iovs[0].iov_base;

	// printf("Received idx %lu\n", first);
	// print_sqe(io->iovs[0].iov_base);

	if (first > 0 && first != hello_ch->last_idx + 1) {
		hello_ch->dropped += first - hello_ch->last_idx - 1;
		// printf("Jump from %lu to %lu\n", hello_ch->last_idx, first);
	}

	hello_ch->last_idx = first;

	if (g_dir == Q_DIR_BI) {
		spdk_axi_dma_tx_channel_send(hello_ch->tx_ch, io->iovs, io->iovcnt, tx_cmpl_cb, cio, NULL);
	} else {
		spdk_axi_dma_rx_channel_recv(ch, io->iovs, io->iovcnt, rx_cmpl_cb, cio);
	}

	spdk_axi_dma_io_free(io);
}

static int stat_poller(void *ctx)
{
	struct hello_context_t *hello_context = ctx;
	double t = (double)spdk_get_ticks() / spdk_get_ticks_hz();
	// double avg_lat = (double)hello_context->total_time * 1000000 / spdk_get_ticks_hz() / hello_context->cnt;

	// printf("%lf: %s %lu, avg lat %lf us\n", t, g_tx ? "TX" : "RX", hello_context->cnt, avg_lat);
	printf("[%u] %lf: %s", spdk_env_get_current_core(), t, dir_name(g_dir));
	for (int i = 0; i < g_num_channels; i++) {
		if (g_dir == Q_DIR_C2H) {
			printf(" %lu", hello_context->chs[i].tx_cnt);
			hello_context->chs[i].tx_cnt = 0;
		} else if (g_dir == Q_DIR_H2C) {
			printf(" %lu", hello_context->chs[i].rx_cnt);
			hello_context->chs[i].rx_cnt = 0;
		} else {
			printf(" %lu-%lu", hello_context->chs[i].rx_cnt, hello_context->chs[i].tx_cnt);
			hello_context->chs[i].tx_cnt = hello_context->chs[i].rx_cnt = 0;
		}

		// spdk_axi_dma_ch_get_stat(hello_context->chs[i].ch, &hello_context->chs[i].stat);
	}

	if (g_dir != Q_DIR_C2H) {
		printf(", dropped");

		for (int i = 0; i < g_num_channels; i++) {
			spdk_axi_dma_ch_get_stat(hello_context->chs[i].rx_ch, &hello_context->chs[i].stat);
			printf(" %u", hello_context->chs[i].stat.pkt_dropped);
		}

		printf(", skipped");

		for (int i = 0; i < g_num_channels; i++) {
			double drop_ratio = (double)hello_context->chs[i].dropped / hello_context->chs[i].rx_cnt;
			printf(" %lu(%.3lf)", hello_context->chs[i].dropped, drop_ratio);
			// hello_context->chs[i].dropped = 0;
		}
	}

	printf("\n");
	// hello_context->cnt = 0;
	hello_context->total_time = 0;

	return 0;
}

/*
 * Our initial event that kicks off everything from main().
 */
static void
hello_thread_start(void *arg1)
{
	struct hello_context_t *hello_context = arg1;
	uintptr_t idx = hello_context->idx;
	struct spdk_axi_dma_ctrl ctrl;

	printf("Core %u thread %p working on q [%u, %u)\n", spdk_env_get_current_core(), spdk_get_thread(), idx, idx + g_num_channels);

	// struct spdk_axi_dma_dev *devs[3];

	// devs[0] = hello_context->dev;

	// struct spdk_axi_dma_dev *dev1 = spdk_axi_dma_get_device(g_dma_tx_name_1);
	// if (!dev1) {
	// 	SPDK_ERRLOG("Failed to open device %s\n", g_dma_tx_name_1);
	// 	// spdk_put_io_channel(hello_context->bdev_io_channel);
	// 	// spdk_bdev_close(hello_context->bdev_desc);
	// 	spdk_app_stop(-1);
	// 	return;
	// }
	// struct spdk_axi_dma_dev *dev2 = spdk_axi_dma_get_device(g_dma_tx_name_2);
	// if (!dev2) {
	// 	SPDK_ERRLOG("Failed to open device %s\n", g_dma_tx_name_2);
	// 	// spdk_put_io_channel(hello_context->bdev_io_channel);
	// 	// spdk_bdev_close(hello_context->bdev_desc);
	// 	spdk_app_stop(-1);
	// 	return;
	// }

	// devs[1] = dev1;
	// devs[2] = dev2;

    // hello_context->rx_dev = spdk_axi_dma_get_device(g_dma_rx_name);
	// if (!hello_context->rx_dev) {
	// 	SPDK_ERRLOG("Failed to open device %s\n", g_dma_rx_name);
	// 	// spdk_put_io_channel(hello_context->bdev_io_channel);
	// 	// spdk_bdev_close(hello_context->bdev_desc);
	// 	spdk_app_stop(-1);
	// 	return;
	// }

	hello_context->chs = spdk_zmalloc(g_num_channels * sizeof(struct hello_channel), PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);

	if (!hello_context->chs) {
		SPDK_ERRLOG("Failed to create channel array on device %s\n", g_dev_name);
		// spdk_put_io_channel(hello_context->bdev_io_channel);
		// spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	for (int i = 0; i < g_num_channels; i++) {
		struct hello_channel *ch = &hello_context->chs[i];
		ch->ctx = hello_context;
		if (g_dir == Q_DIR_C2H) {
			// ch->tx_ch = spdk_axi_dma_create_tx_channel(devs[0], g_num_bds, i + 1, i + 1);
			ch->tx_ch = spdk_axi_dma_create_tx_channel(hello_context->dev, g_num_bds, i + idx, 16 + idx + i);
		} else if (g_dir == Q_DIR_H2C) {
			ch->rx_ch = spdk_axi_dma_create_rx_channel(hello_context->dev, g_num_bds, i + idx, i + idx);
		} else {
			ch->tx_ch = spdk_axi_dma_create_tx_channel(hello_context->dev, g_num_bds, i, i + 1);
			ch->rx_ch = spdk_axi_dma_create_rx_channel(hello_context->dev, g_num_bds, i, i);
		}

		if (!ch->tx_ch && !ch->rx_ch) {
			SPDK_ERRLOG("Failed to create channel %d on device %p\n", i, hello_context->dev);
			// spdk_put_io_channel(hello_context->bdev_io_channel);
			// spdk_bdev_close(hello_context->bdev_desc);
			spdk_app_stop(-1);
			return;
		}

		ch->ios = spdk_zmalloc(g_qd * sizeof(struct channel_io), PAGE_SIZE, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_SHARE);

		printf("qd %u\n", g_qd);

		for (uint32_t i = 0; i < g_qd; i++) {
			struct channel_io *io = &ch->ios[i];
			io->ch = ch;
			io->iovcnt = g_burst_size;
			io->iovs = calloc(io->iovcnt, sizeof(struct spdk_axi_dma_iovec));

			ctrl.tid = 16 + idx + i;

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

			// snprintf(io->iovs[0].iov_base, BLK_SIZE, "%s", "Hello World!\n");
			// uint8_t *p = io->iovs[0].iov_base;
			// for (int k = 0; k < BLK_SIZE; k++) {
			// 	p[k] = k % UINT8_MAX;
			// }

			if (g_dir == Q_DIR_C2H) {
				// printf("data to be sent is :%s\n", (char *)io->iovs[0].iov_base);
				// printf("data to be sent is :%d\n", io->iovcnt);
				// print_sqe(io->iovs[0].iov_base);
				spdk_axi_dma_tx_channel_send(ch->tx_ch, io->iovs, io->iovcnt, tx_cmpl_cb, io, &ctrl);
			} else {
				spdk_axi_dma_rx_channel_recv(ch->rx_ch, io->iovs, io->iovcnt, rx_cmpl_cb, io);
			}
		}

		// exit(-1);

		if (g_dir == Q_DIR_C2H) {
			spdk_poller_register(spdk_axi_dma_poller, ch->tx_ch, 0);
		} else if (g_dir == Q_DIR_H2C) {
			spdk_poller_register(spdk_axi_dma_poller, ch->rx_ch, 0);
		} else {
			spdk_poller_register(spdk_axi_dma_poller, ch->tx_ch, 0);
			spdk_poller_register(spdk_axi_dma_poller, ch->rx_ch, 0);
		}
	}

	if (!spdk_poller_register(stat_poller, hello_context, 1000 * 1000)) {
		SPDK_ERRLOG("Failed to register stat poller on context %p on thread %p\n", hello_context, spdk_get_thread());
	}
}

static void perf_start(void *arg)
{
	struct hello_context_t *hello_context;
	uint32_t i, current_core;
	uint64_t idx = 0;
	int rc;
	struct spdk_axi_dma_dev *dev = spdk_axi_dma_get_device(g_dev_name);
	if (!dev) {
		SPDK_ERRLOG("Failed to open device %s\n", g_dev_name);
		// spdk_put_io_channel(hello_context->bdev_io_channel);
		// spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	current_core = spdk_env_get_current_core();
	SPDK_ENV_FOREACH_CORE(i) {
		hello_context = calloc(1, sizeof(struct hello_context_t));
		hello_context->dev = dev;
		hello_context->idx = idx;
		struct spdk_cpuset cpuset;
		spdk_cpuset_set_cpu(&cpuset, i, true);
		struct spdk_thread *trd = spdk_thread_create("perf worker", &cpuset);
		printf("call event on core %u\n", i);
		spdk_thread_send_msg(trd, hello_thread_start, hello_context);
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
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "I:O:D:C:S:q:b:", NULL, hello_bdev_parse_arg,
				      hello_bdev_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}

	if (g_dir == Q_DIR_C2H) {
		printf("Running in TX mode\n");
		g_dev_name = g_dma_tx_name;
	} else if (g_dir == Q_DIR_H2C) {
		printf("Running in RX mode\n");
		g_dev_name = g_dma_rx_name;
	} else if (g_dir == Q_DIR_BI) {
		printf("Running in BI mode\n");
		g_dev_name = g_dma_tx_name;
	} else {
		printf("Neither -I, -O nor -D specified\n");
		exit(-1);
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
    return 0;
}
