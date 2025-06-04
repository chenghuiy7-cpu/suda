/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NVMe over Fabrics common host code.
 * Copyright (c) 2015-2016 HGST, a Western Digital Company.
 */
#ifndef _NVMQ_FABRICS_H
#define _NVMQ_FABRICS_H 1

#include <linux/in.h>
#include <linux/inet.h>

#define NVMQF_MIN_QUEUE_SIZE	16
#define NVMQF_MAX_QUEUE_SIZE	1024
#define NVMQF_DEF_QUEUE_SIZE	128
#define NVMQF_DEF_RECONNECT_DELAY	10
/* default to 600 seconds of reconnect attempts before giving up */
#define NVMQF_DEF_CTRL_LOSS_TMO		600

/*
 * Define a host as seen by the target.  We allocate one at boot, but also
 * allow the override it when creating controllers.  This is both to provide
 * persistence of the Host NQN over multiple boots, and to allow using
 * multiple ones, for example in a container scenario.  Because we must not
 * use different Host NQNs with the same Host ID we generate a Host ID and
 * use this structure to keep track of the relation between the two.
 */
struct nvmqf_host {
	struct kref		ref;
	struct list_head	list;
	char			nqn[NVMF_NQN_SIZE];
	uuid_t			id;
};

/**
 * enum nvmqf_parsing_opts - used to define the sysfs parsing options used.
 */
enum {
	NVMQF_OPT_ERR		= 0,
	NVMQF_OPT_TRANSPORT	= 1 << 0,
	NVMQF_OPT_NQN		= 1 << 1,
	NVMQF_OPT_TRADDR		= 1 << 2,
	NVMQF_OPT_TRSVCID	= 1 << 3,
	NVMQF_OPT_QUEUE_SIZE	= 1 << 4,
	NVMQF_OPT_NR_IO_QUEUES	= 1 << 5,
	NVMQF_OPT_TL_RETRY_COUNT	= 1 << 6,
	NVMQF_OPT_KATO		= 1 << 7,
	NVMQF_OPT_HOSTNQN	= 1 << 8,
	NVMQF_OPT_RECONNECT_DELAY = 1 << 9,
	NVMQF_OPT_HOST_TRADDR	= 1 << 10,
	NVMQF_OPT_CTRL_LOSS_TMO	= 1 << 11,
	NVMQF_OPT_HOST_ID	= 1 << 12,
	NVMQF_OPT_DUP_CONNECT	= 1 << 13,
	NVMQF_OPT_DISABLE_SQFLOW = 1 << 14,
	NVMQF_OPT_HDR_DIGEST	= 1 << 15,
	NVMQF_OPT_DATA_DIGEST	= 1 << 16,
	NVMQF_OPT_NR_WRITE_QUEUES = 1 << 17,
	NVMQF_OPT_NR_POLL_QUEUES = 1 << 18,
	NVMQF_OPT_TOS		= 1 << 19,
};

/**
 * struct nvmqf_ctrl_options - Used to hold the options specified
 *			      with the parsing opts enum.
 * @mask:	Used by the fabrics library to parse through sysfs options
 *		on adding a NVMe controller.
 * @transport:	Holds the fabric transport "technology name" (for a lack of
 *		better description) that will be used by an NVMe controller
 *		being added.
 * @subsysnqn:	Hold the fully qualified NQN subystem name (format defined
 *		in the NVMe specification, "NVMe Qualified Names").
 * @traddr:	The transport-specific TRADDR field for a port on the
 *              subsystem which is adding a controller.
 * @trsvcid:	The transport-specific TRSVCID field for a port on the
 *              subsystem which is adding a controller.
 * @host_traddr: A transport-specific field identifying the NVME host port
 *              to use for the connection to the controller.
 * @queue_size: Number of IO queue elements.
 * @nr_io_queues: Number of controller IO queues that will be established.
 * @reconnect_delay: Time between two consecutive reconnect attempts.
 * @discovery_nqn: indicates if the subsysnqn is the well-known discovery NQN.
 * @kato:	Keep-alive timeout.
 * @host:	Virtual NVMe host, contains the NQN and Host ID.
 * @max_reconnects: maximum number of allowed reconnect attempts before removing
 *              the controller, (-1) means reconnect forever, zero means remove
 *              immediately;
 * @disable_sqflow: disable controller sq flow control
 * @hdr_digest: generate/verify header digest (TCP)
 * @data_digest: generate/verify data digest (TCP)
 * @nr_write_queues: number of queues for write I/O
 * @nr_poll_queues: number of queues for polling I/O
 * @tos: type of service
 */
