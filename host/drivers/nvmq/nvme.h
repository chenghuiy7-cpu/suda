/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2011-2014, Intel Corporation.
 */

#ifndef _NVMQ_H
#define _NVMQ_H

#include <linux/nvme.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/kref.h>
#include <linux/blk-mq.h>
#include <linux/lightnvm.h>
#include <linux/sed-opal.h>
#include <linux/fault-inject.h>
#include <linux/rcupdate.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include <trace/events/block.h>

extern unsigned int nvmq_io_timeout;
#define NVMQ_IO_TIMEOUT	(nvmq_io_timeout * HZ)

extern unsigned int admin_timeout;
#define ADMIN_TIMEOUT	(admin_timeout * HZ)

#define NVMQ_DEFAULT_KATO	5
#define NVMQ_KATO_GRACE		10

extern struct workqueue_struct *nvmq_wq;
extern struct workqueue_struct *nvmq_reset_wq;
extern struct workqueue_struct *nvmq_delete_wq;

enum {
	NVME_NS_LBA		= 0,
	NVME_NS_LIGHTNVM	= 1,
};

/*
 * List of workarounds for devices that required behavior not specified in
 * the standard.
 */
enum nvmq_quirks {
	/*
	 * Prefers I/O aligned to a stripe size specified in a vendor
	 * specific Identify field.
	 */
	NVME_QUIRK_STRIPE_SIZE			= (1 << 0),

	/*
	 * The controller doesn't handle Identify value others than 0 or 1
	 * correctly.
	 */
	NVME_QUIRK_IDENTIFY_CNS			= (1 << 1),

	/*
	 * The controller deterministically returns O's on reads to
	 * logical blocks that deallocate was called on.
	 */
	NVME_QUIRK_DEALLOCATE_ZEROES		= (1 << 2),

	/*
	 * The controller needs a delay before starts checking the device
	 * readiness, which is done by reading the NVME_CSTS_RDY bit.
	 */
	NVME_QUIRK_DELAY_BEFORE_CHK_RDY		= (1 << 3),

	/*
	 * APST should not be used.
	 */
	NVME_QUIRK_NO_APST			= (1 << 4),

	/*
	 * The deepest sleep state should not be used.
	 */
	NVME_QUIRK_NO_DEEPEST_PS		= (1 << 5),

	/*
	 * Supports the LighNVM command set if indicated in vs[1].
	 */
	NVME_QUIRK_LIGHTNVM			= (1 << 6),

	/*
	 * Set MEDIUM priority on SQ creation
	 */
	NVME_QUIRK_MEDIUM_PRIO_SQ		= (1 << 7),

	/*
	 * Ignore device provided subnqn.
	 */
	NVME_QUIRK_IGNORE_DEV_SUBNQN		= (1 << 8),

	/*
	 * Broken Write Zeroes.
	 */
	NVME_QUIRK_DISABLE_WRITE_ZEROES		= (1 << 9),

	/*
	 * Force simple suspend/resume path.
	 */
	NVME_QUIRK_SIMPLE_SUSPEND		= (1 << 10),

	/*
	 * Use only one interrupt vector for all queues
	 */
	NVME_QUIRK_SINGLE_VECTOR		= (1 << 11),

	/*
	 * Use non-standard 128 bytes SQEs.
	 */
	NVME_QUIRK_128_BYTES_SQES		= (1 << 12),

	/*
	 * Prevent tag overlap between queues
	 */
	NVME_QUIRK_SHARED_TAGS                  = (1 << 13),

	/*
	 * The controller doesn't handle the Identify Namespace
	 * Identification Descriptor list subcommand despite claiming
	 * NVMe 1.3 compliance.
	 */
	NVME_QUIRK_NO_NS_DESC_LIST		= (1 << 15),
};

/*
 * Common request structure for NVMe passthrough.  All drivers must have
 * this structure as the first member of their request-private data.
 */
struct nvmq_request {
	struct nvme_command	*cmd;
	union nvme_result	result;
	u8			retries;
	u8			flags;
	u16			status;
	struct nvmq_ctrl	*ctrl;
};

struct nvmq_kernel {
	struct nvme_command	*cmds;
	union nvme_result	result;
	u8			num_cmds;
	u8			flags;
	u16			status;
	struct nvmq_ctrl	*ctrl;
};

/*
 * Mark a bio as coming in through the mpath node.
 */
#define REQ_NVME_MPATH		REQ_DRV

