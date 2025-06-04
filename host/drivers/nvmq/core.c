// SPDX-License-Identifier: GPL-2.0
/*
 * NVM Express device driver
 * Copyright (c) 2011-2014, Intel Corporation.
 */

#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/backing-dev.h>
#include <linux/list_sort.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pr.h>
#include <linux/ptrace.h>
#include <linux/nvme_ioctl.h>
#include <linux/t10-pi.h>
#include <linux/pm_qos.h>
#include <asm/unaligned.h>


#include "nvme.h"
#include "fabrics.h"

#define CREATE_TRACE_POINTS
#include "trace.h"

#define NVME_MINORS		(1U << MINORBITS)

unsigned int admin_timeout = 60;
// module_param(admin_timeout, uint, 0644);
// MODULE_PARM_DESC(admin_timeout, "timeout in seconds for admin commands");
// EXPORT_SYMBOL_GPL(admin_timeout);

unsigned int nvmq_io_timeout = 30;
// module_param_named(io_timeout, nvmq_io_timeout, uint, 0644);
// MODULE_PARM_DESC(io_timeout, "timeout in seconds for I/O");
// EXPORT_SYMBOL_GPL(nvmq_io_timeout);

static unsigned char shutdown_timeout = 5;
// module_param(shutdown_timeout, byte, 0644);
// MODULE_PARM_DESC(shutdown_timeout, "timeout in seconds for controller shutdown");

static u8 nvmq_max_retries = 5;
// module_param_named(max_retries, nvmq_max_retries, byte, 0644);
// MODULE_PARM_DESC(max_retries, "max number of retries a command may have");

static unsigned long default_ps_max_latency_us = 100000;
// module_param(default_ps_max_latency_us, ulong, 0644);
// MODULE_PARM_DESC(default_ps_max_latency_us,
// 		 "max power saving latency for new devices; use PM QOS to change per device");

static bool force_apst;
// module_param(force_apst, bool, 0644);
// MODULE_PARM_DESC(force_apst, "allow APST for newly enumerated devices even if quirked off");

static bool streams;
// module_param(streams, bool, 0644);
// MODULE_PARM_DESC(streams, "turn on support for Streams write directives");

/*
 * nvmq_wq - hosts nvmq related works that are not reset or delete
 * nvmq_reset_wq - hosts nvmq reset works
 * nvmq_delete_wq - hosts nvmq delete works
 *
 * nvmq_wq will host works such as scan, aen handling, fw activation,
 * keep-alive, periodic reconnects etc. nvmq_reset_wq
 * runs reset works which also flush works hosted on nvmq_wq for
 * serialization purposes. nvmq_delete_wq host controller deletion
 * works which flush reset works for serialization.
 */
struct workqueue_struct *nvmq_wq;
EXPORT_SYMBOL_GPL(nvmq_wq);

struct workqueue_struct *nvmq_reset_wq;
EXPORT_SYMBOL_GPL(nvmq_reset_wq);

struct workqueue_struct *nvmq_delete_wq;
EXPORT_SYMBOL_GPL(nvmq_delete_wq);

static LIST_HEAD(nvmq_subsystems);
static DEFINE_MUTEX(nvmq_subsystems_lock);

static DEFINE_IDA(nvmq_instance_ida);
static dev_t nvmq_chr_devt;
static struct class *nvmq_class;
static struct class *nvmq_subsys_class;

static DEFINE_IDA(nvmq_ns_chr_minor_ida);
static dev_t nvmq_ns_chr_devt;
static struct class *nvmq_ns_chr_class;

static int nvmq_revalidate_disk(struct gendisk *disk);
static void nvmq_put_subsystem(struct nvmq_subsystem *subsys);
static void nvmq_remove_invalid_namespaces(struct nvmq_ctrl *ctrl,
					   unsigned nsid);

/*
 * 结构体 async_aio_task 用于保存一次异步操作的信息，
 * 包括传入的 kiocb、用户空间缓冲区地址、数据长度以及操作类型。
 */
struct async_ns_chr_task {
	struct work_struct work;
	struct kiocb *iocb;
	bool is_read;         /* true: read, false: write */
	size_t count;         /* 待传输的数据字节数 */
	/* 假定仅有一个 iovec，用户空间地址 */
	char __user *user_buf;
	char *kernel_buf;
	struct page** buf_pages;
	struct nvmq_ns* ns;
	bool write;
	struct request *req;
	struct bio *bio;
	void *meta;
	struct nvme_command c;
};

static inline void nvmq_blk_rq_bio_prep(struct request *rq, struct bio *bio,
	unsigned int nr_segs)
{
	rq->nr_phys_segments = nr_segs;
	rq->__data_len = bio->bi_iter.bi_size;
	rq->bio = rq->biotail = bio;
	rq->ioprio = bio_prio(bio);

	if (bio->bi_disk)
		rq->rq_disk = bio->bi_disk;
}

static int nvmq_blk_rq_append_bio(struct request *rq, struct bio **bio)
{
	struct bio *orig_bio = *bio;
	struct bvec_iter iter;
	struct bio_vec bv;
	unsigned int nr_segs = 0;

	//blk_queue_bounce(rq->q, bio);

	bio_for_each_bvec(bv, *bio, iter)
		nr_segs++;

	if (!rq->bio) {
		pr_debug("nr segs%d\n",nr_segs);
		nvmq_blk_rq_bio_prep(rq, *bio, nr_segs);
	} else {
		rq->nr_phys_segments += nr_segs;
		rq->biotail->bi_next = *bio;
		rq->biotail = *bio;
		rq->__data_len += (*bio)->bi_iter.bi_size;
	}

	return 0;
}


static int __nvmq_blk_rq_map_user_iov(struct request *rq,
	struct rq_map_data *map_data, struct iov_iter *iter,
	gfp_t gfp_mask, bool copy)
{
	struct request_queue *q = rq->q;
	struct bio *bio, *orig_bio;
	int ret;

	if (!copy){
		bio = bio_map_user_iov(q, iter, gfp_mask);
		pr_debug("BIO ADDRESS%llx\n",bio);
	}
	else{
		pr_err("Does not support copy\n");
		bio=NULL;
		return -ENOMEM;
	}

	if (IS_ERR(bio))
		return PTR_ERR(bio);

	bio->bi_opf &= ~REQ_OP_MASK;
	bio->bi_opf |= req_op(rq);

	orig_bio = bio;

	//this function will map data bigger than block devices's max sectors 
	//so must only use for slm_read / slm_write
	ret = nvmq_blk_rq_append_bio(rq, &bio);
	if (ret) {
		pr_err("MP FAILED TO MAP DATA\n");
		if (orig_bio) {
			if (bio_flagged(orig_bio, BIO_USER_MAPPED))
				bio_unmap_user(orig_bio);
			else
				ret = bio_uncopy_user(orig_bio);
		}
		return ret;
	}
	bio_get(bio);

	return 0;
}

//Map Data Large than MAX Sectors
int nvmq_blk_rq_map_user_iov(struct request_queue *q, struct request *rq,
	struct rq_map_data *map_data,
	const struct iov_iter *iter, gfp_t gfp_mask)
{
	bool copy = false;
	unsigned long align = q->dma_pad_mask | queue_dma_alignment(q);
	struct bio *bio = NULL;
	struct iov_iter i;
	int ret = -EINVAL;

	if (!iter_is_iovec(iter))
	goto fail;

	if (map_data)
		copy = true;
	else if (iov_iter_alignment(iter) & align)
		copy = true;
	else if (queue_virt_boundary(q))
		copy = queue_virt_boundary(q) & iov_iter_gap_alignment(iter);

	i = *iter;
	do {
		ret =__nvmq_blk_rq_map_user_iov(rq, map_data, &i, gfp_mask, copy);
	if (ret)
		goto unmap_rq;
	if (!bio)
		bio = rq->bio;
	} while (iov_iter_count(&i));

	if (!bio_flagged(bio, BIO_USER_MAPPED))
	rq->rq_flags |= RQF_COPY_USER;
	return 0;

	unmap_rq:
	blk_rq_unmap_user(bio);
	fail:
	rq->bio = NULL;
	return ret;
}

int nvmq_blk_rq_map_user(struct request_queue *q, struct request *rq,
	struct rq_map_data *map_data, void __user *ubuf,
	unsigned long len, gfp_t gfp_mask)
{
	struct iovec iov;
	struct iov_iter i;
	int ret = import_single_range(rq_data_dir(rq), ubuf, len, &iov, &i);

	if (unlikely(ret < 0))
	return ret;

	return nvmq_blk_rq_map_user_iov(q, rq, map_data, &i, gfp_mask);
}




static void nvmq_set_queue_dying(struct nvmq_ns *ns)
{
	/*
	 * Revalidating a dead namespace sets capacity to 0. This will end
	 * buffered writers dirtying pages that can't be synced.
	 */
	if (!ns->disk || test_and_set_bit(NVME_NS_DEAD, &ns->flags))
		return;
	blk_set_queue_dying(ns->queue);
	/* Forcibly unquiesce queues to avoid blocking dispatch */
	blk_mq_unquiesce_queue(ns->queue);
	/*
	 * Revalidate after unblocking dispatchers that may be holding bd_butex
	 */
	revalidate_disk(ns->disk);
}

static void nvmq_queue_scan(struct nvmq_ctrl *ctrl)
{
	/*
	 * Only new queue scan work when admin and IO queues are both alive
	 */
	if (ctrl->state == NVME_CTRL_LIVE && ctrl->tagset)
		queue_work(nvmq_wq, &ctrl->scan_work);
}

/*
 * Use this function to proceed with scheduling reset_work for a controller
 * that had previously been set to the resetting state. This is intended for
 * code paths that can't be interrupted by other reset attempts. A hot removal
 * may prevent this from succeeding.
 */
int nvmq_try_sched_reset(struct nvmq_ctrl *ctrl)
{
	if (ctrl->state != NVME_CTRL_RESETTING)
		return -EBUSY;
	if (!queue_work(nvmq_reset_wq, &ctrl->reset_work))
		return -EBUSY;
	return 0;
}
EXPORT_SYMBOL_GPL(nvmq_try_sched_reset);

int nvmq_reset_ctrl(struct nvmq_ctrl *ctrl)
{
	if (!nvmq_change_ctrl_state(ctrl, NVME_CTRL_RESETTING))
		return -EBUSY;
	if (!queue_work(nvmq_reset_wq, &ctrl->reset_work))
		return -EBUSY;
	return 0;
}
EXPORT_SYMBOL_GPL(nvmq_reset_ctrl);

