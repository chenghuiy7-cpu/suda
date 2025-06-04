#include "spdk/compute.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk_internal/event.h"

static struct spdk_compute_qpair *g_qpairs[SPDK_COMPUTE_NUM_RINGS];
uint16_t cnt[SPDK_COMPUTE_NUM_RINGS] = {0};

static int cq_poll(void *arg) {
    struct spdk_compute_qpair *qpair = arg;
    return spdk_compute_poll_cq(qpair);
}

static void submit_sqe(struct spdk_compute_qpair *qpair);

static void compute_cmpl(struct spdk_compute_sqe *sqe, uint32_t status, void *cb_arg) {
    printf("Compute SQE %u-%u completed with status %d\n", sqe->pf_id, sqe->csqe_id, status);
    uint16_t ring_id = sqe->pf_id - 2;
    if (++cnt[ring_id] < 12) {
        submit_sqe(g_qpairs[ring_id]);
    }
}

static void submit_sqe(struct spdk_compute_qpair *qpair) {
    struct spdk_compute_sqe *sqe = spdk_compute_get_sqe(qpair);
    sqe->pf_id = 2 + qpair->ring_id;
    
    spdk_compute_submit_sqe(qpair, sqe, compute_cmpl, NULL);
}

static void compute_start(void *arg) {
    struct spdk_compute_dev *dev = calloc(1, sizeof(struct spdk_compute_dev));
    int ret;

    ret = spdk_compute_dev_init(dev, SPDK_COMPUTE_BAR_PHYS_ADDR);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize compute device\n");
        return;
    }

    for (int i = 0; i < SPDK_COMPUTE_NUM_RINGS; i++) {
        g_qpairs[i] = calloc(1, sizeof(struct spdk_compute_qpair));
        ret = spdk_compute_qpair_create(dev, g_qpairs[i], i);
        if (ret < 0) {
            fprintf(stderr, "Failed to create compute qpair\n");
            return;
        }

        spdk_poller_register(cq_poll, g_qpairs[i], 0);

        submit_sqe(g_qpairs[i]);
    }
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
	rc = spdk_app_start(&opts, compute_start, NULL);
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