enum {
	NVME_REQ_CANCELLED		= (1 << 0),
	NVME_REQ_USERCMD		= (1 << 1),
	NVME_REQ_USERKERNEL		= (1 << 2),
};

static inline struct nvmq_request *nvmq_req(struct request *req)
{
	return blk_mq_rq_to_pdu(req);
}

static inline struct nvmq_kernel *nvmq_knl(struct request *req)
{
	return blk_mq_rq_to_pdu(req);
}

static inline u16 nvmq_req_qid(struct request *req)
{
	if (!req->rq_disk)
		return 0;
	return blk_mq_unique_tag_to_hwq(blk_mq_unique_tag(req)) + 1;
}

/* The below value is the specific amount of delay needed before checking
 * readiness in case of the PCI_DEVICE(0x1c58, 0x0003), which needs the
 * NVME_QUIRK_DELAY_BEFORE_CHK_RDY quirk enabled. The value (in ms) was
 * found empirically.
 */
#define NVME_QUIRK_DELAY_AMOUNT		2300

enum nvmq_ctrl_state {
	NVME_CTRL_NEW,
	NVME_CTRL_LIVE,
	NVME_CTRL_RESETTING,
	NVME_CTRL_CONNECTING,
	NVME_CTRL_DELETING,
	NVME_CTRL_DEAD,
};

struct nvmq_fault_inject {
#ifdef CONFIG_FAULT_INJECTION_DEBUG_FS
	struct fault_attr attr;
	struct dentry *parent;
	bool dont_retry;	/* DNR, do not retry */
	u16 status;		/* status code */
#endif
};

struct nvmq_ctrl {
	bool comp_seen;
	enum nvmq_ctrl_state state;
	bool identified;
	spinlock_t lock;
	struct mutex scan_lock;
	const struct nvmq_ctrl_ops *ops;
	struct request_queue *admin_q;
	struct request_queue *connect_q;
	struct request_queue *fabrics_q;
	struct device *dev;
	int instance;
	int numa_node;
	struct blk_mq_tag_set *tagset;
	struct blk_mq_tag_set *admin_tagset;
	struct list_head namespaces;
	struct rw_semaphore namespaces_rwsem;
	struct device ctrl_device;
	struct device *device;	/* char device */
	struct cdev cdev;
	struct work_struct reset_work;
	struct work_struct delete_work;
	wait_queue_head_t state_wq;

	struct nvmq_subsystem *subsys;
	struct list_head subsys_entry;

	struct opal_dev *opal_dev;

	char name[12];
	u16 cntlid;

	u32 ctrl_config;
	u16 mtfa;
	u32 queue_count;

	u64 cap;
	u32 page_size;
	u32 max_hw_sectors;
	u32 max_segments;
	u16 crdt[3];
	u16 oncs;
	u16 oacs;
	u16 nssa;
	u16 nr_streams;
	u16 sqsize;
	u32 max_namespaces;
	atomic_t abort_limit;
	u8 vwc;
	u32 vs;
	u32 sgls;
	u16 kas;
	u8 npss;
	u8 apsta;
	u32 oaes;
	u32 aen_result;
	u32 ctratt;
	unsigned int shutdown_timeout;
	unsigned int kato;
	bool subsystem;
	unsigned long quirks;
	struct nvme_id_power_state psd[32];
	struct nvme_effects_log *effects;
	struct work_struct scan_work;
	struct work_struct async_event_work;
	struct delayed_work ka_work;
	struct nvme_command ka_cmd;
	struct work_struct fw_act_work;
	unsigned long events;
	bool created;

#ifdef CONFIG_NVMQ_MULTIPATH
	/* asymmetric namespace access: */
	u8 anacap;
	u8 anatt;
	u32 anagrpmax;
	u32 nanagrpid;
	struct mutex ana_lock;
	struct nvme_ana_rsp_hdr *ana_log_buf;
	size_t ana_log_size;
	struct timer_list anatt_timer;
	struct work_struct ana_work;
#endif

	/* Power saving configuration */
	u64 ps_max_latency_us;
	bool apst_enabled;

	/* PCIe only: */
	u32 hmpre;
	u32 hmmin;
	u32 hmminds;
	u16 hmmaxd;

	/* Fabrics only */
	u32 ioccsz;
	u32 iorcsz;
	u16 icdoff;
	u16 maxcmd;
	int nr_reconnects;
	struct nvmqf_ctrl_options *opts;