int nvmq_reset_ctrl_sync(struct nvmq_ctrl *ctrl)
{
	int ret;

	ret = nvmq_reset_ctrl(ctrl);
	if (!ret) {
		flush_work(&ctrl->reset_work);
		if (ctrl->state != NVME_CTRL_LIVE)
			ret = -ENETRESET;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(nvmq_reset_ctrl_sync);

static void nvmq_do_delete_ctrl(struct nvmq_ctrl *ctrl)
{
	dev_info(ctrl->device,
		 "Removing ctrl: NQN \"%s\"\n", ctrl->opts->subsysnqn);

	flush_work(&ctrl->reset_work);
	nvmq_stop_ctrl(ctrl);
	nvmq_remove_namespaces(ctrl);
	ctrl->ops->delete_ctrl(ctrl);
	nvmq_uninit_ctrl(ctrl);
	nvmq_put_ctrl(ctrl);
}

static void nvmq_delete_ctrl_work(struct work_struct *work)
{
	struct nvmq_ctrl *ctrl =
		container_of(work, struct nvmq_ctrl, delete_work);

	nvmq_do_delete_ctrl(ctrl);
}

int nvmq_delete_ctrl(struct nvmq_ctrl *ctrl)
{
	if (!nvmq_change_ctrl_state(ctrl, NVME_CTRL_DELETING))
		return -EBUSY;
	if (!queue_work(nvmq_delete_wq, &ctrl->delete_work))
		return -EBUSY;
	return 0;
}
EXPORT_SYMBOL_GPL(nvmq_delete_ctrl);

static int nvmq_delete_ctrl_sync(struct nvmq_ctrl *ctrl)
{
	int ret = 0;

	/*
	 * Keep a reference until nvme_do_delete_ctrl() complete,
	 * since ->delete_ctrl can free the controller.
	 */
	nvmq_get_ctrl(ctrl);
	if (!nvmq_change_ctrl_state(ctrl, NVME_CTRL_DELETING))
		ret = -EBUSY;
	if (!ret)
		nvmq_do_delete_ctrl(ctrl);
	nvmq_put_ctrl(ctrl);
	return ret;
}

static inline bool nvmq_ns_has_pi(struct nvmq_ns *ns)
{
	return ns->pi_type && ns->ms == sizeof(struct t10_pi_tuple);
}

static blk_status_t nvmq_error_status(u16 status)
{
	switch (status & 0x7ff) {
	case NVME_SC_SUCCESS:
		return BLK_STS_OK;
	case NVME_SC_CAP_EXCEEDED:
		return BLK_STS_NOSPC;
	case NVME_SC_LBA_RANGE:
		return BLK_STS_TARGET;
	case NVME_SC_BAD_ATTRIBUTES:
	case NVME_SC_ONCS_NOT_SUPPORTED:
	case NVME_SC_INVALID_OPCODE:
	case NVME_SC_INVALID_FIELD:
	case NVME_SC_INVALID_NS:
		return BLK_STS_NOTSUPP;
	case NVME_SC_WRITE_FAULT:
	case NVME_SC_READ_ERROR:
	case NVME_SC_UNWRITTEN_BLOCK:
	case NVME_SC_ACCESS_DENIED:
	case NVME_SC_READ_ONLY:
	case NVME_SC_COMPARE_FAILED:
		return BLK_STS_MEDIUM;
	case NVME_SC_GUARD_CHECK:
	case NVME_SC_APPTAG_CHECK:
	case NVME_SC_REFTAG_CHECK:
	case NVME_SC_INVALID_PI:
		return BLK_STS_PROTECTION;
	case NVME_SC_RESERVATION_CONFLICT:
		return BLK_STS_NEXUS;
	case NVME_SC_HOST_PATH_ERROR:
		return BLK_STS_TRANSPORT;
	default:
		return BLK_STS_IOERR;
	}
}

static inline bool nvmq_req_needs_retry(struct request *req)
{
	if (blk_noretry_request(req))
		return false;
	if (nvmq_req(req)->status & NVME_SC_DNR)
		return false;
	if (nvmq_req(req)->retries >= nvmq_max_retries)
		return false;
	return true;
}

static void nvmq_retry_req(struct request *req)
{
	struct nvmq_ns *ns = req->q->queuedata;
	unsigned long delay = 0;
	u16 crd;

	/* The mask and shift result must be <= 3 */
	crd = (nvmq_req(req)->status & NVME_SC_CRD) >> 11;
	if (ns && crd)
		delay = ns->ctrl->crdt[crd - 1] * 100;

	nvmq_req(req)->retries++;
	blk_mq_requeue_request(req, false);
	blk_mq_delay_kick_requeue_list(req->q, delay);
}

void nvmq_complete_rq(struct request *req)
{
	blk_status_t status = nvmq_error_status(nvmq_req(req)->status);

	trace_nvmq_complete_rq(req);

	if (nvmq_req(req)->ctrl->kas)
		nvmq_req(req)->ctrl->comp_seen = true;

	if (unlikely(status != BLK_STS_OK && nvmq_req_needs_retry(req))) {
		if ((req->cmd_flags & REQ_NVME_MPATH) && nvmq_failover_req(req))
			return;

		if (!blk_queue_dying(req->q)) {
			nvmq_retry_req(req);
			return;
		}
	}

	nvmq_trace_bio_complete(req, status);
	blk_mq_end_request(req, status);
}
EXPORT_SYMBOL_GPL(nvmq_complete_rq);

bool nvmq_cancel_request(struct request *req, void *data, bool reserved)
{
	dev_dbg_ratelimited(((struct nvmq_ctrl *) data)->device,
				"Cancelling I/O %d", req->tag);

	/* don't abort one completed request */
	if (blk_mq_request_completed(req))
		return true;

	nvmq_req(req)->status = NVME_SC_HOST_ABORTED_CMD;
	nvmq_req(req)->flags |= NVME_REQ_CANCELLED;
	blk_mq_complete_request(req);
	return true;
}
EXPORT_SYMBOL_GPL(nvmq_cancel_request);

void nvmq_cancel_tagset(struct nvmq_ctrl *ctrl)
{
	if (ctrl->tagset) {
		blk_mq_tagset_busy_iter(ctrl->tagset,
				nvmq_cancel_request, ctrl);
		blk_mq_tagset_wait_completed_request(ctrl->tagset);
	}
}
EXPORT_SYMBOL_GPL(nvmq_cancel_tagset);

void nvmq_cancel_admin_tagset(struct nvmq_ctrl *ctrl)
{
	if (ctrl->admin_tagset) {
		blk_mq_tagset_busy_iter(ctrl->admin_tagset,
				nvmq_cancel_request, ctrl);
		blk_mq_tagset_wait_completed_request(ctrl->admin_tagset);
	}
}
EXPORT_SYMBOL_GPL(nvmq_cancel_admin_tagset);

bool nvmq_change_ctrl_state(struct nvmq_ctrl *ctrl,
		enum nvmq_ctrl_state new_state)
{
	enum nvmq_ctrl_state old_state;
	unsigned long flags;
	bool changed = false;

	spin_lock_irqsave(&ctrl->lock, flags);

	old_state = ctrl->state;
	switch (new_state) {
	case NVME_CTRL_LIVE:
		switch (old_state) {
		case NVME_CTRL_NEW:
		case NVME_CTRL_RESETTING:
		case NVME_CTRL_CONNECTING:
			changed = true;
			/* FALLTHRU */
		default:
			break;
		}
		break;
	case NVME_CTRL_RESETTING:
		switch (old_state) {
		case NVME_CTRL_NEW:
		case NVME_CTRL_LIVE:
			changed = true;
			/* FALLTHRU */
		default:
			break;
		}
		break;
	case NVME_CTRL_CONNECTING:
		switch (old_state) {
		case NVME_CTRL_NEW:
		case NVME_CTRL_RESETTING:
			changed = true;
			/* FALLTHRU */
		default:
			break;
		}
		break;
	case NVME_CTRL_DELETING:
		switch (old_state) {
		case NVME_CTRL_LIVE:
		case NVME_CTRL_RESETTING:
		case NVME_CTRL_CONNECTING:
			changed = true;
			/* FALLTHRU */
		default:
			break;
		}
		break;
	case NVME_CTRL_DEAD:
		switch (old_state) {
		case NVME_CTRL_DELETING:
			changed = true;
			/* FALLTHRU */
		default:
			break;
		}
		break;
	default:
		break;
	}

	if (changed) {
		ctrl->state = new_state;
		wake_up_all(&ctrl->state_wq);
	}

	spin_unlock_irqrestore(&ctrl->lock, flags);
	if (changed && ctrl->state == NVME_CTRL_LIVE)
		nvmq_kick_requeue_lists(ctrl);
	return changed;
}
EXPORT_SYMBOL_GPL(nvmq_change_ctrl_state);

/*
 * Returns true for sink states that can't ever transition back to live.
 */
static bool nvmq_state_terminal(struct nvmq_ctrl *ctrl)
{
	switch (ctrl->state) {
	case NVME_CTRL_NEW:
	case NVME_CTRL_LIVE:
	case NVME_CTRL_RESETTING:
	case NVME_CTRL_CONNECTING:
		return false;
	case NVME_CTRL_DELETING:
	case NVME_CTRL_DEAD:
		return true;
	default:
		WARN_ONCE(1, "Unhandled ctrl state:%d", ctrl->state);
		return true;
	}
}

/*
 * Waits for the controller state to be resetting, or returns false if it is
 * not possible to ever transition to that state.
 */
bool nvmq_wait_reset(struct nvmq_ctrl *ctrl)
{
	wait_event(ctrl->state_wq,
		   nvmq_change_ctrl_state(ctrl, NVME_CTRL_RESETTING) ||
		   nvmq_state_terminal(ctrl));
	return ctrl->state == NVME_CTRL_RESETTING;
}
EXPORT_SYMBOL_GPL(nvmq_wait_reset);

static void nvmq_free_ns_head(struct kref *ref)
{
	struct nvmq_ns_head *head =
		container_of(ref, struct nvmq_ns_head, ref);

	nvmq_mpath_remove_disk(head);
	ida_simple_remove(&head->subsys->ns_ida, head->instance);
	cleanup_srcu_struct(&head->srcu);
	nvmq_put_subsystem(head->subsys);
	kfree(head);
}

static void nvmq_put_ns_head(struct nvmq_ns_head *head)
{
	kref_put(&head->ref, nvmq_free_ns_head);
}

static void nvmq_free_ns(struct kref *kref)
{
	struct nvmq_ns *ns = container_of(kref, struct nvmq_ns, kref);

	if (ns->ndev)
		nvmq_nvm_unregister(ns);

	put_disk(ns->disk);
	nvmq_put_ns_head(ns->head);
	nvmq_put_ctrl(ns->ctrl);
	kfree(ns);
}

static void nvmq_put_ns(struct nvmq_ns *ns)
{
	kref_put(&ns->kref, nvmq_free_ns);
}

static inline void nvmq_clear_nvmq_request(struct request *req)
{
	if (!(req->rq_flags & RQF_DONTPREP)) {
		nvmq_req(req)->retries = 0;
		nvmq_req(req)->flags = 0;
		req->rq_flags |= RQF_DONTPREP;
	}
}

static struct request *nvmq_alloc_kernel(struct request_queue *q,
		blk_mq_req_flags_t flags, int qid)
{
	unsigned op = REQ_OP_DRV_IN;
	struct request *req;

	if (qid == NVME_QID_ANY) {
		req = blk_mq_alloc_request(q, op, flags);
	} else {
		req = blk_mq_alloc_request_hctx(q, op, flags,
				qid ? qid - 1 : 0);
	}
	if (IS_ERR(req))
		return req;

	req->cmd_flags |= REQ_FAILFAST_DRIVER;
	nvmq_clear_nvmq_request(req);

	return req;
}

struct request *nvmq_alloc_request(struct request_queue *q,
		struct nvme_command *cmd, blk_mq_req_flags_t flags, int qid)
{
	unsigned op = nvme_is_write(cmd) ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN;
	struct request *req;

	if (qid == NVME_QID_ANY) {
		req = blk_mq_alloc_request(q, op, flags);
	} else {
		req = blk_mq_alloc_request_hctx(q, op, flags,
				qid ? qid - 1 : 0);
	}
	if (IS_ERR(req))
		return req;

	req->cmd_flags |= REQ_FAILFAST_DRIVER;
	nvmq_clear_nvmq_request(req);
	nvmq_req(req)->cmd = cmd;

	return req;
}
EXPORT_SYMBOL_GPL(nvmq_alloc_request);

static int nvmq_toggle_streams(struct nvmq_ctrl *ctrl, bool enable)
{
	struct nvme_command c;

	memset(&c, 0, sizeof(c));

	c.directive.opcode = nvme_admin_directive_send;
	c.directive.nsid = cpu_to_le32(NVME_NSID_ALL);
	c.directive.doper = NVME_DIR_SND_ID_OP_ENABLE;
	c.directive.dtype = NVME_DIR_IDENTIFY;
	c.directive.tdtype = NVME_DIR_STREAMS;
	c.directive.endir = enable ? NVME_DIR_ENDIR : 0;

	return nvmq_submit_sync_cmd(ctrl->admin_q, &c, NULL, 0);
}

static int nvmq_disable_streams(struct nvmq_ctrl *ctrl)
{
	return nvmq_toggle_streams(ctrl, false);
}

static int nvmq_enable_streams(struct nvmq_ctrl *ctrl)
{
	return nvmq_toggle_streams(ctrl, true);
}

static int nvmq_get_stream_params(struct nvmq_ctrl *ctrl,
				  struct streams_directive_params *s, u32 nsid)
{
	struct nvme_command c;

	memset(&c, 0, sizeof(c));
	memset(s, 0, sizeof(*s));

	c.directive.opcode = nvme_admin_directive_recv;
	c.directive.nsid = cpu_to_le32(nsid);
	c.directive.numd = cpu_to_le32((sizeof(*s) >> 2) - 1);
	c.directive.doper = NVME_DIR_RCV_ST_OP_PARAM;
	c.directive.dtype = NVME_DIR_STREAMS;

	return nvmq_submit_sync_cmd(ctrl->admin_q, &c, s, sizeof(*s));
}

static int nvmq_configure_directives(struct nvmq_ctrl *ctrl)
{
	struct streams_directive_params s;
	int ret;

	if (!(ctrl->oacs & NVME_CTRL_OACS_DIRECTIVES))
		return 0;
	if (!streams)
		return 0;

	ret = nvmq_enable_streams(ctrl);
	if (ret)
		return ret;

	ret = nvmq_get_stream_params(ctrl, &s, NVME_NSID_ALL);
	if (ret)
		return ret;

	ctrl->nssa = le16_to_cpu(s.nssa);
	if (ctrl->nssa < BLK_MAX_WRITE_HINTS - 1) {
		dev_info(ctrl->device, "too few streams (%u) available\n",
					ctrl->nssa);
		nvmq_disable_streams(ctrl);
		return 0;
	}

	ctrl->nr_streams = min_t(unsigned, ctrl->nssa, BLK_MAX_WRITE_HINTS - 1);
	dev_info(ctrl->device, "Using %u streams\n", ctrl->nr_streams);
	return 0;
}

/*
 * Check if 'req' has a write hint associated with it. If it does, assign
 * a valid namespace stream to the write.
 */
static void nvmq_assign_write_stream(struct nvmq_ctrl *ctrl,
				     struct request *req, u16 *control,
				     u32 *dsmgmt)
{
	enum rw_hint streamid = req->write_hint;

	if (streamid == WRITE_LIFE_NOT_SET || streamid == WRITE_LIFE_NONE)
		streamid = 0;
	else {
		streamid--;
		if (WARN_ON_ONCE(streamid > ctrl->nr_streams))
			return;

		*control |= NVME_RW_DTYPE_STREAMS;
		*dsmgmt |= streamid << 16;
	}

	if (streamid < ARRAY_SIZE(req->q->write_hints))
		req->q->write_hints[streamid] += blk_rq_bytes(req) >> 9;
}

static inline void nvmq_setup_flush(struct nvmq_ns *ns,
		struct nvme_command *cmnd)
{
	cmnd->common.opcode = nvme_cmd_flush;
	cmnd->common.nsid = cpu_to_le32(ns->head->ns_id);
}

static blk_status_t nvmq_setup_discard(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *cmnd)
{
	unsigned short segments = blk_rq_nr_discard_segments(req), n = 0;
	struct nvme_dsm_range *range;
	struct bio *bio;

	/*
	 * Some devices do not consider the DSM 'Number of Ranges' field when
	 * determining how much data to DMA. Always allocate memory for maximum
	 * number of segments to prevent device reading beyond end of buffer.
	 */
	static const size_t alloc_size = sizeof(*range) * NVME_DSM_MAX_RANGES;

	range = kzalloc(alloc_size, GFP_ATOMIC | __GFP_NOWARN);
	if (!range) {
		/*
		 * If we fail allocation our range, fallback to the controller
		 * discard page. If that's also busy, it's safe to return
		 * busy, as we know we can make progress once that's freed.
		 */
		if (test_and_set_bit_lock(0, &ns->ctrl->discard_page_busy))
			return BLK_STS_RESOURCE;

		range = page_address(ns->ctrl->discard_page);
	}

	__rq_for_each_bio(bio, req) {
		u64 slba = nvmq_sect_to_lba(ns, bio->bi_iter.bi_sector);
		u32 nlb = bio->bi_iter.bi_size >> ns->lba_shift;

		if (n < segments) {
			range[n].cattr = cpu_to_le32(0);
			range[n].nlb = cpu_to_le32(nlb);
			range[n].slba = cpu_to_le64(slba);
		}
		n++;
	}

	if (WARN_ON_ONCE(n != segments)) {
		if (virt_to_page(range) == ns->ctrl->discard_page)
			clear_bit_unlock(0, &ns->ctrl->discard_page_busy);
		else
			kfree(range);
		return BLK_STS_IOERR;
	}

	cmnd->dsm.opcode = nvme_cmd_dsm;
	cmnd->dsm.nsid = cpu_to_le32(ns->head->ns_id);
	cmnd->dsm.nr = cpu_to_le32(segments - 1);
	cmnd->dsm.attributes = cpu_to_le32(NVME_DSMGMT_AD);

	req->special_vec.bv_page = virt_to_page(range);
	req->special_vec.bv_offset = offset_in_page(range);
	req->special_vec.bv_len = alloc_size;
	req->rq_flags |= RQF_SPECIAL_PAYLOAD;

	return BLK_STS_OK;
}

static inline blk_status_t nvmq_setup_write_zeroes(struct nvmq_ns *ns,
		struct request *req, struct nvme_command *cmnd)
{
	if (ns->ctrl->quirks & NVME_QUIRK_DEALLOCATE_ZEROES)
		return nvmq_setup_discard(ns, req, cmnd);

	cmnd->write_zeroes.opcode = nvme_cmd_write_zeroes;
	cmnd->write_zeroes.nsid = cpu_to_le32(ns->head->ns_id);
	cmnd->write_zeroes.slba =
		cpu_to_le64(nvmq_sect_to_lba(ns, blk_rq_pos(req)));
	cmnd->write_zeroes.length =
		cpu_to_le16((blk_rq_bytes(req) >> ns->lba_shift) - 1);
	if (nvmq_ns_has_pi(ns))
		cmnd->write_zeroes.control = cpu_to_le16(NVME_RW_PRINFO_PRACT);
	else
		cmnd->write_zeroes.control = 0;
	return BLK_STS_OK;
}

static inline blk_status_t nvmq_setup_rw(struct nvmq_ns *ns,
		struct request *req, struct nvme_command *cmnd)
{
	struct nvmq_ctrl *ctrl = ns->ctrl;
	u16 control = 0;
	u32 dsmgmt = 0;

	if (req->cmd_flags & REQ_FUA)
		control |= NVME_RW_FUA;
	if (req->cmd_flags & (REQ_FAILFAST_DEV | REQ_RAHEAD))
		control |= NVME_RW_LR;

	if (req->cmd_flags & REQ_RAHEAD)
		dsmgmt |= NVME_RW_DSM_FREQ_PREFETCH;

	cmnd->rw.opcode = (rq_data_dir(req) ? nvme_cmd_write : nvme_cmd_read);
	cmnd->rw.nsid = cpu_to_le32(ns->head->ns_id);
	cmnd->rw.slba = cpu_to_le64(nvmq_sect_to_lba(ns, blk_rq_pos(req)));
	cmnd->rw.length = cpu_to_le16((blk_rq_bytes(req) >> ns->lba_shift) - 1);

	if (req_op(req) == REQ_OP_WRITE && ctrl->nr_streams)
		nvmq_assign_write_stream(ctrl, req, &control, &dsmgmt);

	if (ns->ms) {
		/*
		 * If formated with metadata, the block layer always provides a
		 * metadata buffer if CONFIG_BLK_DEV_INTEGRITY is enabled.  Else
		 * we enable the PRACT bit for protection information or set the
		 * namespace capacity to zero to prevent any I/O.
		 */
		if (!blk_integrity_rq(req)) {
			if (WARN_ON_ONCE(!nvmq_ns_has_pi(ns)))
				return BLK_STS_NOTSUPP;
			control |= NVME_RW_PRINFO_PRACT;
		}

		switch (ns->pi_type) {
		case NVME_NS_DPS_PI_TYPE3:
			control |= NVME_RW_PRINFO_PRCHK_GUARD;
			break;
		case NVME_NS_DPS_PI_TYPE1:
		case NVME_NS_DPS_PI_TYPE2:
			control |= NVME_RW_PRINFO_PRCHK_GUARD |
					NVME_RW_PRINFO_PRCHK_REF;
			cmnd->rw.reftag = cpu_to_le32(t10_pi_ref_tag(req));
			break;
		}
	}

	cmnd->rw.control = cpu_to_le16(control);
	cmnd->rw.dsmgmt = cpu_to_le32(dsmgmt);
	return 0;
}

void nvmq_cleanup_cmd(struct request *req)
{
	if (req->rq_flags & RQF_SPECIAL_PAYLOAD) {
		struct nvmq_ns *ns = req->rq_disk->private_data;
		struct page *page = req->special_vec.bv_page;

		if (page == ns->ctrl->discard_page)
			clear_bit_unlock(0, &ns->ctrl->discard_page_busy);
		else
			kfree(page_address(page) + req->special_vec.bv_offset);
	}
}
EXPORT_SYMBOL_GPL(nvmq_cleanup_cmd);

blk_status_t nvmq_setup_raw_cmd(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *raw_cmd, struct nvme_command *cmd, bool is_first)
{
	blk_status_t ret = BLK_STS_OK;

	if (is_first) {
		nvmq_clear_nvmq_request(req);
	}

	memcpy(cmd, raw_cmd, sizeof(struct nvme_command));

	cmd->common.command_id = req->tag;
	trace_nvmq_setup_cmd(req, cmd);
	return ret;
}

blk_status_t nvmq_setup_raw_cmd_without_change_cid(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *raw_cmd, struct nvme_command *cmd, bool is_first){
	blk_status_t ret = BLK_STS_OK;

	if (is_first) {
		nvmq_clear_nvmq_request(req);
	}

	memcpy(cmd, raw_cmd, sizeof(struct nvme_command));

	//cmd->common.command_id = req->tag;
	trace_nvmq_setup_cmd(req, cmd);
	return ret;
}

blk_status_t nvmq_setup_cmd(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *cmd)
{
	blk_status_t ret = BLK_STS_OK;

	nvmq_clear_nvmq_request(req);

	memset(cmd, 0, sizeof(*cmd));
	switch (req_op(req)) {
	case REQ_OP_DRV_IN:
	case REQ_OP_DRV_OUT:
		memcpy(cmd, nvmq_req(req)->cmd, sizeof(*cmd));
		break;
	case REQ_OP_FLUSH:
		nvmq_setup_flush(ns, cmd);
		break;
	case REQ_OP_WRITE_ZEROES:
		ret = nvmq_setup_write_zeroes(ns, req, cmd);
		break;
	case REQ_OP_DISCARD:
		ret = nvmq_setup_discard(ns, req, cmd);
		break;
	case REQ_OP_READ:
	case REQ_OP_WRITE:
		ret = nvmq_setup_rw(ns, req, cmd);
		break;
	default:
		WARN_ON_ONCE(1);
		return BLK_STS_IOERR;
	}

	cmd->common.command_id = req->tag;
	trace_nvmq_setup_cmd(req, cmd);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmq_setup_cmd);

static void nvmq_end_sync_rq(struct request *rq, blk_status_t error)
{
	struct completion *waiting = rq->end_io_data;

	rq->end_io_data = NULL;
	complete(waiting);
}

static void nvmq_execute_rq_polled(struct request_queue *q,
		struct gendisk *bd_disk, struct request *rq, int at_head)
{
	DECLARE_COMPLETION_ONSTACK(wait);

	WARN_ON_ONCE(!test_bit(QUEUE_FLAG_POLL, &q->queue_flags));

	rq->cmd_flags |= REQ_HIPRI;
	rq->end_io_data = &wait;
	blk_execute_rq_nowait(q, bd_disk, rq, at_head, nvmq_end_sync_rq);

	while (!completion_done(&wait)) {
		blk_poll(q, request_to_qc_t(rq->mq_hctx, rq), true);
		cond_resched();
	}
}

/*
 * Returns 0 on success.  If the result is negative, it's a Linux error code;
 * if the result is positive, it's an NVM Express status code
 */
int __nvmq_submit_sync_cmd(struct request_queue *q, struct nvme_command *cmd,
		union nvme_result *result, void *buffer, unsigned bufflen,
		unsigned timeout, int qid, int at_head,
		blk_mq_req_flags_t flags, bool poll)
{
	struct request *req;
	int ret;

	req = nvmq_alloc_request(q, cmd, flags, qid);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->timeout = timeout ? timeout : ADMIN_TIMEOUT;

	if (buffer && bufflen) {
		ret = blk_rq_map_kern(q, req, buffer, bufflen, GFP_KERNEL);
		if (ret)
			goto out;
	}

	if (poll)
		nvmq_execute_rq_polled(req->q, NULL, req, at_head);
	else
		blk_execute_rq(req->q, NULL, req, at_head);
	if (result)
		*result = nvmq_req(req)->result;
	if (nvmq_req(req)->flags & NVME_REQ_CANCELLED)
		ret = -EINTR;
	else
		ret = nvmq_req(req)->status;

 out:
	pr_debug("rq %p status %u ret %d\n", nvmq_req(req), nvmq_req(req)->status, ret);
	blk_mq_free_request(req);
	return ret;
}
EXPORT_SYMBOL_GPL(__nvmq_submit_sync_cmd);

int nvmq_submit_sync_cmd(struct request_queue *q, struct nvme_command *cmd,
		void *buffer, unsigned bufflen)
{
	return __nvmq_submit_sync_cmd(q, cmd, NULL, buffer, bufflen, 0,
			NVME_QID_ANY, 0, 0, false);
}
EXPORT_SYMBOL_GPL(nvmq_submit_sync_cmd);

static void *nvmq_add_user_metadata(struct bio *bio, void __user *ubuf,
		unsigned len, u32 seed, bool write)
{
	struct bio_integrity_payload *bip;
	int ret = -ENOMEM;
	void *buf;

	buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		goto out;

	ret = -EFAULT;
	if (write && copy_from_user(buf, ubuf, len))
		goto out_free_meta;

	bip = bio_integrity_alloc(bio, GFP_KERNEL, 1);
	if (IS_ERR(bip)) {
		ret = PTR_ERR(bip);
		goto out_free_meta;
	}

	bip->bip_iter.bi_size = len;
	bip->bip_iter.bi_sector = seed;
	ret = bio_integrity_add_page(bio, virt_to_page(buf), len,
			offset_in_page(buf));
	if (ret == len)
		return buf;
	ret = -ENOMEM;
out_free_meta:
	kfree(buf);
out:
	return ERR_PTR(ret);
}

static int nvmq_submit_user_kernel(struct request_queue *q,
		struct nvme_command *cmds, u8 num_cmds, u64 *result, unsigned timeout)
{
	struct nvmq_ns *ns = q->queuedata;
	struct gendisk *disk = ns ? ns->disk : NULL;
	struct request *req;
	struct bio *bio = NULL;
	// void *meta = NULL;
	int ret;

	req = nvmq_alloc_kernel(q, 0, NVME_QID_ANY);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->timeout = timeout ? timeout : ADMIN_TIMEOUT;
	nvmq_knl(req)->flags |= NVME_REQ_USERKERNEL;
	nvmq_knl(req)->cmds = cmds;
	nvmq_knl(req)->num_cmds = num_cmds;

	// Jinhao: We currently ignore user buffer mapping

	// if (ubuffer && bufflen) {
	// 	ret = blk_rq_map_user(q, req, NULL, ubuffer, bufflen,
	// 			GFP_KERNEL);
	// 	if (ret)
	// 		goto out;
	// 	bio = req->bio;
	// 	bio->bi_disk = disk;
	// 	if (disk && meta_buffer && meta_len) {
	// 		meta = nvmq_add_user_metadata(bio, meta_buffer, meta_len,
	// 				meta_seed, write);
	// 		if (IS_ERR(meta)) {
	// 			ret = PTR_ERR(meta);
	// 			goto out_unmap;
	// 		}
	// 		req->cmd_flags |= REQ_INTEGRITY;
	// 	}
	// }
	
	blk_execute_rq(req->q, disk, req, 0);
	if (nvmq_knl(req)->flags & NVME_REQ_CANCELLED)
		ret = -EINTR;
	else
		ret = nvmq_knl(req)->status;
	if (result)
		*result = le64_to_cpu(nvmq_knl(req)->result.u64);
	// if (meta && !ret) {
	// 	if (copy_to_user(meta_buffer, meta, meta_len))
	// 		ret = -EFAULT;
	// }
	// kfree(meta);
 out_unmap:
	if (bio)
		blk_rq_unmap_user(bio);
 out:
	blk_mq_free_request(req);
	return ret;
}
#define SLM_MASK (1<<31)
static int nvmq_prepare_user_uring_cmd(struct request_queue *q,
	struct nvme_command *cmd, void __user *ubuffer,
	unsigned bufflen, void __user *meta_buffer, unsigned meta_len,
	u32 meta_seed, unsigned timeout,struct async_ns_chr_task *task)
{
	bool write = nvme_is_write(cmd);
	struct nvmq_ns *ns = q->queuedata;
	struct gendisk *disk = ns ? ns->disk : NULL;
	struct request *req;
	struct bio *bio = NULL;
	void *meta = NULL;
	int ret;

	req = nvmq_alloc_request(q, cmd, 0, NVME_QID_ANY);
	if (IS_ERR(req)){
		pr_debug("ERR PTR\n");
		return PTR_ERR(req);
	}

	req->timeout = timeout ? timeout : ADMIN_TIMEOUT;
	nvmq_req(req)->flags |= NVME_REQ_USERCMD;

	if (ubuffer && bufflen) {
		pr_debug("OH! q address%llx req%llx ubuffer%llx bufferlen%d\n",q,req,ubuffer,bufflen);
		if(nvme_is_slm_rw(cmd))
		{
			//only slm read write could work only prepare for operate host data bigger than 128KB
			ret = nvmq_blk_rq_map_user(q,req,NULL,ubuffer,bufflen,GFP_KERNEL);
		}else
			ret = blk_rq_map_user(q, req, NULL, ubuffer, bufflen,
				GFP_KERNEL);			
		if (ret){
			pr_debug("OH NO!\n");
			goto out;
		}
		bio = req->bio;
		bio->bi_disk = disk;
		if (disk && meta_buffer && meta_len) {
			meta = nvmq_add_user_metadata(bio, meta_buffer, meta_len,
					meta_seed, write);
			if (IS_ERR(meta)) {
				ret = PTR_ERR(meta);
				goto out_unmap;
			}
			req->cmd_flags |= REQ_INTEGRITY;
		}
	}
	task->bio = bio;
	task->req = req;
	task->meta = meta;
	task->write = write;
	
	return ret;
 out_unmap:
	if(bio)
		pr_err("UNMAP USER\n");
	if (bio)
		blk_rq_unmap_user(bio);
 out:
	blk_mq_free_request(req);
	return ret;
}
static int nvmq_user_uring_cmd(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
	struct nvme_passthru_cmd __user *ucmd,
	struct async_ns_chr_task *task);


static int nvmq_submit_user_cmd(struct request_queue *q,
		struct nvme_command *cmd, void __user *ubuffer,
		unsigned bufflen, void __user *meta_buffer, unsigned meta_len,
		u32 meta_seed, u64 *result, unsigned timeout)
{
	//pr_debug("submit user cmd\n");
	bool write = nvme_is_write(cmd);
	struct nvmq_ns *ns = q->queuedata;
	struct gendisk *disk = ns ? ns->disk : NULL;
	struct request *req;
	struct bio *bio = NULL;
	void *meta = NULL;
	int ret;

	req = nvmq_alloc_request(q, cmd, 0, NVME_QID_ANY);
	if (IS_ERR(req)){
		pr_debug("ERR PTR\n");
		return PTR_ERR(req);
	}

	req->timeout = timeout ? timeout : ADMIN_TIMEOUT;
	nvmq_req(req)->flags |= NVME_REQ_USERCMD;

	if (ubuffer && bufflen) {
		//pr_debug("OH! q address%llx req%llx ubuffer%llx bufferlen%d\n",q,req,ubuffer,bufflen);
		if(nvme_is_slm_rw(cmd))
		{
			ret = nvmq_blk_rq_map_user(q,req,NULL,ubuffer,bufflen,GFP_KERNEL);
		}else
			ret = blk_rq_map_user(q, req, NULL, ubuffer, bufflen,
				GFP_KERNEL);
				
		if (ret){
			goto out;
		}
		bio = req->bio;
		bio->bi_disk = disk;
		if (disk && meta_buffer && meta_len) {
			meta = nvmq_add_user_metadata(bio, meta_buffer, meta_len,
					meta_seed, write);
			if (IS_ERR(meta)) {
				ret = PTR_ERR(meta);
				goto out_unmap;
			}
			req->cmd_flags |= REQ_INTEGRITY;
		}
	}
	
	
	blk_execute_rq(req->q, disk, req, 0);
	if (nvmq_req(req)->flags & NVME_REQ_CANCELLED)
		ret = -EINTR;
	else
		ret = nvmq_req(req)->status;
	if (result)
		*result = le64_to_cpu(nvmq_req(req)->result.u64);
	if (meta && !ret && !write) {
		if (copy_to_user(meta_buffer, meta, meta_len))
			ret = -EFAULT;
	}
	kfree(meta);
 out_unmap:
	if (bio)
		blk_rq_unmap_user(bio);
 out:
	blk_mq_free_request(req);
	return ret;
}

static void nvmq_keep_alive_end_io(struct request *rq, blk_status_t status)
{
	struct nvmq_ctrl *ctrl = rq->end_io_data;
	unsigned long flags;
	bool startka = false;

	blk_mq_free_request(rq);

	if (status) {
		dev_err(ctrl->device,
			"failed nvme_keep_alive_end_io error=%d\n",
				status);
		return;
	}

	ctrl->comp_seen = false;
	spin_lock_irqsave(&ctrl->lock, flags);
	if (ctrl->state == NVME_CTRL_LIVE ||
	    ctrl->state == NVME_CTRL_CONNECTING)
		startka = true;
	spin_unlock_irqrestore(&ctrl->lock, flags);
	if (startka)
		queue_delayed_work(nvmq_wq, &ctrl->ka_work, ctrl->kato * HZ);
}

static int nvmq_keep_alive(struct nvmq_ctrl *ctrl)
{
	struct request *rq;

	rq = nvmq_alloc_request(ctrl->admin_q, &ctrl->ka_cmd, BLK_MQ_REQ_RESERVED,
			NVME_QID_ANY);
	if (IS_ERR(rq))
		return PTR_ERR(rq);

	rq->timeout = ctrl->kato * HZ;
	rq->end_io_data = ctrl;

	blk_execute_rq_nowait(rq->q, NULL, rq, 0, nvmq_keep_alive_end_io);

	return 0;
}

static void nvmq_keep_alive_work(struct work_struct *work)
{
	struct nvmq_ctrl *ctrl = container_of(to_delayed_work(work),
			struct nvmq_ctrl, ka_work);
	bool comp_seen = ctrl->comp_seen;

	if ((ctrl->ctratt & NVME_CTRL_ATTR_TBKAS) && comp_seen) {
		dev_dbg(ctrl->device,
			"reschedule traffic based keep-alive timer\n");
		ctrl->comp_seen = false;
		queue_delayed_work(nvmq_wq, &ctrl->ka_work, ctrl->kato * HZ);
		return;
	}

	if (nvmq_keep_alive(ctrl)) {
		/* allocation failure, reset the controller */
		dev_err(ctrl->device, "keep-alive failed\n");
		nvmq_reset_ctrl(ctrl);
		return;
	}
}

static void nvmq_start_keep_alive(struct nvmq_ctrl *ctrl)
{
	if (unlikely(ctrl->kato == 0))
		return;

	queue_delayed_work(nvmq_wq, &ctrl->ka_work, ctrl->kato * HZ);
}

void nvmq_stop_keep_alive(struct nvmq_ctrl *ctrl)
{
	if (unlikely(ctrl->kato == 0))
		return;

	cancel_delayed_work_sync(&ctrl->ka_work);
}
EXPORT_SYMBOL_GPL(nvmq_stop_keep_alive);

/*
 * In NVMe 1.0 the CNS field was just a binary controller or namespace
 * flag, thus sending any new CNS opcodes has a big chance of not working.
 * Qemu unfortunately had that bug after reporting a 1.1 version compliance
 * (but not for any later version).
 */
static bool nvmq_ctrl_limited_cns(struct nvmq_ctrl *ctrl)
{
	if (ctrl->quirks & NVME_QUIRK_IDENTIFY_CNS)
		return ctrl->vs < NVME_VS(1, 2, 0);
	return ctrl->vs < NVME_VS(1, 1, 0);
}

static int nvmq_identify_ctrl(struct nvmq_ctrl *dev, struct nvme_id_ctrl **id)
{
	struct nvme_command c = { };
	int error;

	/* gcc-4.4.4 (at least) has issues with initializers and anon unions */
	c.identify.opcode = nvme_admin_identify;
	c.identify.cns = NVME_ID_CNS_CTRL;

	*id = kmalloc(sizeof(struct nvme_id_ctrl), GFP_KERNEL);
	if (!*id)
		return -ENOMEM;

	error = nvmq_submit_sync_cmd(dev->admin_q, &c, *id,
			sizeof(struct nvme_id_ctrl));
	if (error)
		kfree(*id);
	return error;
}

static int nvmq_identify_ns_descs(struct nvmq_ctrl *ctrl, unsigned nsid,
		struct nvmq_ns_ids *ids)
{
	struct nvme_command c = { };
	int status;
	void *data;
	int pos;
	int len;

	if (ctrl->quirks & NVME_QUIRK_NO_NS_DESC_LIST)
		return 0;

	c.identify.opcode = nvme_admin_identify;
	c.identify.nsid = cpu_to_le32(nsid);
	c.identify.cns = NVME_ID_CNS_NS_DESC_LIST;

	data = kzalloc(NVME_IDENTIFY_DATA_SIZE, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	status = nvmq_submit_sync_cmd(ctrl->admin_q, &c, data,
				      NVME_IDENTIFY_DATA_SIZE);
	if (status) {
		dev_warn(ctrl->device,
			"Identify Descriptors failed (%d)\n", status);
		goto free_data;
	}

	for (pos = 0; pos < NVME_IDENTIFY_DATA_SIZE; pos += len) {
		struct nvme_ns_id_desc *cur = data + pos;

		if (cur->nidl == 0)
			break;

		switch (cur->nidt) {
		case NVME_NIDT_EUI64:
			if (cur->nidl != NVME_NIDT_EUI64_LEN) {
				dev_warn(ctrl->device,
					 "ctrl returned bogus length: %d for NVME_NIDT_EUI64\n",
					 cur->nidl);
				goto free_data;
			}
			len = NVME_NIDT_EUI64_LEN;
			memcpy(ids->eui64, data + pos + sizeof(*cur), len);
			break;
		case NVME_NIDT_NGUID:
			if (cur->nidl != NVME_NIDT_NGUID_LEN) {
				dev_warn(ctrl->device,
					 "ctrl returned bogus length: %d for NVME_NIDT_NGUID\n",
					 cur->nidl);
				goto free_data;
			}
			len = NVME_NIDT_NGUID_LEN;
			memcpy(ids->nguid, data + pos + sizeof(*cur), len);
			break;
		case NVME_NIDT_UUID:
			if (cur->nidl != NVME_NIDT_UUID_LEN) {
				dev_warn(ctrl->device,
					 "ctrl returned bogus length: %d for NVME_NIDT_UUID\n",
					 cur->nidl);
				goto free_data;
			}
			len = NVME_NIDT_UUID_LEN;
			uuid_copy(&ids->uuid, data + pos + sizeof(*cur));
			break;
		default:
			/* Skip unknown types */
			len = cur->nidl;
			break;
		}

		len += sizeof(*cur);
	}
free_data:
	kfree(data);
	return status;
}

static int nvmq_identify_ns_list(struct nvmq_ctrl *dev, unsigned nsid, __le32 *ns_list)
{
	struct nvme_command c = { };

	c.identify.opcode = nvme_admin_identify;
	c.identify.cns = NVME_ID_CNS_NS_ACTIVE_LIST;
	c.identify.nsid = cpu_to_le32(nsid);
	return nvmq_submit_sync_cmd(dev->admin_q, &c, ns_list,
				    NVME_IDENTIFY_DATA_SIZE);
}

static int nvmq_identify_ns(struct nvmq_ctrl *ctrl,
		unsigned nsid, struct nvme_id_ns **id)
{
	struct nvme_command c = { };
	int error;

	/* gcc-4.4.4 (at least) has issues with initializers and anon unions */
	c.identify.opcode = nvme_admin_identify;
	c.identify.nsid = cpu_to_le32(nsid);
	c.identify.cns = NVME_ID_CNS_NS;

	*id = kmalloc(sizeof(**id), GFP_KERNEL);
	if (!*id)
		return -ENOMEM;

	error = nvmq_submit_sync_cmd(ctrl->admin_q, &c, *id, sizeof(**id));
	if (error) {
		dev_warn(ctrl->device, "Identify namespace failed (%d)\n", error);
		kfree(*id);
	}

	return error;
}

static int nvmq_features(struct nvmq_ctrl *dev, u8 op, unsigned int fid,
		unsigned int dword11, void *buffer, size_t buflen, u32 *result)
{
	union nvme_result res = { 0 };
	struct nvme_command c;
	int ret;

	memset(&c, 0, sizeof(c));
	c.features.opcode = op;
	c.features.fid = cpu_to_le32(fid);
	c.features.dword11 = cpu_to_le32(dword11);

	ret = __nvmq_submit_sync_cmd(dev->admin_q, &c, &res,
			buffer, buflen, 0, NVME_QID_ANY, 0, 0, false);
	if (ret >= 0 && result)
		*result = le32_to_cpu(res.u32);
	return ret;
}

int nvmq_set_features(struct nvmq_ctrl *dev, unsigned int fid,
		      unsigned int dword11, void *buffer, size_t buflen,
		      u32 *result)
{
	return nvmq_features(dev, nvme_admin_set_features, fid, dword11, buffer,
			     buflen, result);
}
EXPORT_SYMBOL_GPL(nvmq_set_features);

int nvmq_get_features(struct nvmq_ctrl *dev, unsigned int fid,
		      unsigned int dword11, void *buffer, size_t buflen,
		      u32 *result)
{
	return nvmq_features(dev, nvme_admin_get_features, fid, dword11, buffer,
			     buflen, result);
}
EXPORT_SYMBOL_GPL(nvmq_get_features);

int nvmq_set_queue_count(struct nvmq_ctrl *ctrl, int *count)
{
	u32 q_count = (*count - 1) | ((*count - 1) << 16);
	u32 result;
	int status, nr_io_queues;

	status = nvmq_set_features(ctrl, NVME_FEAT_NUM_QUEUES, q_count, NULL, 0,
			&result);
	if (status < 0)
		return status;

	/*
	 * Degraded controllers might return an error when setting the queue
	 * count.  We still want to be able to bring them online and offer
	 * access to the admin queue, as that might be only way to fix them up.
	 */
	if (status > 0) {
		dev_err(ctrl->device, "Could not set queue count (%d)\n", status);
		*count = 0;
	} else {
		nr_io_queues = min(result & 0xffff, result >> 16) + 1;
		*count = min(*count, nr_io_queues);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(nvmq_set_queue_count);

#define NVME_AEN_SUPPORTED \
	(NVME_AEN_CFG_NS_ATTR | NVME_AEN_CFG_FW_ACT | \
	 NVME_AEN_CFG_ANA_CHANGE | NVME_AEN_CFG_DISC_CHANGE)

static void nvmq_enable_aen(struct nvmq_ctrl *ctrl)
{
	u32 result, supported_aens = ctrl->oaes & NVME_AEN_SUPPORTED;
	int status;

	if (!supported_aens)
		return;

	status = nvmq_set_features(ctrl, NVME_FEAT_ASYNC_EVENT, supported_aens,
			NULL, 0, &result);
	if (status)
		dev_warn(ctrl->device, "Failed to configure AEN (cfg %x)\n",
			 supported_aens);

	queue_work(nvmq_wq, &ctrl->async_event_work);
}

/*
 * Convert integer values from ioctl structures to user pointers, silently
 * ignoring the upper bits in the compat case to match behaviour of 32-bit
 * kernels.
 */
static void __user *nvmq_to_user_ptr(uintptr_t ptrval)
{
	if (in_compat_syscall())
		ptrval = (compat_uptr_t)ptrval;
	return (void __user *)ptrval;
}

static int nvmq_submit_io(struct nvmq_ns *ns, struct nvme_user_io __user *uio)
{
	struct nvme_user_io io;
	struct nvme_command c;
	unsigned length, meta_len;
	void __user *metadata;

	if (copy_from_user(&io, uio, sizeof(io)))
		return -EFAULT;
	if (io.flags)
		return -EINVAL;

	switch (io.opcode) {
	case nvme_cmd_write:
	case nvme_cmd_read:
	case nvme_cmd_compare:
		break;
	default:
		return -EINVAL;
	}

	length = (io.nblocks + 1) << ns->lba_shift;

	if ((io.control & NVME_RW_PRINFO_PRACT) &&
	    ns->ms == sizeof(struct t10_pi_tuple)) {
		/*
		 * Protection information is stripped/inserted by the
		 * controller.
		 */
		if (nvmq_to_user_ptr(io.metadata))
			return -EINVAL;
		meta_len = 0;
		metadata = NULL;
	} else {
		meta_len = (io.nblocks + 1) * ns->ms;
		metadata = nvmq_to_user_ptr(io.metadata);
	}

	if (ns->ext) {
		length += meta_len;
		meta_len = 0;
	} else if (meta_len) {
		if ((io.metadata & 3) || !io.metadata)
			return -EINVAL;
	}

	memset(&c, 0, sizeof(c));
	c.rw.opcode = io.opcode;
	c.rw.flags = io.flags;
	c.rw.nsid = cpu_to_le32(ns->head->ns_id);
	c.rw.slba = cpu_to_le64(io.slba);
	c.rw.length = cpu_to_le16(io.nblocks);
	c.rw.control = cpu_to_le16(io.control);
	c.rw.dsmgmt = cpu_to_le32(io.dsmgmt);
	c.rw.reftag = cpu_to_le32(io.reftag);
	c.rw.apptag = cpu_to_le16(io.apptag);
	c.rw.appmask = cpu_to_le16(io.appmask);

	return nvmq_submit_user_cmd(ns->queue, &c,
			nvmq_to_user_ptr(io.addr), length,
			metadata, meta_len, lower_32_bits(io.slba), NULL, 0);
}

static u32 nvmq_known_admin_effects(u8 opcode)
{
	switch (opcode) {
	case nvme_admin_format_nvm:
		return NVME_CMD_EFFECTS_CSUPP | NVME_CMD_EFFECTS_LBCC |
					NVME_CMD_EFFECTS_CSE_MASK;
	case nvme_admin_sanitize_nvm:
		return NVME_CMD_EFFECTS_CSE_MASK;
	default:
		break;
	}
	return 0;
}

static u32 nvmq_passthru_start(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
								u8 opcode)
{
	u32 effects = 0;

	if (ns) {
		if (ctrl->effects)
			effects = le32_to_cpu(ctrl->effects->iocs[opcode]);
		if (effects & ~(NVME_CMD_EFFECTS_CSUPP | NVME_CMD_EFFECTS_LBCC))
			dev_warn(ctrl->device,
				 "IO command:%02x has unhandled effects:%08x\n",
				 opcode, effects);
		return 0;
	}

	if (ctrl->effects)
		effects = le32_to_cpu(ctrl->effects->acs[opcode]);
	effects |= nvmq_known_admin_effects(opcode);

	/*
	 * For simplicity, IO to all namespaces is quiesced even if the command
	 * effects say only one namespace is affected.
	 */
	if (effects & (NVME_CMD_EFFECTS_LBCC | NVME_CMD_EFFECTS_CSE_MASK)) {
		mutex_lock(&ctrl->scan_lock);
		mutex_lock(&ctrl->subsys->lock);
		nvmq_mpath_start_freeze(ctrl->subsys);
		nvmq_mpath_wait_freeze(ctrl->subsys);
		nvmq_start_freeze(ctrl);
		nvmq_wait_freeze(ctrl);
	}
	return effects;
}

static void nvmq_update_formats(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		if (ns->disk && nvmq_revalidate_disk(ns->disk))
			nvmq_set_queue_dying(ns);
	up_read(&ctrl->namespaces_rwsem);
}

static void nvmq_passthru_end(struct nvmq_ctrl *ctrl, u32 effects)
{
	/*
	 * Revalidate LBA changes prior to unfreezing. This is necessary to
	 * prevent memory corruption if a logical block size was changed by
	 * this command.
	 */
	if (effects & NVME_CMD_EFFECTS_LBCC)
		nvmq_update_formats(ctrl);
	if (effects & (NVME_CMD_EFFECTS_LBCC | NVME_CMD_EFFECTS_CSE_MASK)) {
		nvmq_unfreeze(ctrl);
		nvmq_mpath_unfreeze(ctrl->subsys);
		mutex_unlock(&ctrl->subsys->lock);
		nvmq_remove_invalid_namespaces(ctrl, NVME_NSID_ALL);
		mutex_unlock(&ctrl->scan_lock);
	}
	if (effects & NVME_CMD_EFFECTS_CCC)
		nvmq_init_identify(ctrl);
	if (effects & (NVME_CMD_EFFECTS_NIC | NVME_CMD_EFFECTS_NCC))
		nvmq_queue_scan(ctrl);
}

struct nvme_passthru_kernel {
	struct nvme_passthru_cmd64 __user *cmds;
	__u8   num_cmds;
};

#define NVME_IOCTL_KERNEL     _IOWR('N', 0x49, struct nvme_passthru_kernel)

static int nvmq_user_kernel(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
			struct nvme_passthru_cmd __user *ukernel)
{
	struct nvme_passthru_kernel kernel;
	unsigned timeout = 0;
	u32 effects;
	u64 result;
	int status;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (copy_from_user(&kernel, ukernel, sizeof(kernel)))
		return -EFAULT;
	
	pr_debug("Got kernel with %u commands\n", kernel.num_cmds);

	struct nvme_command c[kernel.num_cmds];
	int i;
	//TODO 表示遇到了计算核
	// Fill in the commands with data copied from kernel.cmds
	for (i = 0; i < kernel.num_cmds;) {
		struct nvme_passthru_cmd64 cmd;
		if (copy_from_user(&cmd, kernel.cmds + i, sizeof(cmd)))
			return -EFAULT;
		if (cmd.flags && cmd.opcode != 0x26)
			return -EINVAL;
		memset(&c[i], 0, sizeof(c[i]));
		c[i].common.opcode = cmd.opcode;
		c[i].common.flags = cmd.flags;
		c[i].common.nsid = cpu_to_le32(cmd.nsid);
		c[i].common.cdw2[0] = cpu_to_le32(cmd.cdw2);
		c[i].common.cdw2[1] = cpu_to_le32(cmd.cdw3);
		c[i].common.cdw10 = cpu_to_le32(cmd.cdw10);
		c[i].common.cdw11 = cpu_to_le32(cmd.cdw11);
		c[i].common.cdw12 = cpu_to_le32(cmd.cdw12);
		c[i].common.cdw13 = cpu_to_le32(cmd.cdw13);
		c[i].common.cdw14 = cpu_to_le32(cmd.cdw14);
		c[i].common.cdw15 = cpu_to_le32(cmd.cdw15);
		c[i].rw.length = cpu_to_le16(cmd.data_len - 1);
		c[i].rw.dptr.prp1 = cpu_to_le64(cmd.addr);
		pr_debug("CMD %d opcode %u", i, c[i].common.opcode);

		///////
		i++;//注意！！！！
		//////

		if(cmd.opcode == 0x26)
		{
			//TODO memcopy 当前写死，后面23需要修改
			memcpy(&c[i-1],&cmd,sizeof(char)*64);
			uint32_t payload_size = (__le32)cmd.metadata;
			int j = 0;
			for(;j < payload_size && (i+j) < kernel.num_cmds ;j++){
				if (copy_from_user(&cmd, kernel.cmds + i + j, sizeof(cmd)))
					return -EFAULT;
				memcpy(&c[i+j],&cmd,sizeof(char)*64);
				int k=0;
				for(k=0;k<8;k++){
					pr_debug("%u ",((uint64_t*)&cmd)[k]);
				}
				pr_debug("GRAPH DATA DUMP\n");
			}
			i += j;
		}
	}

	// effects = nvmq_passthru_start(ctrl, ns, cmd.opcode);
	status = nvmq_submit_user_kernel(ns ? ns->queue : ctrl->admin_q,
									 c, kernel.num_cmds,
									 &result, timeout);
	// nvmq_passthru_end(ctrl, effects);

	if (status >= 0) {
		if (put_user(result, &ukernel->result))
			return -EFAULT;
	}

	return status;
	
	return 0;
}



static int nvmq_user_cmd(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
			struct nvme_passthru_cmd __user *ucmd)
{
	struct nvme_passthru_cmd cmd;
	struct nvme_command c;
	unsigned timeout = 0;
	u32 effects;
	u64 result;
	int status;

	if (!capable(CAP_SYS_ADMIN)){
		pr_debug("o1\n");
		return -EACCES;
	}
	if (copy_from_user(&cmd, ucmd, sizeof(cmd))){
		pr_debug("o2\n");
		return -EFAULT;
	}
	if (cmd.flags){
		pr_debug("o3\n");
		return -EINVAL;
	}

	memset(&c, 0, sizeof(c));
	c.common.opcode = cmd.opcode;
	c.common.flags = cmd.flags;
	c.common.nsid = cpu_to_le32(cmd.nsid);
	c.common.cdw2[0] = cpu_to_le32(cmd.cdw2);
	c.common.cdw2[1] = cpu_to_le32(cmd.cdw3);
	c.common.cdw10 = cpu_to_le32(cmd.cdw10);
	c.common.cdw11 = cpu_to_le32(cmd.cdw11);
	c.common.cdw12 = cpu_to_le32(cmd.cdw12);
	c.common.cdw13 = cpu_to_le32(cmd.cdw13);
	c.common.cdw14 = cpu_to_le32(cmd.cdw14);
	c.common.cdw15 = cpu_to_le32(cmd.cdw15);

	if (cmd.timeout_ms)
		timeout = msecs_to_jiffies(cmd.timeout_ms);

	effects = nvmq_passthru_start(ctrl, ns, cmd.opcode);
	status = nvmq_submit_user_cmd(ns ? ns->queue : ctrl->admin_q, &c,
			nvmq_to_user_ptr(cmd.addr), cmd.data_len,
			nvmq_to_user_ptr(cmd.metadata), cmd.metadata_len,
			0, &result, timeout);
	nvmq_passthru_end(ctrl, effects);

	if (status >= 0) {
		if (put_user(result, &ucmd->result))
			return -EFAULT;
	}

	return status;
}

static int nvmq_user_uring_cmd(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
	struct nvme_passthru_cmd __user *ucmd,
	struct async_ns_chr_task *task)
{
	struct nvme_passthru_cmd cmd;
	struct nvme_command* c = &(task->c);
	unsigned timeout = 0;
	u32 effects;
	u64 result;
	int status;

	if (!capable(CAP_SYS_ADMIN)){
	pr_debug("o1\n");
	return -EACCES;
	}
	if (copy_from_user(&cmd, ucmd, sizeof(cmd))){
	pr_debug("o2\n");
	return -EFAULT;
	}
	if (cmd.flags){
	pr_debug("o3\n");
	return -EINVAL;
	}

	memset(c, 0, sizeof(c));
	c->common.opcode = cmd.opcode;
	c->common.flags = cmd.flags;
	c->common.nsid = cpu_to_le32(cmd.nsid);
	c->common.cdw2[0] = cpu_to_le32(cmd.cdw2);
	c->common.cdw2[1] = cpu_to_le32(cmd.cdw3);
	c->common.cdw10 = cpu_to_le32(cmd.cdw10);
	c->common.cdw11 = cpu_to_le32(cmd.cdw11);
	c->common.cdw12 = cpu_to_le32(cmd.cdw12);
	c->common.cdw13 = cpu_to_le32(cmd.cdw13);
	c->common.cdw14 = cpu_to_le32(cmd.cdw14);
	c->common.cdw15 = cpu_to_le32(cmd.cdw15);

	if (cmd.timeout_ms)
	timeout = msecs_to_jiffies(cmd.timeout_ms);

	effects = nvmq_passthru_start(ctrl, ns, cmd.opcode);
	status = nvmq_prepare_user_uring_cmd((ns ? ns->queue : ctrl->admin_q), c,
		nvmq_to_user_ptr(cmd.addr), cmd.data_len,
		nvmq_to_user_ptr(cmd.metadata), cmd.metadata_len,
		0,  timeout,task);
	nvmq_passthru_end(ctrl, effects);

	return status;
}

static int nvmq_user_cmd64(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns,
			struct nvme_passthru_cmd64 __user *ucmd)
{
	struct nvme_passthru_cmd64 cmd;
	struct nvme_command c;
	unsigned timeout = 0;
	u32 effects;
	int status;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (copy_from_user(&cmd, ucmd, sizeof(cmd)))
		return -EFAULT;
	if (cmd.flags)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.common.opcode = cmd.opcode;
	c.common.flags = cmd.flags;
	c.common.nsid = cpu_to_le32(cmd.nsid);
	c.common.cdw2[0] = cpu_to_le32(cmd.cdw2);
	c.common.cdw2[1] = cpu_to_le32(cmd.cdw3);
	c.common.cdw10 = cpu_to_le32(cmd.cdw10);
	c.common.cdw11 = cpu_to_le32(cmd.cdw11);
	c.common.cdw12 = cpu_to_le32(cmd.cdw12);
	c.common.cdw13 = cpu_to_le32(cmd.cdw13);
	c.common.cdw14 = cpu_to_le32(cmd.cdw14);
	c.common.cdw15 = cpu_to_le32(cmd.cdw15);

	if (cmd.timeout_ms)
		timeout = msecs_to_jiffies(cmd.timeout_ms);

	effects = nvmq_passthru_start(ctrl, ns, cmd.opcode);
	status = nvmq_submit_user_cmd(ns ? ns->queue : ctrl->admin_q, &c,
			nvmq_to_user_ptr(cmd.addr), cmd.data_len,
			nvmq_to_user_ptr(cmd.metadata), cmd.metadata_len,
			0, &cmd.result, timeout);
	nvmq_passthru_end(ctrl, effects);

	if (status >= 0) {
		if (put_user(cmd.result, &ucmd->result))
			return -EFAULT;
	}

	return status;
}



/*
 * Issue ioctl requests on the first available path.  Note that unlike normal
 * block layer requests we will not retry failed request on another controller.
 */
static struct nvmq_ns *nvmq_get_ns_from_disk(struct gendisk *disk,
		struct nvmq_ns_head **head, int *srcu_idx)
{
#ifdef CONFIG_NVMQ_MULTIPATH
	if (disk->fops == &nvmq_ns_head_ops) {
		struct nvmq_ns *ns;

		*head = disk->private_data;
		*srcu_idx = srcu_read_lock(&(*head)->srcu);
		ns = nvmq_find_path(*head);
		if (!ns)
			srcu_read_unlock(&(*head)->srcu, *srcu_idx);
		return ns;
	}
#endif
	*head = NULL;
	*srcu_idx = -1;
	return disk->private_data;
}

static void nvmq_put_ns_from_disk(struct nvmq_ns_head *head, int idx)
{
	if (head)
		srcu_read_unlock(&head->srcu, idx);
}

static bool is_ctrl_ioctl(unsigned int cmd)
{
	if (cmd == NVME_IOCTL_ADMIN_CMD || cmd == NVME_IOCTL_ADMIN64_CMD)
		return true;
	if (is_sed_ioctl(cmd))
		return true;
	return false;
}

static int nvmq_handle_ctrl_ioctl(struct nvmq_ns *ns, unsigned int cmd,
				  void __user *argp,
				  struct nvmq_ns_head *head,
				  int srcu_idx)
{
	struct nvmq_ctrl *ctrl = ns->ctrl;
	int ret;

	nvmq_get_ctrl(ns->ctrl);
	nvmq_put_ns_from_disk(head, srcu_idx);

	switch (cmd) {
	case NVME_IOCTL_ADMIN_CMD:
		ret = nvmq_user_cmd(ctrl, NULL, argp);
		break;
	case NVME_IOCTL_ADMIN64_CMD:
		ret = nvmq_user_cmd64(ctrl, NULL, argp);
		break;
	default:
		ret = sed_ioctl(ctrl->opal_dev, cmd, argp);
		break;
	}
	nvmq_put_ctrl(ctrl);
	return ret;
}

static int nvmq_ioctl(struct block_device *bdev, fmode_t mode,
		unsigned int cmd, unsigned long arg)
{
	struct nvmq_ns_head *head = NULL;
	void __user *argp = (void __user *)arg;
	struct nvmq_ns *ns;
	int srcu_idx, ret;
	pr_debug("Got IOCTL \n");

	ns = nvmq_get_ns_from_disk(bdev->bd_disk, &head, &srcu_idx);
	if (unlikely(!ns))
		return -EWOULDBLOCK;

	/*
	 * Handle ioctls that apply to the controller instead of the namespace
	 * seperately and drop the ns SRCU reference early.  This avoids a
	 * deadlock when deleting namespaces using the passthrough interface.
	 */
	if (is_ctrl_ioctl(cmd))
		return nvmq_handle_ctrl_ioctl(ns, cmd, argp, head, srcu_idx);

	switch (cmd) {
	case NVME_IOCTL_ID:
		force_successful_syscall_return();
		ret = ns->head->ns_id;
		break;
	case NVME_IOCTL_IO_CMD:
		ret = nvmq_user_cmd(ns->ctrl, ns, argp);
		break;
	case NVME_IOCTL_KERNEL:
		ret = nvmq_user_kernel(ns->ctrl, ns, argp);
		break;
	case NVME_IOCTL_SUBMIT_IO:
		ret = nvmq_submit_io(ns, argp);
		
		break;
	case NVME_IOCTL_IO64_CMD:
		ret = nvmq_user_cmd64(ns->ctrl, ns, argp);
		break;
	default:
		if (ns->ndev)
			ret = nvmq_nvm_ioctl(ns, cmd, arg);
		else
			ret = -ENOTTY;
	}

	nvmq_put_ns_from_disk(head, srcu_idx);
	return ret;
}

static int nvmq_open(struct block_device *bdev, fmode_t mode)
{
	struct nvmq_ns *ns = bdev->bd_disk->private_data;

#ifdef CONFIG_NVMQ_MULTIPATH
	/* should never be called due to GENHD_FL_HIDDEN */
	if (WARN_ON_ONCE(ns->head->disk))
		goto fail;
#endif
	if (!kref_get_unless_zero(&ns->kref))
		goto fail;
	if (!try_module_get(ns->ctrl->ops->module))
		goto fail_put_ns;

	return 0;

fail_put_ns:
	nvmq_put_ns(ns);
fail:
	return -ENXIO;
}

static void nvmq_release(struct gendisk *disk, fmode_t mode)
{
	struct nvmq_ns *ns = disk->private_data;

	module_put(ns->ctrl->ops->module);
	nvmq_put_ns(ns);
}

static int nvmq_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	/* some standard values */
	geo->heads = 1 << 6;
	geo->sectors = 1 << 5;
	geo->cylinders = get_capacity(bdev->bd_disk) >> 11;
	return 0;
}

#ifdef CONFIG_BLK_DEV_INTEGRITY
static void nvmq_init_integrity(struct gendisk *disk, u16 ms, u8 pi_type)
{
	struct blk_integrity integrity;

	memset(&integrity, 0, sizeof(integrity));
	switch (pi_type) {
	case NVME_NS_DPS_PI_TYPE3:
		integrity.profile = &t10_pi_type3_crc;
		integrity.tag_size = sizeof(u16) + sizeof(u32);
		integrity.flags |= BLK_INTEGRITY_DEVICE_CAPABLE;
		break;
	case NVME_NS_DPS_PI_TYPE1:
	case NVME_NS_DPS_PI_TYPE2:
		integrity.profile = &t10_pi_type1_crc;
		integrity.tag_size = sizeof(u16);
		integrity.flags |= BLK_INTEGRITY_DEVICE_CAPABLE;
		break;
	default:
		integrity.profile = NULL;
		break;
	}
	integrity.tuple_size = ms;
	blk_integrity_register(disk, &integrity);
	blk_queue_max_integrity_segments(disk->queue, 1);
}
#else
static void nvmq_init_integrity(struct gendisk *disk, u16 ms, u8 pi_type)
{
}
#endif /* CONFIG_BLK_DEV_INTEGRITY */

static void nvmq_config_discard(struct gendisk *disk, struct nvmq_ns *ns)
{
	struct nvmq_ctrl *ctrl = ns->ctrl;
	struct request_queue *queue = disk->queue;
	u32 size = queue_logical_block_size(queue);

	if (!(ctrl->oncs & NVME_CTRL_ONCS_DSM)) {
		blk_queue_flag_clear(QUEUE_FLAG_DISCARD, queue);
		return;
	}

	if (ctrl->nr_streams && ns->sws && ns->sgs)
		size *= ns->sws * ns->sgs;

	BUILD_BUG_ON(PAGE_SIZE / sizeof(struct nvme_dsm_range) <
			NVME_DSM_MAX_RANGES);

	queue->limits.discard_alignment = 0;
	queue->limits.discard_granularity = size;

	/* If discard is already enabled, don't reset queue limits */
	if (blk_queue_flag_test_and_set(QUEUE_FLAG_DISCARD, queue))
		return;

	blk_queue_max_discard_sectors(queue, UINT_MAX);
	blk_queue_max_discard_segments(queue, NVME_DSM_MAX_RANGES);

	if (ctrl->quirks & NVME_QUIRK_DEALLOCATE_ZEROES)
		blk_queue_max_write_zeroes_sectors(queue, UINT_MAX);
}

/*
 * Even though NVMe spec explicitly states that MDTS is not applicable to the
 * write-zeroes, we are cautious and limit the size to the controllers
 * max_hw_sectors value, which is based on the MDTS field and possibly other
 * limiting factors.
 */
static void nvmq_config_write_zeroes(struct request_queue *q,
		struct nvmq_ctrl *ctrl)
{
	if ((ctrl->oncs & NVME_CTRL_ONCS_WRITE_ZEROES) &&
	    !(ctrl->quirks & NVME_QUIRK_DISABLE_WRITE_ZEROES))
		blk_queue_max_write_zeroes_sectors(q, ctrl->max_hw_sectors);
}

static int nvmq_report_ns_ids(struct nvmq_ctrl *ctrl, unsigned int nsid,
		struct nvme_id_ns *id, struct nvmq_ns_ids *ids)
{
	memset(ids, 0, sizeof(*ids));

	if (ctrl->vs >= NVME_VS(1, 1, 0))
		memcpy(ids->eui64, id->eui64, sizeof(id->eui64));
	if (ctrl->vs >= NVME_VS(1, 2, 0))
		memcpy(ids->nguid, id->nguid, sizeof(id->nguid));
	if (ctrl->vs >= NVME_VS(1, 3, 0))
		return nvmq_identify_ns_descs(ctrl, nsid, ids);
	return 0;
}

static bool nvmq_ns_ids_valid(struct nvmq_ns_ids *ids)
{
	return !uuid_is_null(&ids->uuid) ||
		memchr_inv(ids->nguid, 0, sizeof(ids->nguid)) ||
		memchr_inv(ids->eui64, 0, sizeof(ids->eui64));
}

static bool nvmq_ns_ids_equal(struct nvmq_ns_ids *a, struct nvmq_ns_ids *b)
{
	return uuid_equal(&a->uuid, &b->uuid) &&
		memcmp(&a->nguid, &b->nguid, sizeof(a->nguid)) == 0 &&
		memcmp(&a->eui64, &b->eui64, sizeof(a->eui64)) == 0;
}

static void nvmq_update_disk_info(struct gendisk *disk,
		struct nvmq_ns *ns, struct nvme_id_ns *id)
{
	sector_t capacity = nvmq_lba_to_sect(ns, le64_to_cpu(id->nsze));
	unsigned short bs = 1 << ns->lba_shift;
	u32 atomic_bs, phys_bs, io_opt;

	if (ns->lba_shift > PAGE_SHIFT) {
		/* unsupported block size, set capacity to 0 later */
		bs = (1 << 9);
	}
	blk_mq_freeze_queue(disk->queue);
	blk_integrity_unregister(disk);

	if (id->nabo == 0) {
		/*
		 * Bit 1 indicates whether NAWUPF is defined for this namespace
		 * and whether it should be used instead of AWUPF. If NAWUPF ==
		 * 0 then AWUPF must be used instead.
		 */
		if (id->nsfeat & (1 << 1) && id->nawupf)
			atomic_bs = (1 + le16_to_cpu(id->nawupf)) * bs;
		else
			atomic_bs = (1 + ns->ctrl->subsys->awupf) * bs;
	} else {
		atomic_bs = bs;
	}
	phys_bs = bs;
	io_opt = bs;
	if (id->nsfeat & (1 << 4)) {
		/* NPWG = Namespace Preferred Write Granularity */
		phys_bs *= 1 + le16_to_cpu(id->npwg);
		/* NOWS = Namespace Optimal Write Size */
		io_opt *= 1 + le16_to_cpu(id->nows);
	}

	blk_queue_logical_block_size(disk->queue, bs);
	/*
	 * Linux filesystems assume writing a single physical block is
	 * an atomic operation. Hence limit the physical block size to the
	 * value of the Atomic Write Unit Power Fail parameter.
	 */
	blk_queue_physical_block_size(disk->queue, min(phys_bs, atomic_bs));
	blk_queue_io_min(disk->queue, phys_bs);
	blk_queue_io_opt(disk->queue, io_opt);

	if (ns->ms && !ns->ext &&
	    (ns->ctrl->ops->flags & NVME_F_METADATA_SUPPORTED))
		nvmq_init_integrity(disk, ns->ms, ns->pi_type);
	if ((ns->ms && !nvmq_ns_has_pi(ns) && !blk_get_integrity(disk)) ||
	    ns->lba_shift > PAGE_SHIFT)
		capacity = 0;

	set_capacity(disk, capacity);

	nvmq_config_discard(disk, ns);
	nvmq_config_write_zeroes(disk->queue, ns->ctrl);

	if (id->nsattr & (1 << 0))
		set_disk_ro(disk, true);
	else
		set_disk_ro(disk, false);

	blk_mq_unfreeze_queue(disk->queue);
}

static void __nvmq_revalidate_disk(struct gendisk *disk, struct nvme_id_ns *id)
{
	struct nvmq_ns *ns = disk->private_data;
	u32 iob;

	/*
	 * If identify namespace failed, use default 512 byte block size so
	 * block layer can use before failing read/write for 0 capacity.
	 */
	ns->lba_shift = id->lbaf[id->flbas & NVME_NS_FLBAS_LBA_MASK].ds;
	if (ns->lba_shift == 0)
		ns->lba_shift = 9;

	if ((ns->ctrl->quirks & NVME_QUIRK_STRIPE_SIZE) &&
	    is_power_of_2(ns->ctrl->max_hw_sectors))
		iob = ns->ctrl->max_hw_sectors;
	else
		iob = nvmq_lba_to_sect(ns, le16_to_cpu(id->noiob));

	ns->ms = le16_to_cpu(id->lbaf[id->flbas & NVME_NS_FLBAS_LBA_MASK].ms);
	ns->ext = ns->ms && (id->flbas & NVME_NS_FLBAS_META_EXT);
	/* the PI implementation requires metadata equal t10 pi tuple size */
	if (ns->ms == sizeof(struct t10_pi_tuple))
		ns->pi_type = id->dps & NVME_NS_DPS_PI_MASK;
	else
		ns->pi_type = 0;

	if (iob)
		blk_queue_chunk_sectors(ns->queue, rounddown_pow_of_two(iob));
	nvmq_update_disk_info(disk, ns, id);
#ifdef CONFIG_NVMQ_MULTIPATH
	if (ns->head->disk) {
		nvmq_update_disk_info(ns->head->disk, ns, id);
		blk_queue_stack_limits(ns->head->disk->queue, ns->queue);
		nvmq_mpath_update_disk_size(ns->head->disk);
	}
#endif
}

static int nvmq_revalidate_disk(struct gendisk *disk)
{
	struct nvmq_ns *ns = disk->private_data;
	struct nvmq_ctrl *ctrl = ns->ctrl;
	struct nvme_id_ns *id;
	struct nvmq_ns_ids ids;
	int ret = 0;

	if (test_bit(NVME_NS_DEAD, &ns->flags)) {
		set_capacity(disk, 0);
		return -ENODEV;
	}

	ret = nvmq_identify_ns(ctrl, ns->head->ns_id, &id);
	if (ret)
		goto out;

	if (id->ncap == 0) {
		ret = -ENODEV;
		goto free_id;
	}

	__nvmq_revalidate_disk(disk, id);
	ret = nvmq_report_ns_ids(ctrl, ns->head->ns_id, id, &ids);
	if (ret)
		goto free_id;

	if (!nvmq_ns_ids_equal(&ns->head->ids, &ids)) {
		dev_err(ctrl->device,
			"identifiers changed for nsid %d\n", ns->head->ns_id);
		ret = -ENODEV;
	}

free_id:
	kfree(id);
out:
	/*
	 * Only fail the function if we got a fatal error back from the
	 * device, otherwise ignore the error and just move on.
	 */
	if (ret == -ENOMEM || (ret > 0 && !(ret & NVME_SC_DNR)))
		ret = 0;
	else if (ret > 0)
		ret = blk_status_to_errno(nvmq_error_status(ret));
	return ret;
}

static char nvmq_pr_type(enum pr_type type)
{
	switch (type) {
	case PR_WRITE_EXCLUSIVE:
		return 1;
	case PR_EXCLUSIVE_ACCESS:
		return 2;
	case PR_WRITE_EXCLUSIVE_REG_ONLY:
		return 3;
	case PR_EXCLUSIVE_ACCESS_REG_ONLY:
		return 4;
	case PR_WRITE_EXCLUSIVE_ALL_REGS:
		return 5;
	case PR_EXCLUSIVE_ACCESS_ALL_REGS:
		return 6;
	default:
		return 0;
	}
};

static int nvmq_pr_command(struct block_device *bdev, u32 cdw10,
				u64 key, u64 sa_key, u8 op)
{
	struct nvmq_ns_head *head = NULL;
	struct nvmq_ns *ns;
	struct nvme_command c;
	int srcu_idx, ret;
	u8 data[16] = { 0, };

	ns = nvmq_get_ns_from_disk(bdev->bd_disk, &head, &srcu_idx);
	if (unlikely(!ns))
		return -EWOULDBLOCK;

	put_unaligned_le64(key, &data[0]);
	put_unaligned_le64(sa_key, &data[8]);

	memset(&c, 0, sizeof(c));
	c.common.opcode = op;
	c.common.nsid = cpu_to_le32(ns->head->ns_id);
	c.common.cdw10 = cpu_to_le32(cdw10);

	ret = nvmq_submit_sync_cmd(ns->queue, &c, data, 16);
	nvmq_put_ns_from_disk(head, srcu_idx);
	return ret;
}

static int nvmq_pr_register(struct block_device *bdev, u64 old,
		u64 new, unsigned flags)
{
	u32 cdw10;

	if (flags & ~PR_FL_IGNORE_KEY)
		return -EOPNOTSUPP;

	cdw10 = old ? 2 : 0;
	cdw10 |= (flags & PR_FL_IGNORE_KEY) ? 1 << 3 : 0;
	cdw10 |= (1 << 30) | (1 << 31); /* PTPL=1 */
	return nvmq_pr_command(bdev, cdw10, old, new, nvme_cmd_resv_register);
}

static int nvmq_pr_reserve(struct block_device *bdev, u64 key,
		enum pr_type type, unsigned flags)
{
	u32 cdw10;

	if (flags & ~PR_FL_IGNORE_KEY)
		return -EOPNOTSUPP;

	cdw10 = nvmq_pr_type(type) << 8;
	cdw10 |= ((flags & PR_FL_IGNORE_KEY) ? 1 << 3 : 0);
	return nvmq_pr_command(bdev, cdw10, key, 0, nvme_cmd_resv_acquire);
}

static int nvmq_pr_preempt(struct block_device *bdev, u64 old, u64 new,
		enum pr_type type, bool abort)
{
	u32 cdw10 = nvmq_pr_type(type) << 8 | (abort ? 2 : 1);
	return nvmq_pr_command(bdev, cdw10, old, new, nvme_cmd_resv_acquire);
}

static int nvmq_pr_clear(struct block_device *bdev, u64 key)
{
	u32 cdw10 = 1 | (key ? 1 << 3 : 0);
	return nvmq_pr_command(bdev, cdw10, key, 0, nvme_cmd_resv_register);
}

static int nvmq_pr_release(struct block_device *bdev, u64 key, enum pr_type type)
{
	u32 cdw10 = nvmq_pr_type(type) << 8 | (key ? 1 << 3 : 0);
	return nvmq_pr_command(bdev, cdw10, key, 0, nvme_cmd_resv_release);
}

static const struct pr_ops nvmq_pr_ops = {
	.pr_register	= nvmq_pr_register,
	.pr_reserve	= nvmq_pr_reserve,
	.pr_release	= nvmq_pr_release,
	.pr_preempt	= nvmq_pr_preempt,
	.pr_clear	= nvmq_pr_clear,
};

#ifdef CONFIG_BLK_SED_OPAL
int nvmq_sec_submit(void *data, u16 spsp, u8 secp, void *buffer, size_t len,
		bool send)
{
	struct nvmq_ctrl *ctrl = data;
	struct nvme_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	if (send)
		cmd.common.opcode = nvme_admin_security_send;
	else
		cmd.common.opcode = nvme_admin_security_recv;
	cmd.common.nsid = 0;
	cmd.common.cdw10 = cpu_to_le32(((u32)secp) << 24 | ((u32)spsp) << 8);
	cmd.common.cdw11 = cpu_to_le32(len);

	return __nvmq_submit_sync_cmd(ctrl->admin_q, &cmd, NULL, buffer, len,
				      ADMIN_TIMEOUT, NVME_QID_ANY, 1, 0, false);
}
EXPORT_SYMBOL_GPL(nvmq_sec_submit);
#endif /* CONFIG_BLK_SED_OPAL */

static const struct block_device_operations nvmq_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= nvmq_ioctl,
	.compat_ioctl	= nvmq_ioctl,
	.open		= nvmq_open,
	.release	= nvmq_release,
	.getgeo		= nvmq_getgeo,
	.revalidate_disk= nvmq_revalidate_disk,
	.pr_ops		= &nvmq_pr_ops,
};

#ifdef CONFIG_NVMQ_MULTIPATH
static int nvmq_ns_head_open(struct block_device *bdev, fmode_t mode)
{
	struct nvmq_ns_head *head = bdev->bd_disk->private_data;

	if (!kref_get_unless_zero(&head->ref))
		return -ENXIO;
	return 0;
}

static void nvmq_ns_head_release(struct gendisk *disk, fmode_t mode)
{
	nvmq_put_ns_head(disk->private_data);
}

const struct block_device_operations nvmq_ns_head_ops = {
	.owner		= THIS_MODULE,
	.open		= nvmq_ns_head_open,
	.release	= nvmq_ns_head_release,
	.ioctl		= nvmq_ioctl,
	.compat_ioctl	= nvmq_ioctl,
	.getgeo		= nvmq_getgeo,
	.pr_ops		= &nvmq_pr_ops,
};
#endif /* CONFIG_NVMQ_MULTIPATH */

static int nvmq_wait_ready(struct nvmq_ctrl *ctrl, u64 cap, bool enabled)
{
	unsigned long timeout =
		((NVME_CAP_TIMEOUT(cap) + 1) * HZ / 2) + jiffies;
	u32 csts, bit = enabled ? NVME_CSTS_RDY : 0;
	int ret;

	while ((ret = ctrl->ops->reg_read32(ctrl, NVME_REG_CSTS, &csts)) == 0) {
		if (csts == ~0)
			return -ENODEV;
		if ((csts & NVME_CSTS_RDY) == bit)
			break;

		msleep(100);
		if (fatal_signal_pending(current))
			return -EINTR;
		if (time_after(jiffies, timeout)) {
			dev_err(ctrl->device,
				"Device not ready; aborting %s\n", enabled ?
						"initialisation" : "reset");
			return -ENODEV;
		}
	}

	return ret;
}

/*
 * If the device has been passed off to us in an enabled state, just clear
 * the enabled bit.  The spec says we should set the 'shutdown notification
 * bits', but doing so may cause the device to complete commands to the
 * admin queue ... and we don't know what memory that might be pointing at!
 */
int nvmq_disable_ctrl(struct nvmq_ctrl *ctrl)
{
	int ret;

	ctrl->ctrl_config &= ~NVME_CC_SHN_MASK;
	ctrl->ctrl_config &= ~NVME_CC_ENABLE;

	ret = ctrl->ops->reg_write32(ctrl, NVME_REG_CC, ctrl->ctrl_config);
	if (ret)
		return ret;

	if (ctrl->quirks & NVME_QUIRK_DELAY_BEFORE_CHK_RDY)
		msleep(NVME_QUIRK_DELAY_AMOUNT);

	return nvmq_wait_ready(ctrl, ctrl->cap, false);
}
EXPORT_SYMBOL_GPL(nvmq_disable_ctrl);

int nvmq_enable_ctrl(struct nvmq_ctrl *ctrl)
{
	/*
	 * Default to a 4K page size, with the intention to update this
	 * path in the future to accomodate architectures with differing
	 * kernel and IO page sizes.
	 */
	unsigned dev_page_min, page_shift = 12;
	int ret;

	ret = ctrl->ops->reg_read64(ctrl, NVME_REG_CAP, &ctrl->cap);
	if (ret) {
		dev_err(ctrl->device, "Reading CAP failed (%d)\n", ret);
		return ret;
	}
	dev_page_min = NVME_CAP_MPSMIN(ctrl->cap) + 12;

	if (page_shift < dev_page_min) {
		dev_err(ctrl->device,
			"Minimum device page size %u too large for host (%u)\n",
			1 << dev_page_min, 1 << page_shift);
		return -ENODEV;
	}

	ctrl->page_size = 1 << page_shift;

	ctrl->ctrl_config = NVME_CC_CSS_NVM;
	ctrl->ctrl_config |= (page_shift - 12) << NVME_CC_MPS_SHIFT;
	ctrl->ctrl_config |= NVME_CC_AMS_RR | NVME_CC_SHN_NONE;
	ctrl->ctrl_config |= NVME_CC_IOSQES | NVME_CC_IOCQES;
	ctrl->ctrl_config |= NVME_CC_ENABLE;

	ret = ctrl->ops->reg_write32(ctrl, NVME_REG_CC, ctrl->ctrl_config);
	if (ret)
		return ret;
	return nvmq_wait_ready(ctrl, ctrl->cap, true);
}
EXPORT_SYMBOL_GPL(nvmq_enable_ctrl);

int nvmq_shutdown_ctrl(struct nvmq_ctrl *ctrl)
{
	unsigned long timeout = jiffies + (ctrl->shutdown_timeout * HZ);
	u32 csts;
	int ret;

	ctrl->ctrl_config &= ~NVME_CC_SHN_MASK;
	ctrl->ctrl_config |= NVME_CC_SHN_NORMAL;

	ret = ctrl->ops->reg_write32(ctrl, NVME_REG_CC, ctrl->ctrl_config);
	if (ret)
		return ret;

	while ((ret = ctrl->ops->reg_read32(ctrl, NVME_REG_CSTS, &csts)) == 0) {
		if ((csts & NVME_CSTS_SHST_MASK) == NVME_CSTS_SHST_CMPLT)
			break;

		msleep(100);
		if (fatal_signal_pending(current))
			return -EINTR;
		if (time_after(jiffies, timeout)) {
			dev_err(ctrl->device,
				"Device shutdown incomplete; abort shutdown\n");
			return -ENODEV;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(nvmq_shutdown_ctrl);

static void nvmq_set_queue_limits(struct nvmq_ctrl *ctrl,
		struct request_queue *q)
{
	bool vwc = false;

	if (ctrl->max_hw_sectors) {
		u32 max_segments =
			(ctrl->max_hw_sectors / (ctrl->page_size >> 9)) + 1;

		max_segments = min_not_zero(max_segments, ctrl->max_segments);
		blk_queue_max_hw_sectors(q, ctrl->max_hw_sectors);
		blk_queue_max_segments(q, min_t(u32, max_segments, USHRT_MAX));
	}
	blk_queue_virt_boundary(q, ctrl->page_size - 1);
	if (ctrl->vwc & NVME_CTRL_VWC_PRESENT)
		vwc = true;
	blk_queue_write_cache(q, vwc, vwc);
}

static int nvmq_configure_timestamp(struct nvmq_ctrl *ctrl)
{
	__le64 ts;
	int ret;

	if (!(ctrl->oncs & NVME_CTRL_ONCS_TIMESTAMP))
		return 0;

	ts = cpu_to_le64(ktime_to_ms(ktime_get_real()));
	ret = nvmq_set_features(ctrl, NVME_FEAT_TIMESTAMP, 0, &ts, sizeof(ts),
			NULL);
	if (ret)
		dev_warn_once(ctrl->device,
			"could not set timestamp (%d)\n", ret);
	return ret;
}

static int nvmq_configure_acre(struct nvmq_ctrl *ctrl)
{
	struct nvme_feat_host_behavior *host;
	int ret;

	/* Don't bother enabling the feature if retry delay is not reported */
	if (!ctrl->crdt[0])
		return 0;

	host = kzalloc(sizeof(*host), GFP_KERNEL);
	if (!host)
		return 0;

	host->acre = NVME_ENABLE_ACRE;
	ret = nvmq_set_features(ctrl, NVME_FEAT_HOST_BEHAVIOR, 0,
				host, sizeof(*host), NULL);
	kfree(host);
	return ret;
}

static int nvmq_configure_apst(struct nvmq_ctrl *ctrl)
{
	/*
	 * APST (Autonomous Power State Transition) lets us program a
	 * table of power state transitions that the controller will
	 * perform automatically.  We configure it with a simple
	 * heuristic: we are willing to spend at most 2% of the time
	 * transitioning between power states.  Therefore, when running
	 * in any given state, we will enter the next lower-power
	 * non-operational state after waiting 50 * (enlat + exlat)
	 * microseconds, as long as that state's exit latency is under
	 * the requested maximum latency.
	 *
	 * We will not autonomously enter any non-operational state for
	 * which the total latency exceeds ps_max_latency_us.  Users
	 * can set ps_max_latency_us to zero to turn off APST.
	 */

	unsigned apste;
	struct nvme_feat_auto_pst *table;
	u64 max_lat_us = 0;
	int max_ps = -1;
	int ret;

	/*
	 * If APST isn't supported or if we haven't been initialized yet,
	 * then don't do anything.
	 */
	if (!ctrl->apsta)
		return 0;

	if (ctrl->npss > 31) {
		dev_warn(ctrl->device, "NPSS is invalid; not using APST\n");
		return 0;
	}

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return 0;

	if (!ctrl->apst_enabled || ctrl->ps_max_latency_us == 0) {
		/* Turn off APST. */
		apste = 0;
		dev_dbg(ctrl->device, "APST disabled\n");
	} else {
		__le64 target = cpu_to_le64(0);
		int state;

		/*
		 * Walk through all states from lowest- to highest-power.
		 * According to the spec, lower-numbered states use more
		 * power.  NPSS, despite the name, is the index of the
		 * lowest-power state, not the number of states.
		 */
		for (state = (int)ctrl->npss; state >= 0; state--) {
			u64 total_latency_us, exit_latency_us, transition_ms;

			if (target)
				table->entries[state] = target;

			/*
			 * Don't allow transitions to the deepest state
			 * if it's quirked off.
			 */
			if (state == ctrl->npss &&
			    (ctrl->quirks & NVME_QUIRK_NO_DEEPEST_PS))
				continue;

			/*
			 * Is this state a useful non-operational state for
			 * higher-power states to autonomously transition to?
			 */
			if (!(ctrl->psd[state].flags &
			      NVME_PS_FLAGS_NON_OP_STATE))
				continue;

			exit_latency_us =
				(u64)le32_to_cpu(ctrl->psd[state].exit_lat);
			if (exit_latency_us > ctrl->ps_max_latency_us)
				continue;

			total_latency_us =
				exit_latency_us +
				le32_to_cpu(ctrl->psd[state].entry_lat);

			/*
			 * This state is good.  Use it as the APST idle
			 * target for higher power states.
			 */
			transition_ms = total_latency_us + 19;
			do_div(transition_ms, 20);
			if (transition_ms > (1 << 24) - 1)
				transition_ms = (1 << 24) - 1;

			target = cpu_to_le64((state << 3) |
					     (transition_ms << 8));

			if (max_ps == -1)
				max_ps = state;

			if (total_latency_us > max_lat_us)
				max_lat_us = total_latency_us;
		}

		apste = 1;

		if (max_ps == -1) {
			dev_dbg(ctrl->device, "APST enabled but no non-operational states are available\n");
		} else {
			dev_dbg(ctrl->device, "APST enabled: max PS = %d, max round-trip latency = %lluus, table = %*phN\n",
				max_ps, max_lat_us, (int)sizeof(*table), table);
		}
	}

	ret = nvmq_set_features(ctrl, NVME_FEAT_AUTO_PST, apste,
				table, sizeof(*table), NULL);
	if (ret)
		dev_err(ctrl->device, "failed to set APST feature (%d)\n", ret);

	kfree(table);
	return ret;
}

static void nvmq_set_latency_tolerance(struct device *dev, s32 val)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);
	u64 latency;

	switch (val) {
	case PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT:
	case PM_QOS_LATENCY_ANY:
		latency = U64_MAX;
		break;

	default:
		latency = val;
	}

	if (ctrl->ps_max_latency_us != latency) {
		ctrl->ps_max_latency_us = latency;
		if (ctrl->state == NVME_CTRL_LIVE)
			nvmq_configure_apst(ctrl);
	}
}

struct nvme_core_quirk_entry {
	/*
	 * NVMe model and firmware strings are padded with spaces.  For
	 * simplicity, strings in the quirk table are padded with NULLs
	 * instead.
	 */
	u16 vid;
	const char *mn;
	const char *fr;
	unsigned long quirks;
};

static const struct nvme_core_quirk_entry core_quirks[] = {
	{
		/*
		 * This Toshiba device seems to die using any APST states.  See:
		 * https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1678184/comments/11
		 */
		.vid = 0x1179,
		.mn = "THNSF5256GPUK TOSHIBA",
		.quirks = NVME_QUIRK_NO_APST,
	},
	{
		/*
		 * This LiteON CL1-3D*-Q11 firmware version has a race
		 * condition associated with actions related to suspend to idle
		 * LiteON has resolved the problem in future firmware
		 */
		.vid = 0x14a4,
		.fr = "22301111",
		.quirks = NVME_QUIRK_SIMPLE_SUSPEND,
	}
};

/* match is null-terminated but idstr is space-padded. */
static bool string_matches(const char *idstr, const char *match, size_t len)
{
	size_t matchlen;

	if (!match)
		return true;

	matchlen = strlen(match);
	WARN_ON_ONCE(matchlen > len);

	if (memcmp(idstr, match, matchlen))
		return false;

	for (; matchlen < len; matchlen++)
		if (idstr[matchlen] != ' ')
			return false;

	return true;
}

static bool quirk_matches(const struct nvme_id_ctrl *id,
			  const struct nvme_core_quirk_entry *q)
{
	return q->vid == le16_to_cpu(id->vid) &&
		string_matches(id->mn, q->mn, sizeof(id->mn)) &&
		string_matches(id->fr, q->fr, sizeof(id->fr));
}

static void nvmq_init_subnqn(struct nvmq_subsystem *subsys, struct nvmq_ctrl *ctrl,
		struct nvme_id_ctrl *id)
{
	size_t nqnlen;
	int off;

	if(!(ctrl->quirks & NVME_QUIRK_IGNORE_DEV_SUBNQN)) {
		nqnlen = strnlen(id->subnqn, NVMF_NQN_SIZE);
		if (nqnlen > 0 && nqnlen < NVMF_NQN_SIZE) {
			strlcpy(subsys->subnqn, id->subnqn, NVMF_NQN_SIZE);
			return;
		}

		if (ctrl->vs >= NVME_VS(1, 2, 1))
			dev_warn(ctrl->device, "missing or invalid SUBNQN field.\n");
	}

	/* Generate a "fake" NQN per Figure 254 in NVMe 1.3 + ECN 001 */
	off = snprintf(subsys->subnqn, NVMF_NQN_SIZE,
			"nqn.2014.08.org.nvmexpress:%04x%04x",
			le16_to_cpu(id->vid), le16_to_cpu(id->ssvid));
	memcpy(subsys->subnqn + off, id->sn, sizeof(id->sn));
	off += sizeof(id->sn);
	memcpy(subsys->subnqn + off, id->mn, sizeof(id->mn));
	off += sizeof(id->mn);
	memset(subsys->subnqn + off, 0, sizeof(subsys->subnqn) - off);
}

static void nvmq_release_subsystem(struct device *dev)
{
	struct nvmq_subsystem *subsys =
		container_of(dev, struct nvmq_subsystem, dev);

	if (subsys->instance >= 0)
		ida_simple_remove(&nvmq_instance_ida, subsys->instance);
	kfree(subsys);
}

static void nvmq_destroy_subsystem(struct kref *ref)
{
	struct nvmq_subsystem *subsys =
			container_of(ref, struct nvmq_subsystem, ref);

	mutex_lock(&nvmq_subsystems_lock);
	list_del(&subsys->entry);
	mutex_unlock(&nvmq_subsystems_lock);

	ida_destroy(&subsys->ns_ida);
	device_del(&subsys->dev);
	put_device(&subsys->dev);
}

static void nvmq_put_subsystem(struct nvmq_subsystem *subsys)
{
	kref_put(&subsys->ref, nvmq_destroy_subsystem);
}

static struct nvmq_subsystem *__nvmq_find_get_subsystem(const char *subsysnqn)
{
	struct nvmq_subsystem *subsys;

	lockdep_assert_held(&nvmq_subsystems_lock);

	/*
	 * Fail matches for discovery subsystems. This results
	 * in each discovery controller bound to a unique subsystem.
	 * This avoids issues with validating controller values
	 * that can only be true when there is a single unique subsystem.
	 * There may be multiple and completely independent entities
	 * that provide discovery controllers.
	 */
	if (!strcmp(subsysnqn, NVME_DISC_SUBSYS_NAME))
		return NULL;

	list_for_each_entry(subsys, &nvmq_subsystems, entry) {
		if (strcmp(subsys->subnqn, subsysnqn))
			continue;
		if (!kref_get_unless_zero(&subsys->ref))
			continue;
		return subsys;
	}

	return NULL;
}

#define SUBSYS_ATTR_RO(_name, _mode, _show)			\
	struct device_attribute subsys_attr_##_name = \
		__ATTR(_name, _mode, _show, NULL)

static ssize_t nvmq_subsys_show_nqn(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct nvmq_subsystem *subsys =
		container_of(dev, struct nvmq_subsystem, dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", subsys->subnqn);
}
static SUBSYS_ATTR_RO(subsysnqn, S_IRUGO, nvmq_subsys_show_nqn);

#define nvmq_subsys_show_str_function(field)				\
static ssize_t subsys_##field##_show(struct device *dev,		\
			    struct device_attribute *attr, char *buf)	\
{									\
	struct nvmq_subsystem *subsys =					\
		container_of(dev, struct nvmq_subsystem, dev);		\
	return sprintf(buf, "%.*s\n",					\
		       (int)sizeof(subsys->field), subsys->field);	\
}									\
static SUBSYS_ATTR_RO(field, S_IRUGO, subsys_##field##_show);

nvmq_subsys_show_str_function(model);
nvmq_subsys_show_str_function(serial);
nvmq_subsys_show_str_function(firmware_rev);

static struct attribute *nvmq_subsys_attrs[] = {
	&subsys_attr_model.attr,
	&subsys_attr_serial.attr,
	&subsys_attr_firmware_rev.attr,
	&subsys_attr_subsysnqn.attr,
#ifdef CONFIG_NVMQ_MULTIPATH
	&subsys_attr_iopolicy.attr,
#endif
	NULL,
};

static struct attribute_group nvmq_subsys_attrs_group = {
	.attrs = nvmq_subsys_attrs,
};

static const struct attribute_group *nvmq_subsys_attrs_groups[] = {
	&nvmq_subsys_attrs_group,
	NULL,
};

static bool nvmq_validate_cntlid(struct nvmq_subsystem *subsys,
		struct nvmq_ctrl *ctrl, struct nvme_id_ctrl *id)
{
	struct nvmq_ctrl *tmp;

	lockdep_assert_held(&nvmq_subsystems_lock);

	list_for_each_entry(tmp, &subsys->ctrls, subsys_entry) {
		if (tmp->state == NVME_CTRL_DELETING ||
		    tmp->state == NVME_CTRL_DEAD)
			continue;

		if (tmp->cntlid == ctrl->cntlid) {
			dev_err(ctrl->device,
				"Duplicate cntlid %u with %s, rejecting\n",
				ctrl->cntlid, dev_name(tmp->device));
			return false;
		}

		if ((id->cmic & (1 << 1)) ||
		    (ctrl->opts && ctrl->opts->discovery_nqn))
			continue;

		dev_err(ctrl->device,
			"Subsystem does not support multiple controllers\n");
		return false;
	}

	return true;
}

static int nvmq_init_subsystem(struct nvmq_ctrl *ctrl, struct nvme_id_ctrl *id)
{
	struct nvmq_subsystem *subsys, *found;
	int ret;

	subsys = kzalloc(sizeof(*subsys), GFP_KERNEL);
	if (!subsys)
		return -ENOMEM;

	subsys->instance = -1;
	mutex_init(&subsys->lock);
	kref_init(&subsys->ref);
	INIT_LIST_HEAD(&subsys->ctrls);
	INIT_LIST_HEAD(&subsys->nsheads);
	nvmq_init_subnqn(subsys, ctrl, id);
	memcpy(subsys->serial, id->sn, sizeof(subsys->serial));
	memcpy(subsys->model, id->mn, sizeof(subsys->model));
	memcpy(subsys->firmware_rev, id->fr, sizeof(subsys->firmware_rev));
	subsys->vendor_id = le16_to_cpu(id->vid);
	subsys->cmic = id->cmic;
	subsys->awupf = le16_to_cpu(id->awupf);
#ifdef CONFIG_NVMQ_MULTIPATH
	subsys->iopolicy = NVME_IOPOLICY_NUMA;
#endif

	subsys->dev.class = nvmq_subsys_class;
	subsys->dev.release = nvmq_release_subsystem;
	subsys->dev.groups = nvmq_subsys_attrs_groups;
	dev_set_name(&subsys->dev, "nvmq-subsys%d", ctrl->instance);
	device_initialize(&subsys->dev);

	mutex_lock(&nvmq_subsystems_lock);
	found = __nvmq_find_get_subsystem(subsys->subnqn);
	if (found) {
		put_device(&subsys->dev);
		subsys = found;

		if (!nvmq_validate_cntlid(subsys, ctrl, id)) {
			ret = -EINVAL;
			goto out_put_subsystem;
		}
	} else {
		ret = device_add(&subsys->dev);
		if (ret) {
			dev_err(ctrl->device,
				"failed to register subsystem device.\n");
			put_device(&subsys->dev);
			goto out_unlock;
		}
		ida_init(&subsys->ns_ida);
		list_add_tail(&subsys->entry, &nvmq_subsystems);
	}

	ret = sysfs_create_link(&subsys->dev.kobj, &ctrl->device->kobj,
				dev_name(ctrl->device));
	if (ret) {
		dev_err(ctrl->device,
			"failed to create sysfs link from subsystem.\n");
		goto out_put_subsystem;
	}

	if (!found)
		subsys->instance = ctrl->instance;
	ctrl->subsys = subsys;
	list_add_tail(&ctrl->subsys_entry, &subsys->ctrls);
	mutex_unlock(&nvmq_subsystems_lock);
	return 0;

out_put_subsystem:
	nvmq_put_subsystem(subsys);
out_unlock:
	mutex_unlock(&nvmq_subsystems_lock);
	return ret;
}

int nvmq_get_log(struct nvmq_ctrl *ctrl, u32 nsid, u8 log_page, u8 lsp,
		void *log, size_t size, u64 offset)
{
	struct nvme_command c = { };
	unsigned long dwlen = size / 4 - 1;

	c.get_log_page.opcode = nvme_admin_get_log_page;
	c.get_log_page.nsid = cpu_to_le32(nsid);
	c.get_log_page.lid = log_page;
	c.get_log_page.lsp = lsp;
	c.get_log_page.numdl = cpu_to_le16(dwlen & ((1 << 16) - 1));
	c.get_log_page.numdu = cpu_to_le16(dwlen >> 16);
	c.get_log_page.lpol = cpu_to_le32(lower_32_bits(offset));
	c.get_log_page.lpou = cpu_to_le32(upper_32_bits(offset));

	return nvmq_submit_sync_cmd(ctrl->admin_q, &c, log, size);
}

static int nvmq_get_effects_log(struct nvmq_ctrl *ctrl)
{
	int ret;

	if (!ctrl->effects)
		ctrl->effects = kzalloc(sizeof(*ctrl->effects), GFP_KERNEL);

	if (!ctrl->effects)
		return 0;

	ret = nvmq_get_log(ctrl, NVME_NSID_ALL, NVME_LOG_CMD_EFFECTS, 0,
			ctrl->effects, sizeof(*ctrl->effects), 0);
	if (ret) {
		kfree(ctrl->effects);
		ctrl->effects = NULL;
	}
	return ret;
}

/*
 * Initialize the cached copies of the Identify data and various controller
 * register in our nvmq_ctrl structure.  This should be called as soon as
 * the admin queue is fully up and running.
 */
int nvmq_init_identify(struct nvmq_ctrl *ctrl)
{
	struct nvme_id_ctrl *id;
	int ret, page_shift;
	u32 max_hw_sectors;
	bool prev_apst_enabled;

	ret = ctrl->ops->reg_read32(ctrl, NVME_REG_VS, &ctrl->vs);
	if (ret) {
		dev_err(ctrl->device, "Reading VS failed (%d)\n", ret);
		return ret;
	}
	page_shift = NVME_CAP_MPSMIN(ctrl->cap) + 12;
	ctrl->sqsize = min_t(int, NVME_CAP_MQES(ctrl->cap), ctrl->sqsize);

	if (ctrl->vs >= NVME_VS(1, 1, 0))
		ctrl->subsystem = NVME_CAP_NSSRC(ctrl->cap);

	ret = nvmq_identify_ctrl(ctrl, &id);
	if (ret) {
		dev_err(ctrl->device, "Identify Controller failed (%d)\n", ret);
		return -EIO;
	}

	if (id->lpa & NVME_CTRL_LPA_CMD_EFFECTS_LOG) {
		ret = nvmq_get_effects_log(ctrl);
		if (ret < 0)
			goto out_free;
	}

	if (!(ctrl->ops->flags & NVME_F_FABRICS))
		ctrl->cntlid = le16_to_cpu(id->cntlid);

	if (!ctrl->identified) {
		int i;

		ret = nvmq_init_subsystem(ctrl, id);
		if (ret)
			goto out_free;

		/*
		 * Check for quirks.  Quirk can depend on firmware version,
		 * so, in principle, the set of quirks present can change
		 * across a reset.  As a possible future enhancement, we
		 * could re-scan for quirks every time we reinitialize
		 * the device, but we'd have to make sure that the driver
		 * behaves intelligently if the quirks change.
		 */
		for (i = 0; i < ARRAY_SIZE(core_quirks); i++) {
			if (quirk_matches(id, &core_quirks[i]))
				ctrl->quirks |= core_quirks[i].quirks;
		}
	}

	if (force_apst && (ctrl->quirks & NVME_QUIRK_NO_DEEPEST_PS)) {
		dev_warn(ctrl->device, "forcibly allowing all power states due to nvme_core.force_apst -- use at your own risk\n");
		ctrl->quirks &= ~NVME_QUIRK_NO_DEEPEST_PS;
	}

	ctrl->crdt[0] = le16_to_cpu(id->crdt1);
	ctrl->crdt[1] = le16_to_cpu(id->crdt2);
	ctrl->crdt[2] = le16_to_cpu(id->crdt3);

	ctrl->oacs = le16_to_cpu(id->oacs);
	ctrl->oncs = le16_to_cpu(id->oncs);
	ctrl->mtfa = le16_to_cpu(id->mtfa);
	ctrl->oaes = le32_to_cpu(id->oaes);
	atomic_set(&ctrl->abort_limit, id->acl + 1);
	ctrl->vwc = id->vwc;
	if (id->mdts)
		max_hw_sectors = 1 << (id->mdts + page_shift - 9);
	else
		max_hw_sectors = UINT_MAX;
	ctrl->max_hw_sectors =
		min_not_zero(ctrl->max_hw_sectors, max_hw_sectors);

	nvmq_set_queue_limits(ctrl, ctrl->admin_q);
	ctrl->sgls = le32_to_cpu(id->sgls);
	ctrl->kas = le16_to_cpu(id->kas);
	ctrl->max_namespaces = le32_to_cpu(id->mnan);
	ctrl->ctratt = le32_to_cpu(id->ctratt);

	if (id->rtd3e) {
		/* us -> s */
		u32 transition_time = le32_to_cpu(id->rtd3e) / 1000000;

		ctrl->shutdown_timeout = clamp_t(unsigned int, transition_time,
						 shutdown_timeout, 60);

		if (ctrl->shutdown_timeout != shutdown_timeout)
			dev_info(ctrl->device,
				 "Shutdown timeout set to %u seconds\n",
				 ctrl->shutdown_timeout);
	} else
		ctrl->shutdown_timeout = shutdown_timeout;

	ctrl->npss = id->npss;
	ctrl->apsta = id->apsta;
	prev_apst_enabled = ctrl->apst_enabled;
	if (ctrl->quirks & NVME_QUIRK_NO_APST) {
		if (force_apst && id->apsta) {
			dev_warn(ctrl->device, "forcibly allowing APST due to nvme_core.force_apst -- use at your own risk\n");
			ctrl->apst_enabled = true;
		} else {
			ctrl->apst_enabled = false;
		}
	} else {
		ctrl->apst_enabled = id->apsta;
	}
	memcpy(ctrl->psd, id->psd, sizeof(ctrl->psd));

	if (ctrl->ops->flags & NVME_F_FABRICS) {
		ctrl->icdoff = le16_to_cpu(id->icdoff);
		ctrl->ioccsz = le32_to_cpu(id->ioccsz);
		ctrl->iorcsz = le32_to_cpu(id->iorcsz);
		ctrl->maxcmd = le16_to_cpu(id->maxcmd);

		/*
		 * In fabrics we need to verify the cntlid matches the
		 * admin connect
		 */
		if (ctrl->cntlid != le16_to_cpu(id->cntlid)) {
			ret = -EINVAL;
			goto out_free;
		}

		if (!ctrl->opts->discovery_nqn && !ctrl->kas) {
			dev_err(ctrl->device,
				"keep-alive support is mandatory for fabrics\n");
			ret = -EINVAL;
			goto out_free;
		}
	} else {
		ctrl->hmpre = le32_to_cpu(id->hmpre);
		ctrl->hmmin = le32_to_cpu(id->hmmin);
		ctrl->hmminds = le32_to_cpu(id->hmminds);
		ctrl->hmmaxd = le16_to_cpu(id->hmmaxd);
	}

	ret = nvmq_mpath_init_identify(ctrl, id);
	kfree(id);

	if (ret < 0)
		return ret;

	if (ctrl->apst_enabled && !prev_apst_enabled)
		dev_pm_qos_expose_latency_tolerance(ctrl->device);
	else if (!ctrl->apst_enabled && prev_apst_enabled)
		dev_pm_qos_hide_latency_tolerance(ctrl->device);

	ret = nvmq_configure_apst(ctrl);
	if (ret < 0)
		return ret;
	
	ret = nvmq_configure_timestamp(ctrl);
	if (ret < 0)
		return ret;

	ret = nvmq_configure_directives(ctrl);
	if (ret < 0)
		return ret;

	ret = nvmq_configure_acre(ctrl);
	if (ret < 0)
		return ret;

	ctrl->identified = true;

	return 0;

out_free:
	kfree(id);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmq_init_identify);

static int nvmq_dev_open(struct inode *inode, struct file *file)
{
	struct nvmq_ctrl *ctrl =
		container_of(inode->i_cdev, struct nvmq_ctrl, cdev);

	switch (ctrl->state) {
	case NVME_CTRL_LIVE:
		break;
	default:
		return -EWOULDBLOCK;
	}

	nvmq_get_ctrl(ctrl);
	if (!try_module_get(ctrl->ops->module)) {
		nvmq_put_ctrl(ctrl);
		return -EINVAL;
	}

	file->private_data = ctrl;
	return 0;
}

static int nvmq_dev_release(struct inode *inode, struct file *file)
{
	struct nvmq_ctrl *ctrl =
		container_of(inode->i_cdev, struct nvmq_ctrl, cdev);

	module_put(ctrl->ops->module);
	nvmq_put_ctrl(ctrl);
	return 0;
}

static int nvmq_dev_user_cmd(struct nvmq_ctrl *ctrl, void __user *argp)
{
	struct nvmq_ns *ns;
	int ret;

	down_read(&ctrl->namespaces_rwsem);
	if (list_empty(&ctrl->namespaces)) {
		ret = -ENOTTY;
		goto out_unlock;
	}

	ns = list_first_entry(&ctrl->namespaces, struct nvmq_ns, list);
	if (ns != list_last_entry(&ctrl->namespaces, struct nvmq_ns, list)) {
		dev_warn(ctrl->device,
			"NVME_IOCTL_IO_CMD not supported when multiple namespaces present!\n");
		ret = -EINVAL;
		goto out_unlock;
	}

	dev_warn(ctrl->device,
		"using deprecated NVME_IOCTL_IO_CMD ioctl on the char device!\n");
	kref_get(&ns->kref);
	up_read(&ctrl->namespaces_rwsem);

	ret = nvmq_user_cmd(ctrl, ns, argp);
	nvmq_put_ns(ns);
	return ret;

out_unlock:
	up_read(&ctrl->namespaces_rwsem);
	return ret;
}

static long nvmq_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct nvmq_ctrl *ctrl = file->private_data;
	void __user *argp = (void __user *)arg;

	pr_debug("Got IOCTL \n");
	switch (cmd) {
	case NVME_IOCTL_ADMIN_CMD:
		return nvmq_user_cmd(ctrl, NULL, argp);
	case NVME_IOCTL_ADMIN64_CMD:
		return nvmq_user_cmd64(ctrl, NULL, argp);
	case NVME_IOCTL_IO_CMD:
		return nvmq_dev_user_cmd(ctrl, argp);
	case NVME_IOCTL_KERNEL:
		// ret = nvmq_user_kernel(ctrl, ns, argp);
		pr_err("IOCTL on dev not supported\n");
		return 0;
		break;
	case NVME_IOCTL_RESET:
		dev_warn(ctrl->device, "resetting controller\n");
		return nvmq_reset_ctrl_sync(ctrl);
	case NVME_IOCTL_SUBSYS_RESET:
		return nvmq_reset_subsystem(ctrl);
	case NVME_IOCTL_RESCAN:
		nvmq_queue_scan(ctrl);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations nvmq_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= nvmq_dev_open,
	.release	= nvmq_dev_release,
	.unlocked_ioctl	= nvmq_dev_ioctl,
	.compat_ioctl	= nvmq_dev_ioctl,
};

static inline bool nvmq_get_ns(struct nvmq_ns *ns){
	return kref_get_unless_zero(&ns->kref);
}




static int nvmq_ns_chr_open(struct inode *inode, struct file *file)
{
	//根据cdev的地址反推nvme_ns的地址
	struct nvmq_ns* ns = (container_of(inode->i_cdev, struct nvmq_ns, cdev));
	/*should never be called due to GENHD_FL_HIDDEN*/
	file->private_data = bdget_disk(ns->disk, 0);
	pr_debug("OPEN NS CHR DEVICE");
	if (!nvmq_get_ns(ns))
		goto fail;
	if (!try_module_get(ns->ctrl->ops->module))
		goto fail_put_ns;

	return 0;
fail_put_ns:
	pr_debug("FAILED TO PUT NS");
	nvmq_put_ns(ns);
fail:
	return -ENXIO;
}

static int nvmq_ns_chr_release(struct inode *inode, struct file *file)
{
	struct nvmq_ns* ns = (container_of(inode->i_cdev, struct nvmq_ns, cdev));
	
	module_put(ns->ctrl->ops->module);
	nvmq_put_ns(ns);
	return 0;
}



static void async_ns_chr_work_handler(struct work_struct* work)
{
	struct async_ns_chr_task *task = container_of(work, struct async_ns_chr_task, work);
	int ret = 0;

	char *kbuf = NULL;
	if(task->count < sizeof(struct nvme_command)){
		ret = -EFAULT;
		goto complete;
	}
	
	struct nvmq_ns* ns = task->ns;
	if(ns==NULL){
		pr_err("Error get wrong namespace\n");
	}
	int res;
	bool write = task->write;
	struct gendisk *disk = ns ? ns->disk : NULL;
	struct request *req = task->req;
	struct bio *bio = task->bio;
	void *meta = task->meta;
	if(req==NULL){
		pr_err("Error get wrong bio or req\n");
	}
	blk_execute_rq(req->q, disk, req, 0);
	if (nvmq_req(req)->flags & NVME_REQ_CANCELLED)
		ret = -EINTR;
	else
		ret = nvmq_req(req)->status;
	if(ret>=0){
		struct nvme_passthru_cmd* cmd = (struct nvme_passthru_cmd*)(((unsigned long)(task->kernel_buf)) | (((unsigned long)(task->user_buf))&(~PAGE_MASK)));
		cmd->result = le64_to_cpu(nvmq_knl(req)->result.u64);
		vunmap(task->kernel_buf);
		put_page((task->buf_pages[0]));
	}
	if(meta!=NULL);
		kfree(meta);
 out_unmap:
	if (bio)
		blk_rq_unmap_user(bio);
 out:
	blk_mq_free_request(req);
	/* 成功返回实际传输的字节数 */
	ret = ret<0? ret : task->count;

complete:
	/* 完成异步 I/O 请求，通知 io_uring 层 */
	task->iocb->ki_complete(task->iocb, ret, 0);
	kfree(task);
} 

static ssize_t nvmq_ns_chr_read_iter(struct kiocb *iocb,struct iov_iter *iter){
	struct async_ns_chr_task *task;
	size_t count = iov_iter_count(iter);
	if(count < sizeof(struct nvme_command)||iter->nr_segs!=1){
		pr_err("%s: Failed nr_segs or count\n",__func__);
		return -EINVAL;
	}
	task = kmalloc(sizeof(*task),GFP_KERNEL);
	if(!task)
		return -ENOMEM;
	task->iocb = iocb;
	task->is_read = true;
	task->count = count;
	task->user_buf = ((struct iovec*)iter->iov)[0].iov_base;
	struct file* filep = iocb->ki_filp;
	struct nvmq_ns* ns =(container_of(filep->f_inode->i_cdev, struct nvmq_ns, cdev));
	INIT_WORK(&task->work,async_ns_chr_work_handler);
	task->ns = ns;
	int ret = nvmq_user_uring_cmd(ns->ctrl,ns,(struct nvme_passthru_cmd __user *)task->user_buf,task);
	if(ret<0) return ret;
	queue_work(ns->nvme_wq,&(task->work));
	return -EIOCBQUEUED;
}

static ssize_t nvmq_ns_chr_write_iter(struct kiocb *iocb,struct iov_iter *iter){
	struct async_ns_chr_task *task;
	size_t count = iov_iter_count(iter);
	if(count < sizeof(struct nvme_command)||iter->nr_segs!=1){
		pr_err("%s: Failed nr_segs or count\n",__func__);
		return -EINVAL;
	}
	task = kmalloc(sizeof(struct async_ns_chr_task),GFP_KERNEL);
	if(!task)
		return -ENOMEM;
	task->buf_pages = kmalloc(sizeof(struct page*),GFP_KERNEL);
	if(!task->buf_pages)
		return -ENOMEM;
	task->iocb = iocb;
	task->is_read = false;
	task->count = count;
	task->user_buf = ((struct iovec*)iter->iov)[0].iov_base;
	struct file* filep = iocb->ki_filp;
	pr_debug("base address%llx\n",task->user_buf);
	struct nvme_passthru_cmd cmd;
	if(copy_from_user((&cmd),((struct iovec*)iter->iov)[0].iov_base,sizeof(struct nvme_passthru_cmd))){
		return -EFAULT;
	}
	pr_debug("Opocde:%d\n cdw10:%llx\ncdw11:%llx\ncdw12:%llx addr len%d addr%llx",cmd.opcode,cmd.cdw10,cmd.cdw11,cmd.cdw12,cmd.data_len,cmd.addr);
	struct nvmq_ns* ns =(container_of(filep->f_inode->i_cdev, struct nvmq_ns, cdev));
	INIT_WORK(&task->work,async_ns_chr_work_handler);
	task->ns = ns;
	int ret = nvmq_user_uring_cmd(ns->ctrl,ns,(struct nvme_passthru_cmd __user *)task->user_buf,task);
	if(ret<0) return ret;
	
	//Map Data
	ret = get_user_pages_fast(((unsigned long)(task->user_buf))&PAGE_MASK,1,FOLL_WRITE,task->buf_pages);
	
	if(ret < 0){
		pr_err("Failed to pin nvme command pages!\n");
	}else if(ret < 1){
		pr_err("Fake successed! pages get is not one\n");
		put_page(task->buf_pages[0]);
		return -EFAULT;
	}
	task->kernel_buf = vmap(task->buf_pages,1,VM_MAP,PAGE_KERNEL);
	if(!task->kernel_buf){
		pr_err("Failed to map to kernel space\n");
		return -ENOMEM;
	}
	queue_work(ns->nvme_wq,&(task->work));

	
	return -EIOCBQUEUED;
}



static ssize_t nvmq_ns_chr_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *pos){
	//struct nvme_passthru_cmd cmd;
	 // 保存当前地址访问模式
	
	 // 设置为用户空间访问模式
	struct nvmq_ns* ns =(container_of(file->f_inode->i_cdev, struct nvmq_ns, cdev));
	mm_segment_t old_fs;
	old_fs = get_fs();
	struct block_device* bdev = file->private_data;
	set_fs(USER_DS);
	int res = __blkdev_driver_ioctl(bdev,0,NVME_IOCTL_IO_CMD,(unsigned long)ubuf);
	set_fs(old_fs);
	// 恢复原来的地址访问模式

	return count;
}

static ssize_t nvmq_ns_chr_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *pos){
	struct nvme_passthru_cmd cmd;
	pr_debug("%s enter ppos%d count%d \n",__func__,*pos,count);
	copy_from_user((&cmd)+*pos,ubuf,sizeof(struct nvme_passthru_cmd));
	char __user *writable_buf = (char __user *)ubuf;
	pr_debug("Opocde:%d\n cdw10:%llx\ncdw11:%llx\ncdw12:%llx",cmd.opcode,cmd.cdw10,cmd.cdw11,cmd.cdw12);
	struct nvmq_ns* ns = (container_of(file->f_inode->i_cdev, struct nvmq_ns, cdev));
	nvmq_user_cmd(ns->ctrl,ns,(struct nvme_passthru_cmd __user *)writable_buf);
	return count;
}

static const struct file_operations nvmq_ns_chr_fops = {
	.owner		= THIS_MODULE,
	.open		= nvmq_ns_chr_open,
	.release	= nvmq_ns_chr_release,
	.write 		= nvmq_ns_chr_write,
	.read 		= nvmq_ns_chr_read,
	.write_iter = nvmq_ns_chr_write_iter,
	.read_iter = nvmq_ns_chr_read_iter
	};

static void nvmq_cdev_rel(struct device* dev){
	ida_free(&nvmq_ns_chr_minor_ida,MINOR(dev->devt));
}

void nvmq_cdev_del(struct cdev* cdev,struct device* cdev_device){

	cdev_device_del(cdev,cdev_device);
	put_device(cdev_device);
}

int nvmq_cdev_add(struct cdev *cdev,struct device *cdev_device,
const struct file_operations *fops,struct module *owner){
	int minor, ret;
	//Allocating Identifiers
	minor = ida_alloc(&nvmq_ns_chr_minor_ida, GFP_KERNEL);
	if (minor < 0){
		pr_err("CREATE NS CHR DEVICE FAILED!\nMINOR LESS THAN 0!\n");
		return minor;
	}
	
	cdev_device->devt = MKDEV(MAJOR(nvmq_ns_chr_devt), minor);
	cdev_device->class = nvmq_ns_chr_class;
	cdev_device->release = nvmq_cdev_rel;
	device_initialize(cdev_device);
	cdev_init(cdev, fops);
	cdev->owner = owner;
	ret = cdev_device_add(cdev, cdev_device);
	if (ret)
		put_device(cdev_device);
	return ret;
}

static int nvmq_add_ns_cdev(struct nvmq_ns* ns){
	int ret;
	ns->cdev_device.parent = ns->ctrl->device;
	ret = dev_set_name(&(ns->cdev_device),"ng%dn%d",
	ns->ctrl->instance,ns->head->instance);
	char wq_name[100];
	sprintf(wq_name,"async_wq_ng%dn%d",ns->ctrl->instance,ns->head->instance);
	ns->nvme_wq = alloc_workqueue(wq_name,WQ_HIGHPRI|WQ_UNBOUND ,0);
	pr_debug("Create NS CDEV NAME %s\n",dev_name(&(ns->cdev_device)));
	if(ret)
		return ret;	
	ret =  nvmq_cdev_add(&ns->cdev,&ns->cdev_device,
	&nvmq_ns_chr_fops,ns->ctrl->ops->module);
	pr_debug("Major number = %d,minor number = %d\n",MAJOR(ns->cdev_device.devt),MINOR(ns->cdev_device.devt));
	return ret;
}

static ssize_t nvmq_sysfs_reset(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);
	int ret;

	ret = nvmq_reset_ctrl_sync(ctrl);
	if (ret < 0)
		return ret;
	return count;
}
static DEVICE_ATTR(reset_controller, S_IWUSR, NULL, nvmq_sysfs_reset);

static ssize_t nvmq_sysfs_rescan(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	nvmq_queue_scan(ctrl);
	return count;
}
static DEVICE_ATTR(rescan_controller, S_IWUSR, NULL, nvmq_sysfs_rescan);

static inline struct nvmq_ns_head *dev_to_ns_head(struct device *dev)
{
	struct gendisk *disk = dev_to_disk(dev);

	if (disk->fops == &nvmq_fops)
		return nvmq_get_ns_from_dev(dev)->head;
	else
		return disk->private_data;
}

static ssize_t wwid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct nvmq_ns_head *head = dev_to_ns_head(dev);
	struct nvmq_ns_ids *ids = &head->ids;
	struct nvmq_subsystem *subsys = head->subsys;
	int serial_len = sizeof(subsys->serial);
	int model_len = sizeof(subsys->model);

	if (!uuid_is_null(&ids->uuid))
		return sprintf(buf, "uuid.%pU\n", &ids->uuid);

	if (memchr_inv(ids->nguid, 0, sizeof(ids->nguid)))
		return sprintf(buf, "eui.%16phN\n", ids->nguid);

	if (memchr_inv(ids->eui64, 0, sizeof(ids->eui64)))
		return sprintf(buf, "eui.%8phN\n", ids->eui64);

	while (serial_len > 0 && (subsys->serial[serial_len - 1] == ' ' ||
				  subsys->serial[serial_len - 1] == '\0'))
		serial_len--;
	while (model_len > 0 && (subsys->model[model_len - 1] == ' ' ||
				 subsys->model[model_len - 1] == '\0'))
		model_len--;

	return sprintf(buf, "nvmq.%04x-%*phN-%*phN-%08x\n", subsys->vendor_id,
		serial_len, subsys->serial, model_len, subsys->model,
		head->ns_id);
}
static DEVICE_ATTR_RO(wwid);

static ssize_t nguid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%pU\n", dev_to_ns_head(dev)->ids.nguid);
}
static DEVICE_ATTR_RO(nguid);