struct nvmqf_ctrl_options {
	unsigned		mask;
	char			*transport;
	char			*subsysnqn;
	char			*traddr;
	char			*trsvcid;
	char			*host_traddr;
	size_t			queue_size;
	unsigned int		nr_io_queues;
	unsigned int		reconnect_delay;
	bool			discovery_nqn;
	bool			duplicate_connect;
	unsigned int		kato;
	struct nvmqf_host	*host;
	int			max_reconnects;
	bool			disable_sqflow;
	bool			hdr_digest;
	bool			data_digest;
	unsigned int		nr_write_queues;
	unsigned int		nr_poll_queues;
	int			tos;
};

/*
 * struct nvmqf_transport_ops - used to register a specific
 *			       fabric implementation of NVMe fabrics.
 * @entry:		Used by the fabrics library to add the new
 *			registration entry to its linked-list internal tree.
 * @module:             Transport module reference
 * @name:		Name of the NVMe fabric driver implementation.
 * @required_opts:	sysfs command-line options that must be specified
 *			when adding a new NVMe controller.
 * @allowed_opts:	sysfs command-line options that can be specified
 *			when adding a new NVMe controller.
 * @create_ctrl():	function pointer that points to a non-NVMe
 *			implementation-specific fabric technology
 *			that would go into starting up that fabric
 *			for the purpose of conneciton to an NVMe controller
 *			using that fabric technology.
 *
 * Notes:
 *	1. At minimum, 'required_opts' and 'allowed_opts' should
 *	   be set to the same enum parsing options defined earlier.
 *	2. create_ctrl() must be defined (even if it does nothing)
 *	3. struct nvmqf_transport_ops must be statically allocated in the
 *	   modules .bss section so that a pure module_get on @module
 *	   prevents the memory from beeing freed.
 */
struct nvmqf_transport_ops {
	struct list_head	entry;
	struct module		*module;
	const char		*name;
	int			required_opts;
	int			allowed_opts;
	struct nvmq_ctrl	*(*create_ctrl)(struct device *dev,
					struct nvmqf_ctrl_options *opts);
};

static inline bool
nvmqf_ctlr_matches_baseopts(struct nvmq_ctrl *ctrl,
			struct nvmqf_ctrl_options *opts)
{
	if (ctrl->state == NVME_CTRL_DELETING ||
	    ctrl->state == NVME_CTRL_DEAD ||
	    strcmp(opts->subsysnqn, ctrl->opts->subsysnqn) ||
	    strcmp(opts->host->nqn, ctrl->opts->host->nqn) ||
	    memcmp(&opts->host->id, &ctrl->opts->host->id, sizeof(uuid_t)))
		return false;

	return true;
}

int nvmqf_reg_read32(struct nvmq_ctrl *ctrl, u32 off, u32 *val);
int nvmqf_reg_read64(struct nvmq_ctrl *ctrl, u32 off, u64 *val);
int nvmqf_reg_write32(struct nvmq_ctrl *ctrl, u32 off, u32 val);
int nvmqf_connect_admin_queue(struct nvmq_ctrl *ctrl);
int nvmqf_connect_io_queue(struct nvmq_ctrl *ctrl, u16 qid, u8 pfch_tag, bool poll);
int nvmqf_register_transport(struct nvmqf_transport_ops *ops);
void nvmqf_unregister_transport(struct nvmqf_transport_ops *ops);
void nvmqf_free_options(struct nvmqf_ctrl_options *opts);
int nvmqf_get_address(struct nvmq_ctrl *ctrl, char *buf, int size);
bool nvmqf_should_reconnect(struct nvmq_ctrl *ctrl);
blk_status_t nvmqf_fail_nonready_command(struct nvmq_ctrl *ctrl,
		struct request *rq);
bool __nvmqf_check_ready(struct nvmq_ctrl *ctrl, struct request *rq,
		bool queue_live);
bool nvmqf_ip_options_match(struct nvmq_ctrl *ctrl,
		struct nvmqf_ctrl_options *opts);

static inline bool nvmqf_check_ready(struct nvmq_ctrl *ctrl, struct request *rq,
		bool queue_live)
{
	if (likely(ctrl->state == NVME_CTRL_LIVE))
		return true;
	return __nvmqf_check_ready(ctrl, rq, queue_live);
}

int nvmqf_init(void);
void nvmqf_exit(void);

#endif /* _NVME_FABRICS_H */