	struct page *discard_page;
	unsigned long discard_page_busy;

	struct nvmq_fault_inject fault_inject;
};

enum nvmq_iopolicy {
	NVME_IOPOLICY_NUMA,
	NVME_IOPOLICY_RR,
};

struct nvmq_subsystem {
	int			instance;
	struct device		dev;
	/*
	 * Because we unregister the device on the last put we need
	 * a separate refcount.
	 */
	struct kref		ref;
	struct list_head	entry;
	struct mutex		lock;
	struct list_head	ctrls;
	struct list_head	nsheads;
	char			subnqn[NVMF_NQN_SIZE];
	char			serial[20];
	char			model[40];
	char			firmware_rev[8];
	u8			cmic;
	u16			vendor_id;
	u16			awupf;	/* 0's based awupf value. */
	struct ida		ns_ida;
#ifdef CONFIG_NVMQ_MULTIPATH
	enum nvmq_iopolicy	iopolicy;
#endif
};

/*
 * Container structure for uniqueue namespace identifiers.
 */
struct nvmq_ns_ids {
	u8	eui64[8];
	u8	nguid[16];
	uuid_t	uuid;
};

/*
 * Anchor structure for namespaces.  There is one for each namespace in a
 * NVMe subsystem that any of our controllers can see, and the namespace
 * structure for each controller is chained of it.  For private namespaces
 * there is a 1:1 relation to our namespace structures, that is ->list
 * only ever has a single entry for private namespaces.
 */
struct nvmq_ns_head {
	struct list_head	list;
	struct srcu_struct      srcu;
	struct nvmq_subsystem	*subsys;
	unsigned		ns_id;
	struct nvmq_ns_ids	ids;
	struct list_head	entry;
	struct kref		ref;
	int			instance;
#ifdef CONFIG_NVMQ_MULTIPATH
	struct gendisk		*disk;
	struct bio_list		requeue_list;
	spinlock_t		requeue_lock;
	struct work_struct	requeue_work;
	struct mutex		lock;
	unsigned long		flags;
#define NVME_NSHEAD_DISK_LIVE	0
	struct nvmq_ns __rcu	*current_path[];
#endif
};

struct nvmq_ns {
	struct list_head list;

	struct nvmq_ctrl *ctrl;
	struct request_queue *queue;
	struct gendisk *disk;
#ifdef CONFIG_NVMQ_MULTIPATH
	enum nvme_ana_state ana_state;
	u32 ana_grpid;
#endif
	struct list_head siblings;
	struct nvm_dev *ndev;
	struct kref kref;
	struct nvmq_ns_head *head;

	int lba_shift;
	u16 ms;
	u16 sgs;
	u32 sws;
	bool ext;
	u8 pi_type;
	unsigned long flags;
#define NVME_NS_REMOVING	0
#define NVME_NS_DEAD     	1
#define NVME_NS_ANA_PENDING	2

	struct cdev	cdev;
	struct device cdev_device;
	struct workqueue_struct *nvme_wq;
	struct nvmq_fault_inject fault_inject;

};

struct nvmq_ctrl_ops {
	const char *name;
	struct module *module;
	unsigned int flags;
#define NVME_F_FABRICS			(1 << 0)
#define NVME_F_METADATA_SUPPORTED	(1 << 1)
#define NVME_F_PCI_P2PDMA		(1 << 2)
	int (*reg_read32)(struct nvmq_ctrl *ctrl, u32 off, u32 *val);
	int (*reg_write32)(struct nvmq_ctrl *ctrl, u32 off, u32 val);
	int (*reg_read64)(struct nvmq_ctrl *ctrl, u32 off, u64 *val);
	void (*free_ctrl)(struct nvmq_ctrl *ctrl);
	void (*submit_async_event)(struct nvmq_ctrl *ctrl);
	void (*delete_ctrl)(struct nvmq_ctrl *ctrl);
	void (*stop_ctrl)(struct nvmq_ctrl *ctrl);
	int (*get_address)(struct nvmq_ctrl *ctrl, char *buf, int size);
};

#ifdef CONFIG_FAULT_INJECTION_DEBUG_FS
void nvmq_fault_inject_init(struct nvmq_fault_inject *fault_inj,
			    const char *dev_name);