static ssize_t uuid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct nvmq_ns_ids *ids = &dev_to_ns_head(dev)->ids;

	/* For backward compatibility expose the NGUID to userspace if
	 * we have no UUID set
	 */
	if (uuid_is_null(&ids->uuid)) {
		printk_ratelimited(KERN_WARNING
				   "No UUID available providing old NGUID\n");
		return sprintf(buf, "%pU\n", ids->nguid);
	}
	return sprintf(buf, "%pU\n", &ids->uuid);
}
static DEVICE_ATTR_RO(uuid);

static ssize_t eui_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%8ph\n", dev_to_ns_head(dev)->ids.eui64);
}
static DEVICE_ATTR_RO(eui);

static ssize_t nsid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", dev_to_ns_head(dev)->ns_id);
}
static DEVICE_ATTR_RO(nsid);

static struct attribute *nvmq_ns_id_attrs[] = {
	&dev_attr_wwid.attr,
	&dev_attr_uuid.attr,
	&dev_attr_nguid.attr,
	&dev_attr_eui.attr,
	&dev_attr_nsid.attr,
#ifdef CONFIG_NVMQ_MULTIPATH
	&dev_attr_ana_grpid.attr,
	&dev_attr_ana_state.attr,
#endif
	NULL,
};

static umode_t nvmq_ns_id_attrs_are_visible(struct kobject *kobj,
		struct attribute *a, int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct nvmq_ns_ids *ids = &dev_to_ns_head(dev)->ids;

	if (a == &dev_attr_uuid.attr) {
		if (uuid_is_null(&ids->uuid) &&
		    !memchr_inv(ids->nguid, 0, sizeof(ids->nguid)))
			return 0;
	}
	if (a == &dev_attr_nguid.attr) {
		if (!memchr_inv(ids->nguid, 0, sizeof(ids->nguid)))
			return 0;
	}
	if (a == &dev_attr_eui.attr) {
		if (!memchr_inv(ids->eui64, 0, sizeof(ids->eui64)))
			return 0;
	}
#ifdef CONFIG_NVMQ_MULTIPATH
	if (a == &dev_attr_ana_grpid.attr || a == &dev_attr_ana_state.attr) {
		if (dev_to_disk(dev)->fops != &nvmq_fops) /* per-path attr */
			return 0;
		if (!nvmq_ctrl_use_ana(nvmq_get_ns_from_dev(dev)->ctrl))
			return 0;
	}
#endif
	return a->mode;
}

static const struct attribute_group nvmq_ns_id_attr_group = {
	.attrs		= nvmq_ns_id_attrs,
	.is_visible	= nvmq_ns_id_attrs_are_visible,
};

const struct attribute_group *nvmq_ns_id_attr_groups[] = {
	&nvmq_ns_id_attr_group,
#ifdef CONFIG_NVM
	&nvme_nvm_attr_group,
#endif
	NULL,
};

#define nvmq_show_str_function(field)						\
static ssize_t  field##_show(struct device *dev,				\
			    struct device_attribute *attr, char *buf)		\
{										\
        struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);				\
        return sprintf(buf, "%.*s\n",						\
		(int)sizeof(ctrl->subsys->field), ctrl->subsys->field);		\
}										\
static DEVICE_ATTR(field, S_IRUGO, field##_show, NULL);

nvmq_show_str_function(model);
nvmq_show_str_function(serial);
nvmq_show_str_function(firmware_rev);

#define nvmq_show_int_function(field)						\
static ssize_t  field##_show(struct device *dev,				\
			    struct device_attribute *attr, char *buf)		\
{										\
        struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);				\
        return sprintf(buf, "%d\n", ctrl->field);	\
}										\
static DEVICE_ATTR(field, S_IRUGO, field##_show, NULL);

nvmq_show_int_function(cntlid);
nvmq_show_int_function(numa_node);
nvmq_show_int_function(queue_count);
nvmq_show_int_function(sqsize);

static ssize_t nvmq_sysfs_delete(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	/* Can't delete non-created controllers */
	if (!ctrl->created)
		return -EBUSY;

	if (device_remove_file_self(dev, attr))
		nvmq_delete_ctrl_sync(ctrl);
	return count;
}
static DEVICE_ATTR(delete_controller, S_IWUSR, NULL, nvmq_sysfs_delete);

static ssize_t nvmq_sysfs_show_transport(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", ctrl->ops->name);
}
static DEVICE_ATTR(transport, S_IRUGO, nvmq_sysfs_show_transport, NULL);

static ssize_t nvmq_sysfs_show_state(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);
	static const char *const state_name[] = {
		[NVME_CTRL_NEW]		= "new",
		[NVME_CTRL_LIVE]	= "live",
		[NVME_CTRL_RESETTING]	= "resetting",
		[NVME_CTRL_CONNECTING]	= "connecting",
		[NVME_CTRL_DELETING]	= "deleting",
		[NVME_CTRL_DEAD]	= "dead",
	};

	if ((unsigned)ctrl->state < ARRAY_SIZE(state_name) &&
	    state_name[ctrl->state])
		return sprintf(buf, "%s\n", state_name[ctrl->state]);

	return sprintf(buf, "unknown state\n");
}