void nvmq_fault_inject_fini(struct nvmq_fault_inject *fault_inject);
void nvmq_should_fail(struct request *req);
#else
static inline void nvmq_fault_inject_init(struct nvmq_fault_inject *fault_inj,
					  const char *dev_name)
{
}
static inline void nvmq_fault_inject_fini(struct nvmq_fault_inject *fault_inj)
{
}
static inline void nvmq_should_fail(struct request *req) {}
#endif

static inline int nvmq_reset_subsystem(struct nvmq_ctrl *ctrl)
{
	if (!ctrl->subsystem)
		return -ENOTTY;
	return ctrl->ops->reg_write32(ctrl, NVME_REG_NSSR, 0x4E564D65);
}

/*
 * Convert a 512B sector number to a device logical block number.
 */
static inline u64 nvmq_sect_to_lba(struct nvmq_ns *ns, sector_t sector)
{
	return sector >> (ns->lba_shift - SECTOR_SHIFT);
}

/*
 * Convert a device logical block number to a 512B sector number.
 */
static inline sector_t nvmq_lba_to_sect(struct nvmq_ns *ns, u64 lba)
{
	return lba << (ns->lba_shift - SECTOR_SHIFT);
}

static inline void nvmq_end_request(struct request *req, __le16 status,
		union nvme_result result)
{
	struct nvmq_request *rq = nvmq_req(req);

	rq->status = le16_to_cpu(status) >> 1;
	rq->result = result;

	pr_debug("rq %p status %u\n", rq, rq->status);
	/* inject error when permitted by fault injection framework */
	nvmq_should_fail(req);
	blk_mq_complete_request(req);
}

static inline void nvmq_get_ctrl(struct nvmq_ctrl *ctrl)
{
	get_device(ctrl->device);
}

static inline void nvmq_put_ctrl(struct nvmq_ctrl *ctrl)
{
	put_device(ctrl->device);
}

void nvmq_complete_rq(struct request *req);
bool nvmq_cancel_request(struct request *req, void *data, bool reserved);
void nvmq_cancel_tagset(struct nvmq_ctrl *ctrl);
void nvmq_cancel_admin_tagset(struct nvmq_ctrl *ctrl);
bool nvmq_change_ctrl_state(struct nvmq_ctrl *ctrl,
		enum nvmq_ctrl_state new_state);
bool nvmq_wait_reset(struct nvmq_ctrl *ctrl);
int nvmq_disable_ctrl(struct nvmq_ctrl *ctrl);
int nvmq_enable_ctrl(struct nvmq_ctrl *ctrl);
int nvmq_shutdown_ctrl(struct nvmq_ctrl *ctrl);
int nvmq_init_ctrl(struct nvmq_ctrl *ctrl, struct device *dev,
		const struct nvmq_ctrl_ops *ops, unsigned long quirks);
void nvmq_uninit_ctrl(struct nvmq_ctrl *ctrl);
void nvmq_start_ctrl(struct nvmq_ctrl *ctrl);
void nvmq_stop_ctrl(struct nvmq_ctrl *ctrl);
void nvmq_put_ctrl(struct nvmq_ctrl *ctrl);
int nvmq_init_identify(struct nvmq_ctrl *ctrl);

void nvmq_remove_namespaces(struct nvmq_ctrl *ctrl);

int nvmq_sec_submit(void *data, u16 spsp, u8 secp, void *buffer, size_t len,
		bool send);

void nvmq_complete_async_event(struct nvmq_ctrl *ctrl, __le16 status,
		volatile union nvme_result *res);

void nvmq_stop_queues(struct nvmq_ctrl *ctrl);
void nvmq_start_queues(struct nvmq_ctrl *ctrl);
void nvmq_kill_queues(struct nvmq_ctrl *ctrl);
void nvmq_sync_queues(struct nvmq_ctrl *ctrl);
void nvmq_sync_io_queues(struct nvmq_ctrl *ctrl);
void nvmq_unfreeze(struct nvmq_ctrl *ctrl);
void nvmq_wait_freeze(struct nvmq_ctrl *ctrl);
int nvmq_wait_freeze_timeout(struct nvmq_ctrl *ctrl, long timeout);
void nvmq_start_freeze(struct nvmq_ctrl *ctrl);

#define NVME_QID_ANY -1
struct request *nvmq_alloc_request(struct request_queue *q,
		struct nvme_command *cmd, blk_mq_req_flags_t flags, int qid);