static DEVICE_ATTR(state, S_IRUGO, nvmq_sysfs_show_state, NULL);

static ssize_t nvmq_sysfs_show_subsysnqn(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", ctrl->subsys->subnqn);
}
static DEVICE_ATTR(subsysnqn, S_IRUGO, nvmq_sysfs_show_subsysnqn, NULL);

static ssize_t nvmq_sysfs_show_address(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	return ctrl->ops->get_address(ctrl, buf, PAGE_SIZE);
}
static DEVICE_ATTR(address, S_IRUGO, nvmq_sysfs_show_address, NULL);

static struct attribute *nvmq_dev_attrs[] = {
	&dev_attr_reset_controller.attr,
	&dev_attr_rescan_controller.attr,
	&dev_attr_model.attr,
	&dev_attr_serial.attr,
	&dev_attr_firmware_rev.attr,
	&dev_attr_cntlid.attr,
	&dev_attr_delete_controller.attr,
	&dev_attr_transport.attr,
	&dev_attr_subsysnqn.attr,
	&dev_attr_address.attr,
	&dev_attr_state.attr,
	&dev_attr_numa_node.attr,
	&dev_attr_queue_count.attr,
	&dev_attr_sqsize.attr,
	NULL
};

static umode_t nvmq_dev_attrs_are_visible(struct kobject *kobj,
		struct attribute *a, int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct nvmq_ctrl *ctrl = dev_get_drvdata(dev);

	if (a == &dev_attr_delete_controller.attr && !ctrl->ops->delete_ctrl)
		return 0;
	if (a == &dev_attr_address.attr && !ctrl->ops->get_address)
		return 0;

	return a->mode;
}

static struct attribute_group nvmq_dev_attrs_group = {
	.attrs		= nvmq_dev_attrs,
	.is_visible	= nvmq_dev_attrs_are_visible,
};

static const struct attribute_group *nvmq_dev_attr_groups[] = {
	&nvmq_dev_attrs_group,
	NULL,
};

static struct nvmq_ns_head *__nvmq_find_ns_head(struct nvmq_subsystem *subsys,
		unsigned nsid)
{
	struct nvmq_ns_head *h;

	lockdep_assert_held(&subsys->lock);

	list_for_each_entry(h, &subsys->nsheads, entry) {
		if (h->ns_id == nsid && kref_get_unless_zero(&h->ref))
			return h;
	}

	return NULL;
}

static int __nvmq_check_ids(struct nvmq_subsystem *subsys,
		struct nvmq_ns_head *new)
{
	struct nvmq_ns_head *h;

	lockdep_assert_held(&subsys->lock);

	list_for_each_entry(h, &subsys->nsheads, entry) {
		if (nvmq_ns_ids_valid(&new->ids) &&
		    nvmq_ns_ids_equal(&new->ids, &h->ids))
			return -EINVAL;
	}

	return 0;
}

static struct nvmq_ns_head *nvmq_alloc_ns_head(struct nvmq_ctrl *ctrl,
		unsigned nsid, struct nvme_id_ns *id)
{
	struct nvmq_ns_head *head;
	size_t size = sizeof(*head);
	int ret = -ENOMEM;

#ifdef CONFIG_NVMQ_MULTIPATH
	size += num_possible_nodes() * sizeof(struct nvmq_ns *);
#endif

	head = kzalloc(size, GFP_KERNEL);
	if (!head)
		goto out;
	ret = ida_simple_get(&ctrl->subsys->ns_ida, 1, 0, GFP_KERNEL);
	if (ret < 0)
		goto out_free_head;
	head->instance = ret;
	INIT_LIST_HEAD(&head->list);
	ret = init_srcu_struct(&head->srcu);
	if (ret)
		goto out_ida_remove;
	head->subsys = ctrl->subsys;
	head->ns_id = nsid;
	kref_init(&head->ref);

	ret = nvmq_report_ns_ids(ctrl, nsid, id, &head->ids);
	if (ret)
		goto out_cleanup_srcu;

	ret = __nvmq_check_ids(ctrl->subsys, head);
	if (ret) {
		dev_err(ctrl->device,
			"duplicate IDs for nsid %d\n", nsid);
		goto out_cleanup_srcu;
	}

	ret = nvmq_mpath_alloc_disk(ctrl, head);
	if (ret)
		goto out_cleanup_srcu;

	list_add_tail(&head->entry, &ctrl->subsys->nsheads);

	kref_get(&ctrl->subsys->ref);

	return head;
out_cleanup_srcu:
	cleanup_srcu_struct(&head->srcu);
out_ida_remove:
	ida_simple_remove(&ctrl->subsys->ns_ida, head->instance);
out_free_head:
	kfree(head);
out:
	if (ret > 0)
		ret = blk_status_to_errno(nvmq_error_status(ret));
	return ERR_PTR(ret);
}