void nvmq_cleanup_cmd(struct request *req);
blk_status_t nvmq_setup_raw_cmd(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *raw_cmd, struct nvme_command *cmd, bool is_first);
blk_status_t nvmq_setup_cmd(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *cmd);
blk_status_t nvmq_setup_raw_cmd_without_change_cid(struct nvmq_ns *ns, struct request *req,
		struct nvme_command *raw_cmd, struct nvme_command *cmd, bool is_first);

int nvmq_submit_sync_cmd(struct request_queue *q, struct nvme_command *cmd,
		void *buf, unsigned bufflen);
int __nvmq_submit_sync_cmd(struct request_queue *q, struct nvme_command *cmd,
		union nvme_result *result, void *buffer, unsigned bufflen,
		unsigned timeout, int qid, int at_head,
		blk_mq_req_flags_t flags, bool poll);
int nvmq_set_features(struct nvmq_ctrl *dev, unsigned int fid,
		      unsigned int dword11, void *buffer, size_t buflen,
		      u32 *result);
int nvmq_get_features(struct nvmq_ctrl *dev, unsigned int fid,
		      unsigned int dword11, void *buffer, size_t buflen,
		      u32 *result);
int nvmq_set_queue_count(struct nvmq_ctrl *ctrl, int *count);
void nvmq_stop_keep_alive(struct nvmq_ctrl *ctrl);
int nvmq_reset_ctrl(struct nvmq_ctrl *ctrl);
int nvmq_reset_ctrl_sync(struct nvmq_ctrl *ctrl);
int nvmq_try_sched_reset(struct nvmq_ctrl *ctrl);
int nvmq_delete_ctrl(struct nvmq_ctrl *ctrl);

int nvmq_get_log(struct nvmq_ctrl *ctrl, u32 nsid, u8 log_page, u8 lsp,
		void *log, size_t size, u64 offset);

extern const struct attribute_group *nvmq_ns_id_attr_groups[];
extern const struct block_device_operations nvmq_ns_head_ops;

#ifdef CONFIG_NVMQ_MULTIPATH
static inline bool nvmq_ctrl_use_ana(struct nvmq_ctrl *ctrl)
{
	return ctrl->ana_log_buf != NULL;
}

void nvmq_mpath_unfreeze(struct nvmq_subsystem *subsys);
void nvmq_mpath_wait_freeze(struct nvmq_subsystem *subsys);
void nvmq_mpath_start_freeze(struct nvmq_subsystem *subsys);
void nvmq_set_disk_name(char *disk_name, struct nvmq_ns *ns,
			struct nvmq_ctrl *ctrl, int *flags);
bool nvmq_failover_req(struct request *req);
void nvmq_kick_requeue_lists(struct nvmq_ctrl *ctrl);
// int nvmq_mpath_alloc_disk(struct nvmq_ctrl *ctrl,struct nvmq_ns_head *head);
// void nvmq_mpath_add_disk(struct nvmq_ns *ns, struct nvme_id_ns *id);
// void nvmq_mpath_remove_disk(struct nvmq_ns_head *head);
// int nvmq_mpath_init_identify(struct nvmq_ctrl *ctrl, struct nvmq_id_ctrl *id);
// void nvmq_mpath_init_ctrl(struct nvmq_ctrl *ctrl);
// void nvmq_mpath_update(struct nvmq_ctrl *ctrl);
// void nvmq_mpath_uninit(struct nvmq_ctrl *ctrl);
// void nvme_mpath_stop(struct nvme_ctrl *ctrl);
// bool nvme_mpath_clear_current_path(struct nvme_ns *ns);
// void nvme_mpath_clear_ctrl_paths(struct nvme_ctrl *ctrl);
// struct nvme_ns *nvme_find_path(struct nvme_ns_head *head);

// static inline void nvme_mpath_check_last_path(struct nvme_ns *ns)
// {
// 	struct nvme_ns_head *head = ns->head;

// 	if (head->disk && list_empty(&head->list))
// 		kblockd_schedule_work(&head->requeue_work);
// }

static inline void nvmq_trace_bio_complete(struct request *req,
        blk_status_t status)
{
	struct nvmq_ns *ns = req->q->queuedata;

	if (req->cmd_flags & REQ_NVME_MPATH)
		trace_block_bio_complete(ns->head->disk->queue,
					 req->bio, status);
}

// static inline void nvme_mpath_update_disk_size(struct gendisk *disk)
// {
// 	struct block_device *bdev = bdget_disk(disk, 0);

// 	if (bdev) {
// 		bd_set_size(bdev, get_capacity(disk) << SECTOR_SHIFT);
// 		bdput(bdev);
// 	}
// }

extern struct device_attribute dev_attr_ana_grpid;
extern struct device_attribute dev_attr_ana_state;
extern struct device_attribute subsys_attr_iopolicy;

#else
static inline bool nvmq_ctrl_use_ana(struct nvmq_ctrl *ctrl)
{
	return false;
}
/*
 * Without the multipath code enabled, multiple controller per subsystems are
 * visible as devices and thus we cannot use the subsystem instance.
 */
static inline void nvmq_set_disk_name(char *disk_name, struct nvmq_ns *ns,
				      struct nvmq_ctrl *ctrl, int *flags)
{
	sprintf(disk_name, "nvmq%dn%d", ctrl->instance, ns->head->instance);
}

static inline bool nvmq_failover_req(struct request *req)
{
	return false;
}
static inline void nvmq_kick_requeue_lists(struct nvmq_ctrl *ctrl)
{
}
static inline int nvmq_mpath_alloc_disk(struct nvmq_ctrl *ctrl,
		struct nvmq_ns_head *head)
{
	return 0;
}
static inline void nvmq_mpath_add_disk(struct nvmq_ns *ns,
		struct nvme_id_ns *id)
{
}
static inline void nvmq_mpath_remove_disk(struct nvmq_ns_head *head)
{
}
static inline bool nvmq_mpath_clear_current_path(struct nvmq_ns *ns)
{
	return false;
}
static inline void nvmq_mpath_clear_ctrl_paths(struct nvmq_ctrl *ctrl)
{
}
static inline void nvmq_mpath_check_last_path(struct nvmq_ns *ns)
{
}
static inline void nvmq_trace_bio_complete(struct request *req,
        blk_status_t status)
{
}
static inline void nvmq_mpath_init_ctrl(struct nvmq_ctrl *ctrl)
{
}
static inline int nvmq_mpath_init_identify(struct nvmq_ctrl *ctrl,
		struct nvme_id_ctrl *id)
{
	if (ctrl->subsys->cmic & (1 << 3))
		dev_warn(ctrl->device,
"Please enable CONFIG_NVMQ_MULTIPATH for full support of multi-port devices.\n");
	return 0;
}
static inline void nvmq_mpath_update(struct nvmq_ctrl *ctrl)
{
}
static inline void nvmq_mpath_uninit(struct nvmq_ctrl *ctrl)
{
}
static inline void nvmq_mpath_stop(struct nvmq_ctrl *ctrl)
{
}
static inline void nvmq_mpath_unfreeze(struct nvmq_subsystem *subsys)
{
}
static inline void nvmq_mpath_wait_freeze(struct nvmq_subsystem *subsys)
{
}
static inline void nvmq_mpath_start_freeze(struct nvmq_subsystem *subsys)
{
}
static inline void nvmq_mpath_update_disk_size(struct gendisk *disk)
{
}
#endif /* CONFIG_NVMQ_MULTIPATH */

#ifdef CONFIG_NVM
#else
static inline int nvmq_nvm_register(struct nvmq_ns *ns, char *disk_name,
				    int node)
{
	return 0;
}

static inline void nvmq_nvm_unregister(struct nvmq_ns *ns) {};
static inline int nvmq_nvm_ioctl(struct nvmq_ns *ns, unsigned int cmd,
							unsigned long arg)
{
	return -ENOTTY;
}
#endif /* CONFIG_NVM */

static inline struct nvmq_ns *nvmq_get_ns_from_dev(struct device *dev)
{
	return dev_to_disk(dev)->private_data;
}

int nvmq_core_init(void);
void nvmq_core_exit(void);

static inline bool nvme_is_compu_cmd_set(struct nvme_command *cmd)
{
	//May Have BUG Set NVME NAMESPACE LIST FOR THE FUTURE
	return cmd->common.opcode == 0x01 && cmd->common.nsid == 2;
}

static inline bool nvme_is_slm_set(struct nvme_command *cmd)
{
	return  (cmd->common.opcode == 0x01||
			cmd->common.opcode==0x00||
			cmd->common.opcode==0x02||
			cmd->common.opcode==0x05)&&(
				(cmd->common.nsid & (1<<31)) != 0
			);
}

static inline bool nvme_is_slm_rw(struct nvme_command* cmd){
	return (((cmd->common.nsid & (1<<31)) != 0)&&(cmd->common.opcode==0x05||cmd->common.opcode==0x02));
	
}

#endif /* _NVME_H */