static int nvmq_init_ns_head(struct nvmq_ns *ns, unsigned nsid,
		struct nvme_id_ns *id)
{
	struct nvmq_ctrl *ctrl = ns->ctrl;
	bool is_shared = id->nmic & (1 << 0);
	struct nvmq_ns_head *head = NULL;
	int ret = 0;

	mutex_lock(&ctrl->subsys->lock);
	if (is_shared)
		head = __nvmq_find_ns_head(ctrl->subsys, nsid);
	if (!head) {
		head = nvmq_alloc_ns_head(ctrl, nsid, id);
		if (IS_ERR(head)) {
			ret = PTR_ERR(head);
			goto out_unlock;
		}
	} else {
		struct nvmq_ns_ids ids;

		ret = nvmq_report_ns_ids(ctrl, nsid, id, &ids);
		if (ret)
			goto out_unlock;

		if (!nvmq_ns_ids_equal(&head->ids, &ids)) {
			dev_err(ctrl->device,
				"IDs don't match for shared namespace %d\n",
					nsid);
			ret = -EINVAL;
			nvmq_put_ns_head(head);
			goto out_unlock;
		}
	}

	list_add_tail(&ns->siblings, &head->list);
	ns->head = head;

out_unlock:
	mutex_unlock(&ctrl->subsys->lock);
	if (ret > 0)
		ret = blk_status_to_errno(nvmq_error_status(ret));
	return ret;
}

static int ns_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct nvmq_ns *nsa = container_of(a, struct nvmq_ns, list);
	struct nvmq_ns *nsb = container_of(b, struct nvmq_ns, list);

	return nsa->head->ns_id - nsb->head->ns_id;
}

static struct nvmq_ns *nvmq_find_get_ns(struct nvmq_ctrl *ctrl, unsigned nsid)
{
	struct nvmq_ns *ns, *ret = NULL;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list) {
		if (ns->head->ns_id == nsid) {
			if (!kref_get_unless_zero(&ns->kref))
				continue;
			ret = ns;
			break;
		}
		if (ns->head->ns_id > nsid)
			break;
	}
	up_read(&ctrl->namespaces_rwsem);
	return ret;
}

static int nvmq_setup_streams_ns(struct nvmq_ctrl *ctrl, struct nvmq_ns *ns)
{
	struct streams_directive_params s;
	int ret;

	if (!ctrl->nr_streams)
		return 0;

	ret = nvmq_get_stream_params(ctrl, &s, ns->head->ns_id);
	if (ret)
		return ret;

	ns->sws = le32_to_cpu(s.sws);
	ns->sgs = le16_to_cpu(s.sgs);

	if (ns->sws) {
		unsigned int bs = 1 << ns->lba_shift;

		blk_queue_io_min(ns->queue, bs * ns->sws);
		if (ns->sgs)
			blk_queue_io_opt(ns->queue, bs * ns->sws * ns->sgs);
	}

	return 0;
}

static int nvmq_alloc_ns(struct nvmq_ctrl *ctrl, unsigned nsid)
{
	struct nvmq_ns *ns;
	struct gendisk *disk;
	struct nvme_id_ns *id;
	char disk_name[DISK_NAME_LEN];
	int node = ctrl->numa_node, flags = GENHD_FL_EXT_DEVT, ret;

	ns = kzalloc_node(sizeof(*ns), GFP_KERNEL, node);
	if (!ns)
		return -ENOMEM;

	ns->queue = blk_mq_init_queue(ctrl->tagset);
	if (IS_ERR(ns->queue)) {
		ret = PTR_ERR(ns->queue);
		goto out_free_ns;
	}

	if (ctrl->opts && ctrl->opts->data_digest)
		ns->queue->backing_dev_info->capabilities
			|= BDI_CAP_STABLE_WRITES;

	blk_queue_flag_set(QUEUE_FLAG_NONROT, ns->queue);
	if (ctrl->ops->flags & NVME_F_PCI_P2PDMA)
		blk_queue_flag_set(QUEUE_FLAG_PCI_P2PDMA, ns->queue);

	ns->queue->queuedata = ns;
	ns->ctrl = ctrl;

	kref_init(&ns->kref);
	ns->lba_shift = 9; /* set to a default value for 512 until disk is validated */

	blk_queue_logical_block_size(ns->queue, 1 << ns->lba_shift);
	nvmq_set_queue_limits(ctrl, ns->queue);

	ret = nvmq_identify_ns(ctrl, nsid, &id);
	if (ret)
		goto out_free_queue;

	if (id->ncap == 0) {
		ret = -EINVAL;
		goto out_free_id;
	}

	ret = nvmq_init_ns_head(ns, nsid, id);
	if (ret)
		goto out_free_id;
	nvmq_setup_streams_ns(ctrl, ns);
	nvmq_set_disk_name(disk_name, ns, ctrl, &flags);

	disk = alloc_disk_node(0, node);
	if (!disk) {
		ret = -ENOMEM;
		goto out_unlink_ns;
	}

	disk->fops = &nvmq_fops;
	disk->private_data = ns;
	disk->queue = ns->queue;
	disk->flags = flags;
	memcpy(disk->disk_name, disk_name, DISK_NAME_LEN);
	ns->disk = disk;

	__nvmq_revalidate_disk(disk, id);

	if ((ctrl->quirks & NVME_QUIRK_LIGHTNVM) && id->vs[0] == 0x1) {
		ret = nvmq_nvm_register(ns, disk_name, node);
		if (ret) {
			dev_warn(ctrl->device, "LightNVM init failure\n");
			goto out_put_disk;
		}
	}

	down_write(&ctrl->namespaces_rwsem);
	list_add_tail(&ns->list, &ctrl->namespaces);
	up_write(&ctrl->namespaces_rwsem);

	nvmq_get_ctrl(ctrl);

	device_add_disk(ctrl->device, ns->disk, nvmq_ns_id_attr_groups);
	
	//May Has BUG?
	nvmq_add_ns_cdev(ns);
	nvmq_mpath_add_disk(ns, id);
	nvmq_fault_inject_init(&ns->fault_inject, ns->disk->disk_name);
	kfree(id);

	return 0;
 out_put_disk:
	/* prevent double queue cleanup */
	ns->disk->queue = NULL;
	put_disk(ns->disk);
 out_unlink_ns:
	mutex_lock(&ctrl->subsys->lock);
	list_del_rcu(&ns->siblings);
	if (list_empty(&ns->head->list))
		list_del_init(&ns->head->entry);
	mutex_unlock(&ctrl->subsys->lock);
	nvmq_put_ns_head(ns->head);
 out_free_id:
	kfree(id);
 out_free_queue:
	blk_cleanup_queue(ns->queue);
 out_free_ns:
	kfree(ns);
	if (ret > 0)
		ret = blk_status_to_errno(nvmq_error_status(ret));
	return ret;
}

static void nvmq_ns_remove(struct nvmq_ns *ns)
{
	if (test_and_set_bit(NVME_NS_REMOVING, &ns->flags))
		return;

	nvmq_fault_inject_fini(&ns->fault_inject);

	mutex_lock(&ns->ctrl->subsys->lock);
	list_del_rcu(&ns->siblings);
	if (list_empty(&ns->head->list))
		list_del_init(&ns->head->entry);
	mutex_unlock(&ns->ctrl->subsys->lock);

	synchronize_rcu(); /* guarantee not available in head->list */
	nvmq_mpath_clear_current_path(ns);
	synchronize_srcu(&ns->head->srcu); /* wait for concurrent submissions */

	if (ns->disk && ns->disk->flags & GENHD_FL_UP) {
		del_gendisk(ns->disk);
		blk_cleanup_queue(ns->queue);
		if (blk_get_integrity(ns->disk))
			blk_integrity_unregister(ns->disk);
	}

	down_write(&ns->ctrl->namespaces_rwsem);
	list_del_init(&ns->list);
	up_write(&ns->ctrl->namespaces_rwsem);

	nvmq_mpath_check_last_path(ns);
	nvmq_put_ns(ns);
}

static void nvmq_validate_ns(struct nvmq_ctrl *ctrl, unsigned nsid)
{
	struct nvmq_ns *ns;

	ns = nvmq_find_get_ns(ctrl, nsid);
	if (ns) {
		if (ns->disk && revalidate_disk(ns->disk))
			nvmq_ns_remove(ns);
		nvmq_put_ns(ns);
	} else
		nvmq_alloc_ns(ctrl, nsid);
}

static void nvmq_remove_invalid_namespaces(struct nvmq_ctrl *ctrl,
					unsigned nsid)
{
	struct nvmq_ns *ns, *next;
	LIST_HEAD(rm_list);

	down_write(&ctrl->namespaces_rwsem);
	list_for_each_entry_safe(ns, next, &ctrl->namespaces, list) {
		if (ns->head->ns_id > nsid || test_bit(NVME_NS_DEAD, &ns->flags))
			list_move_tail(&ns->list, &rm_list);
	}
	up_write(&ctrl->namespaces_rwsem);

	list_for_each_entry_safe(ns, next, &rm_list, list)
		nvmq_ns_remove(ns);

}

static int nvmq_scan_ns_list(struct nvmq_ctrl *ctrl, unsigned nn)
{
	struct nvmq_ns *ns;
	__le32 *ns_list;
	unsigned i, j, nsid, prev = 0;
	unsigned num_lists = DIV_ROUND_UP_ULL((u64)nn, 1024);
	int ret = 0;

	ns_list = kzalloc(NVME_IDENTIFY_DATA_SIZE, GFP_KERNEL);
	if (!ns_list)
		return -ENOMEM;

	for (i = 0; i < num_lists; i++) {
		ret = nvmq_identify_ns_list(ctrl, prev, ns_list);
		if (ret)
			goto free;

		for (j = 0; j < min(nn, 1024U); j++) {
			nsid = le32_to_cpu(ns_list[j]);
			if (!nsid)
				goto out;

			nvmq_validate_ns(ctrl, nsid);

			while (++prev < nsid) {
				ns = nvmq_find_get_ns(ctrl, prev);
				if (ns) {
					nvmq_ns_remove(ns);
					nvmq_put_ns(ns);
				}
			}
		}
		nn -= j;
	}
 out:
	nvmq_remove_invalid_namespaces(ctrl, prev);
 free:
	kfree(ns_list);
	return ret;
}

static void nvmq_scan_ns_sequential(struct nvmq_ctrl *ctrl, unsigned nn)
{
	unsigned i;

	for (i = 1; i <= nn; i++)
		nvmq_validate_ns(ctrl, i);

	nvmq_remove_invalid_namespaces(ctrl, nn);
}

static void nvmq_clear_changed_ns_log(struct nvmq_ctrl *ctrl)
{
	size_t log_size = NVME_MAX_CHANGED_NAMESPACES * sizeof(__le32);
	__le32 *log;
	int error;

	log = kzalloc(log_size, GFP_KERNEL);
	if (!log)
		return;

	/*
	 * We need to read the log to clear the AEN, but we don't want to rely
	 * on it for the changed namespace information as userspace could have
	 * raced with us in reading the log page, which could cause us to miss
	 * updates.
	 */
	error = nvmq_get_log(ctrl, NVME_NSID_ALL, NVME_LOG_CHANGED_NS, 0, log,
			log_size, 0);
	if (error)
		dev_warn(ctrl->device,
			"reading changed ns log failed: %d\n", error);

	kfree(log);
}

static void nvmq_scan_work(struct work_struct *work)
{
	struct nvmq_ctrl *ctrl =
		container_of(work, struct nvmq_ctrl, scan_work);
	struct nvme_id_ctrl *id;
	unsigned nn;

	/* No tagset on a live ctrl means IO queues could not created */
	if (ctrl->state != NVME_CTRL_LIVE || !ctrl->tagset)
		return;

	if (test_and_clear_bit(NVME_AER_NOTICE_NS_CHANGED, &ctrl->events)) {
		dev_info(ctrl->device, "rescanning namespaces.\n");
		nvmq_clear_changed_ns_log(ctrl);
	}

	if (nvmq_identify_ctrl(ctrl, &id))
		return;

	mutex_lock(&ctrl->scan_lock);
	nn = le32_to_cpu(id->nn);
	if (!nvmq_ctrl_limited_cns(ctrl)) {
		if (!nvmq_scan_ns_list(ctrl, nn))
			goto out_free_id;
	}
	nvmq_scan_ns_sequential(ctrl, nn);
out_free_id:
	mutex_unlock(&ctrl->scan_lock);
	kfree(id);
	down_write(&ctrl->namespaces_rwsem);
	list_sort(NULL, &ctrl->namespaces, ns_cmp);
	up_write(&ctrl->namespaces_rwsem);
}

/*
 * This function iterates the namespace list unlocked to allow recovery from
 * controller failure. It is up to the caller to ensure the namespace list is
 * not modified by scan work while this function is executing.
 */
void nvmq_remove_namespaces(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns, *next;
	LIST_HEAD(ns_list);

	/*
	 * make sure to requeue I/O to all namespaces as these
	 * might result from the scan itself and must complete
	 * for the scan_work to make progress
	 */
	nvmq_mpath_clear_ctrl_paths(ctrl);

	/* prevent racing with ns scanning */
	flush_work(&ctrl->scan_work);

	/*
	 * The dead states indicates the controller was not gracefully
	 * disconnected. In that case, we won't be able to flush any data while
	 * removing the namespaces' disks; fail all the queues now to avoid
	 * potentially having to clean up the failed sync later.
	 */
	if (ctrl->state == NVME_CTRL_DEAD)
		nvmq_kill_queues(ctrl);

	down_write(&ctrl->namespaces_rwsem);
	list_splice_init(&ctrl->namespaces, &ns_list);
	up_write(&ctrl->namespaces_rwsem);

	list_for_each_entry_safe(ns, next, &ns_list, list)
		nvmq_ns_remove(ns);
}
EXPORT_SYMBOL_GPL(nvmq_remove_namespaces);

static int nvmq_class_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct nvmq_ctrl *ctrl =
		container_of(dev, struct nvmq_ctrl, ctrl_device);
	struct nvmqf_ctrl_options *opts = ctrl->opts;
	int ret;

	ret = add_uevent_var(env, "NVMQ_TRTYPE=%s", ctrl->ops->name);
	if (ret)
		return ret;

	if (opts) {
		ret = add_uevent_var(env, "NVMQ_TRADDR=%s", opts->traddr);
		if (ret)
			return ret;

		ret = add_uevent_var(env, "NVMQ_TRSVCID=%s",
				opts->trsvcid ?: "none");
		if (ret)
			return ret;

		ret = add_uevent_var(env, "NVMQ_HOST_TRADDR=%s",
				opts->host_traddr ?: "none");
	}
	return ret;
}

static void nvmq_aen_uevent(struct nvmq_ctrl *ctrl)
{
	char *envp[2] = { NULL, NULL };
	u32 aen_result = ctrl->aen_result;

	ctrl->aen_result = 0;
	if (!aen_result)
		return;

	envp[0] = kasprintf(GFP_KERNEL, "NVMQ_AEN=%#08x", aen_result);
	if (!envp[0])
		return;
	kobject_uevent_env(&ctrl->device->kobj, KOBJ_CHANGE, envp);
	kfree(envp[0]);
}

static void nvmq_async_event_work(struct work_struct *work)
{
	struct nvmq_ctrl *ctrl =
		container_of(work, struct nvmq_ctrl, async_event_work);

	nvmq_aen_uevent(ctrl);

	/*
	 * The transport drivers must guarantee AER submission here is safe by
	 * flushing ctrl async_event_work after changing the controller state
	 * from LIVE and before freeing the admin queue.
	*/
	if (ctrl->state == NVME_CTRL_LIVE)
		ctrl->ops->submit_async_event(ctrl);
}

static bool nvmq_ctrl_pp_status(struct nvmq_ctrl *ctrl)
{

	u32 csts;

	if (ctrl->ops->reg_read32(ctrl, NVME_REG_CSTS, &csts))
		return false;

	if (csts == ~0)
		return false;

	return ((ctrl->ctrl_config & NVME_CC_ENABLE) && (csts & NVME_CSTS_PP));
}

static void nvmq_get_fw_slot_info(struct nvmq_ctrl *ctrl)
{
	struct nvme_fw_slot_info_log *log;

	log = kmalloc(sizeof(*log), GFP_KERNEL);
	if (!log)
		return;

	if (nvmq_get_log(ctrl, NVME_NSID_ALL, NVME_LOG_FW_SLOT, 0, log,
			sizeof(*log), 0))
		dev_warn(ctrl->device, "Get FW SLOT INFO log error\n");
	kfree(log);
}

static void nvmq_fw_act_work(struct work_struct *work)
{
	struct nvmq_ctrl *ctrl = container_of(work,
				struct nvmq_ctrl, fw_act_work);
	unsigned long fw_act_timeout;

	if (ctrl->mtfa)
		fw_act_timeout = jiffies +
				msecs_to_jiffies(ctrl->mtfa * 100);
	else
		fw_act_timeout = jiffies +
				msecs_to_jiffies(admin_timeout * 1000);

	nvmq_stop_queues(ctrl);
	while (nvmq_ctrl_pp_status(ctrl)) {
		if (time_after(jiffies, fw_act_timeout)) {
			dev_warn(ctrl->device,
				"Fw activation timeout, reset controller\n");
			nvmq_try_sched_reset(ctrl);
			return;
		}
		msleep(100);
	}

	if (!nvmq_change_ctrl_state(ctrl, NVME_CTRL_LIVE))
		return;

	nvmq_start_queues(ctrl);
	/* read FW slot information to clear the AER */
	nvmq_get_fw_slot_info(ctrl);
}

static void nvmq_handle_aen_notice(struct nvmq_ctrl *ctrl, u32 result)
{
	u32 aer_notice_type = (result & 0xff00) >> 8;

	trace_nvmq_async_event(ctrl, aer_notice_type);

	switch (aer_notice_type) {
	case NVME_AER_NOTICE_NS_CHANGED:
		set_bit(NVME_AER_NOTICE_NS_CHANGED, &ctrl->events);
		nvmq_queue_scan(ctrl);
		break;
	case NVME_AER_NOTICE_FW_ACT_STARTING:
		/*
		 * We are (ab)using the RESETTING state to prevent subsequent
		 * recovery actions from interfering with the controller's
		 * firmware activation.
		 */
		if (nvmq_change_ctrl_state(ctrl, NVME_CTRL_RESETTING))
			queue_work(nvmq_wq, &ctrl->fw_act_work);
		break;
#ifdef CONFIG_NVMQ_MULTIPATH
	case NVME_AER_NOTICE_ANA:
		if (!ctrl->ana_log_buf)
			break;
		queue_work(nvmq_wq, &ctrl->ana_work);
		break;
#endif
	case NVME_AER_NOTICE_DISC_CHANGED:
		ctrl->aen_result = result;
		break;
	default:
		dev_warn(ctrl->device, "async event result %08x\n", result);
	}
}

void nvmq_complete_async_event(struct nvmq_ctrl *ctrl, __le16 status,
		volatile union nvme_result *res)
{
	u32 result = le32_to_cpu(res->u32);
	u32 aer_type = result & 0x07;

	if (le16_to_cpu(status) >> 1 != NVME_SC_SUCCESS)
		return;

	switch (aer_type) {
	case NVME_AER_NOTICE:
		nvmq_handle_aen_notice(ctrl, result);
		break;
	case NVME_AER_ERROR:
	case NVME_AER_SMART:
	case NVME_AER_CSS:
	case NVME_AER_VS:
		trace_nvmq_async_event(ctrl, aer_type);
		ctrl->aen_result = result;
		break;
	default:
		break;
	}
	queue_work(nvmq_wq, &ctrl->async_event_work);
}
EXPORT_SYMBOL_GPL(nvmq_complete_async_event);

void nvmq_stop_ctrl(struct nvmq_ctrl *ctrl)
{
	nvmq_mpath_stop(ctrl);
	nvmq_stop_keep_alive(ctrl);
	flush_work(&ctrl->async_event_work);
	cancel_work_sync(&ctrl->fw_act_work);
	if (ctrl->ops->stop_ctrl)
		ctrl->ops->stop_ctrl(ctrl);
}
EXPORT_SYMBOL_GPL(nvmq_stop_ctrl);

void nvmq_start_ctrl(struct nvmq_ctrl *ctrl)
{
	if (ctrl->kato)
		nvmq_start_keep_alive(ctrl);

	nvmq_enable_aen(ctrl);

	if (ctrl->queue_count > 1) {
		nvmq_queue_scan(ctrl);
		nvmq_start_queues(ctrl);
		nvmq_mpath_update(ctrl);
	}
	ctrl->created = true;
}
EXPORT_SYMBOL_GPL(nvmq_start_ctrl);

void nvmq_uninit_ctrl(struct nvmq_ctrl *ctrl)
{
	nvmq_fault_inject_fini(&ctrl->fault_inject);
	dev_pm_qos_hide_latency_tolerance(ctrl->device);
	cdev_device_del(&ctrl->cdev, ctrl->device);
}
EXPORT_SYMBOL_GPL(nvmq_uninit_ctrl);

static void nvmq_free_ctrl(struct device *dev)
{
	struct nvmq_ctrl *ctrl =
		container_of(dev, struct nvmq_ctrl, ctrl_device);
	struct nvmq_subsystem *subsys = ctrl->subsys;

	if (!subsys || ctrl->instance != subsys->instance)
		ida_simple_remove(&nvmq_instance_ida, ctrl->instance);

	kfree(ctrl->effects);
	nvmq_mpath_uninit(ctrl);
	__free_page(ctrl->discard_page);

	if (subsys) {
		mutex_lock(&nvmq_subsystems_lock);
		list_del(&ctrl->subsys_entry);
		sysfs_remove_link(&subsys->dev.kobj, dev_name(ctrl->device));
		mutex_unlock(&nvmq_subsystems_lock);
	}

	ctrl->ops->free_ctrl(ctrl);

	if (subsys)
		nvmq_put_subsystem(subsys);
}

/*
 * Initialize a NVMe controller structures.  This needs to be called during
 * earliest initialization so that we have the initialized structured around
 * during probing.
 */
int nvmq_init_ctrl(struct nvmq_ctrl *ctrl, struct device *dev,
		const struct nvmq_ctrl_ops *ops, unsigned long quirks)
{
	int ret;

	ctrl->state = NVME_CTRL_NEW;
	spin_lock_init(&ctrl->lock);
	mutex_init(&ctrl->scan_lock);
	INIT_LIST_HEAD(&ctrl->namespaces);
	init_rwsem(&ctrl->namespaces_rwsem);
	ctrl->dev = dev;
	ctrl->ops = ops;
	ctrl->quirks = quirks;
	INIT_WORK(&ctrl->scan_work, nvmq_scan_work);
	INIT_WORK(&ctrl->async_event_work, nvmq_async_event_work);
	INIT_WORK(&ctrl->fw_act_work, nvmq_fw_act_work);
	INIT_WORK(&ctrl->delete_work, nvmq_delete_ctrl_work);
	init_waitqueue_head(&ctrl->state_wq);

	INIT_DELAYED_WORK(&ctrl->ka_work, nvmq_keep_alive_work);
	memset(&ctrl->ka_cmd, 0, sizeof(ctrl->ka_cmd));
	ctrl->ka_cmd.common.opcode = nvme_admin_keep_alive;

	BUILD_BUG_ON(NVME_DSM_MAX_RANGES * sizeof(struct nvme_dsm_range) >
			PAGE_SIZE);
	ctrl->discard_page = alloc_page(GFP_KERNEL);
	if (!ctrl->discard_page) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ida_simple_get(&nvmq_instance_ida, 0, 0, GFP_KERNEL);
	if (ret < 0)
		goto out;
	ctrl->instance = ret;

	device_initialize(&ctrl->ctrl_device);
	ctrl->device = &ctrl->ctrl_device;
	ctrl->device->devt = MKDEV(MAJOR(nvmq_chr_devt), ctrl->instance);
	ctrl->device->class = nvmq_class;
	ctrl->device->parent = ctrl->dev;
	ctrl->device->groups = nvmq_dev_attr_groups;
	ctrl->device->release = nvmq_free_ctrl;
	dev_set_drvdata(ctrl->device, ctrl);
	ret = dev_set_name(ctrl->device, "nvmq%d", ctrl->instance);
	if (ret)
		goto out_release_instance;

	nvmq_get_ctrl(ctrl);
	cdev_init(&ctrl->cdev, &nvmq_dev_fops);
	ctrl->cdev.owner = ops->module;
	ret = cdev_device_add(&ctrl->cdev, ctrl->device);
	if (ret)
		goto out_free_name;

	/*
	 * Initialize latency tolerance controls.  The sysfs files won't
	 * be visible to userspace unless the device actually supports APST.
	 */
	ctrl->device->power.set_latency_tolerance = nvmq_set_latency_tolerance;
	dev_pm_qos_update_user_latency_tolerance(ctrl->device,
		min(default_ps_max_latency_us, (unsigned long)S32_MAX));

	nvmq_fault_inject_init(&ctrl->fault_inject, dev_name(ctrl->device));
	nvmq_mpath_init_ctrl(ctrl);

	return 0;
out_free_name:
	nvmq_put_ctrl(ctrl);
	kfree_const(ctrl->device->kobj.name);
out_release_instance:
	ida_simple_remove(&nvmq_instance_ida, ctrl->instance);
out:
	if (ctrl->discard_page)
		__free_page(ctrl->discard_page);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmq_init_ctrl);

/**
 * nvme_kill_queues(): Ends all namespace queues
 * @ctrl: the dead controller that needs to end
 *
 * Call this function when the driver determines it is unable to get the
 * controller in a state capable of servicing IO.
 */
void nvmq_kill_queues(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);

	/* Forcibly unquiesce queues to avoid blocking dispatch */
	if (ctrl->admin_q && !blk_queue_dying(ctrl->admin_q))
		blk_mq_unquiesce_queue(ctrl->admin_q);

	list_for_each_entry(ns, &ctrl->namespaces, list)
		nvmq_set_queue_dying(ns);

	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_kill_queues);

void nvmq_unfreeze(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_mq_unfreeze_queue(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_unfreeze);

int nvmq_wait_freeze_timeout(struct nvmq_ctrl *ctrl, long timeout)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list) {
		timeout = blk_mq_freeze_queue_wait_timeout(ns->queue, timeout);
		if (timeout <= 0)
			break;
	}
	up_read(&ctrl->namespaces_rwsem);
	return timeout;
}
EXPORT_SYMBOL_GPL(nvmq_wait_freeze_timeout);

void nvmq_wait_freeze(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_mq_freeze_queue_wait(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_wait_freeze);

void nvmq_start_freeze(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_freeze_queue_start(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_start_freeze);

void nvmq_stop_queues(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_mq_quiesce_queue(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_stop_queues);

void nvmq_start_queues(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_mq_unquiesce_queue(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_start_queues);

void nvmq_sync_io_queues(struct nvmq_ctrl *ctrl)
{
	struct nvmq_ns *ns;

	down_read(&ctrl->namespaces_rwsem);
	list_for_each_entry(ns, &ctrl->namespaces, list)
		blk_sync_queue(ns->queue);
	up_read(&ctrl->namespaces_rwsem);
}
EXPORT_SYMBOL_GPL(nvmq_sync_io_queues);

void nvmq_sync_queues(struct nvmq_ctrl *ctrl)
{
	nvmq_sync_io_queues(ctrl);
	if (ctrl->admin_q)
		blk_sync_queue(ctrl->admin_q);
}
EXPORT_SYMBOL_GPL(nvmq_sync_queues);

/*
 * Check we didn't inadvertently grow the command structure sizes:
 */
static inline void _nvmq_check_size(void)
{
	BUILD_BUG_ON(sizeof(struct nvme_common_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_rw_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_identify) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_features) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_download_firmware) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_format_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_dsm_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_write_zeroes_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_abort_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_get_log_page_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl) != NVME_IDENTIFY_DATA_SIZE);
	BUILD_BUG_ON(sizeof(struct nvme_id_ns) != NVME_IDENTIFY_DATA_SIZE);
	BUILD_BUG_ON(sizeof(struct nvme_lba_range_type) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_smart_log) != 512);
	BUILD_BUG_ON(sizeof(struct nvme_dbbuf) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_directive_cmd) != 64);
}


int nvmq_core_init(void)
{
	int result = -ENOMEM;

	_nvmq_check_size();

	nvmq_wq = alloc_workqueue("nvmq-wq",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);
	if (!nvmq_wq)
		goto out;

	nvmq_reset_wq = alloc_workqueue("nvmq-reset-wq",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);
	if (!nvmq_reset_wq)
		goto destroy_wq;

	nvmq_delete_wq = alloc_workqueue("nvmq-delete-wq",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);
	if (!nvmq_delete_wq)
		goto destroy_reset_wq;

	result = alloc_chrdev_region(&nvmq_chr_devt, 0, NVME_MINORS, "nvmq");
	if (result < 0)
		goto destroy_delete_wq;

	result = alloc_chrdev_region(&nvmq_ns_chr_devt,0,NVME_MINORS,"nvmq-generic");
	nvmq_class = class_create(THIS_MODULE, "nvmq");
	if (IS_ERR(nvmq_class)) {
		result = PTR_ERR(nvmq_class);
		goto unregister_chrdev;
	}
	nvmq_class->dev_uevent = nvmq_class_uevent;

	nvmq_subsys_class = class_create(THIS_MODULE, "nvmq-subsystem");
	if (IS_ERR(nvmq_subsys_class)) {
		result = PTR_ERR(nvmq_subsys_class);
		goto destroy_class;
	}
	return 0;

destroy_class:
	class_destroy(nvmq_class);
unregister_chrdev:
	unregister_chrdev_region(nvmq_chr_devt, NVME_MINORS);
destroy_delete_wq:
	destroy_workqueue(nvmq_delete_wq);
destroy_reset_wq:
	destroy_workqueue(nvmq_reset_wq);
destroy_wq:
	destroy_workqueue(nvmq_wq);
out:
	return result;
}

void nvmq_core_exit(void)
{
	class_destroy(nvmq_subsys_class);
	class_destroy(nvmq_class);
	unregister_chrdev_region(nvmq_chr_devt, NVME_MINORS);
	destroy_workqueue(nvmq_delete_wq);
	destroy_workqueue(nvmq_reset_wq);
	destroy_workqueue(nvmq_wq);
	ida_destroy(&nvmq_ns_chr_minor_ida);
}

// MODULE_LICENSE("GPL");
// MODULE_VERSION("1.0");
// module_init(nvmq_core_init);
// module_exit(nvmq_core_exit);
