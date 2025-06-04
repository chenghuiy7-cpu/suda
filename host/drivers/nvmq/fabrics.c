// SPDX-License-Identifier: GPL-2.0
/*
 * NVMe over Fabrics common host code.
 * Copyright (c) 2015-2016 HGST, a Western Digital Company.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/parser.h>
#include <linux/seq_file.h>
#include "nvme.h"
#include "fabrics.h"

static LIST_HEAD(nvmqf_transports);
static DECLARE_RWSEM(nvmqf_transports_rwsem);

static LIST_HEAD(nvmqf_hosts);
static DEFINE_MUTEX(nvmqf_hosts_mutex);

static struct nvmqf_host *nvmqf_default_host;

static struct nvmqf_host *__nvmqf_host_find(const char *hostnqn)
{
	struct nvmqf_host *host;

	list_for_each_entry(host, &nvmqf_hosts, list) {
		if (!strcmp(host->nqn, hostnqn))
			return host;
	}

	return NULL;
}

static struct nvmqf_host *nvmqf_host_add(const char *hostnqn)
{
	struct nvmqf_host *host;

	mutex_lock(&nvmqf_hosts_mutex);
	host = __nvmqf_host_find(hostnqn);
	if (host) {
		kref_get(&host->ref);
		goto out_unlock;
	}

	host = kmalloc(sizeof(*host), GFP_KERNEL);
	if (!host)
		goto out_unlock;

	kref_init(&host->ref);
	strlcpy(host->nqn, hostnqn, NVMF_NQN_SIZE);

	list_add_tail(&host->list, &nvmqf_hosts);
out_unlock:
	mutex_unlock(&nvmqf_hosts_mutex);
	return host;
}

static struct nvmqf_host *nvmqf_host_default(void)
{
	struct nvmqf_host *host;

	host = kmalloc(sizeof(*host), GFP_KERNEL);
	if (!host)
		return NULL;

	kref_init(&host->ref);
	uuid_gen(&host->id);
	snprintf(host->nqn, NVMF_NQN_SIZE,
		"nqn.2014-08.org.nvmexpress:uuid:%pUb", &host->id);

	mutex_lock(&nvmqf_hosts_mutex);
	list_add_tail(&host->list, &nvmqf_hosts);
	mutex_unlock(&nvmqf_hosts_mutex);

	return host;
}

static void nvmqf_host_destroy(struct kref *ref)
{
	struct nvmqf_host *host = container_of(ref, struct nvmqf_host, ref);

	mutex_lock(&nvmqf_hosts_mutex);
	list_del(&host->list);
	mutex_unlock(&nvmqf_hosts_mutex);

	kfree(host);
}

static void nvmqf_host_put(struct nvmqf_host *host)
{
	if (host)
		kref_put(&host->ref, nvmqf_host_destroy);
}

/**
 * nvmqf_get_address() -  Get address/port
 * @ctrl:	Host NVMe controller instance which we got the address
 * @buf:	OUTPUT parameter that will contain the address/port
 * @size:	buffer size
 */
int nvmqf_get_address(struct nvmq_ctrl *ctrl, char *buf, int size)
{
	int len = 0;

	if (ctrl->opts->mask & NVMQF_OPT_TRADDR)
		len += snprintf(buf, size, "traddr=%s", ctrl->opts->traddr);
	if (ctrl->opts->mask & NVMQF_OPT_TRSVCID)
		len += snprintf(buf + len, size - len, "%strsvcid=%s",
				(len) ? "," : "", ctrl->opts->trsvcid);
	if (ctrl->opts->mask & NVMQF_OPT_HOST_TRADDR)
		len += snprintf(buf + len, size - len, "%shost_traddr=%s",
				(len) ? "," : "", ctrl->opts->host_traddr);
	len += snprintf(buf + len, size - len, "\n");

	return len;
}
EXPORT_SYMBOL_GPL(nvmqf_get_address);

/**
 * nvmqf_reg_read32() -  NVMe Fabrics "Property Get" API function.
 * @ctrl:	Host NVMe controller instance maintaining the admin
 *		queue used to submit the property read command to
 *		the allocated NVMe controller resource on the target system.
 * @off:	Starting offset value of the targeted property
 *		register (see the fabrics section of the NVMe standard).
 * @val:	OUTPUT parameter that will contain the value of
 *		the property after a successful read.
 *
 * Used by the host system to retrieve a 32-bit capsule property value
 * from an NVMe controller on the target system.
 *
 * ("Capsule property" is an "PCIe register concept" applied to the
 * NVMe fabrics space.)
 *
 * Return:
 *	0: successful read
 *	> 0: NVMe error status code
 *	< 0: Linux errno error code
 */
int nvmqf_reg_read32(struct nvmq_ctrl *ctrl, u32 off, u32 *val)
{
	struct nvme_command cmd;
	union nvme_result res;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.prop_get.opcode = nvme_fabrics_command;
	cmd.prop_get.fctype = nvme_fabrics_type_property_get;
	cmd.prop_get.offset = cpu_to_le32(off);

	ret = __nvmq_submit_sync_cmd(ctrl->fabrics_q, &cmd, &res, NULL, 0, 0,
			NVME_QID_ANY, 0, 0, false);

	if (ret >= 0)
		*val = le64_to_cpu(res.u64);
	if (unlikely(ret != 0))
		dev_err(ctrl->device,
			"Property Get error: %d, offset %#x\n",
			ret > 0 ? ret & ~NVME_SC_DNR : ret, off);

	return ret;
}
EXPORT_SYMBOL_GPL(nvmqf_reg_read32);

/**
 * nvmqf_reg_read64() -  NVMe Fabrics "Property Get" API function.
 * @ctrl:	Host NVMe controller instance maintaining the admin
 *		queue used to submit the property read command to
 *		the allocated controller resource on the target system.
 * @off:	Starting offset value of the targeted property
 *		register (see the fabrics section of the NVMe standard).
 * @val:	OUTPUT parameter that will contain the value of
 *		the property after a successful read.
 *
 * Used by the host system to retrieve a 64-bit capsule property value
 * from an NVMe controller on the target system.
 *
 * ("Capsule property" is an "PCIe register concept" applied to the
 * NVMe fabrics space.)
 *
 * Return:
 *	0: successful read
 *	> 0: NVMe error status code
 *	< 0: Linux errno error code
 */
int nvmqf_reg_read64(struct nvmq_ctrl *ctrl, u32 off, u64 *val)
{
	struct nvme_command cmd;
	union nvme_result res;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.prop_get.opcode = nvme_fabrics_command;
	cmd.prop_get.fctype = nvme_fabrics_type_property_get;
	cmd.prop_get.attrib = 1;
	cmd.prop_get.offset = cpu_to_le32(off);

	ret = __nvmq_submit_sync_cmd(ctrl->fabrics_q, &cmd, &res, NULL, 0, 0,
			NVME_QID_ANY, 0, 0, false);

	if (ret >= 0)
		*val = le64_to_cpu(res.u64);
	if (unlikely(ret != 0))
		dev_err(ctrl->device,
			"Property Get error: %d, offset %#x\n",
			ret > 0 ? ret & ~NVME_SC_DNR : ret, off);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmqf_reg_read64);

/**
 * nvmqf_reg_write32() -  NVMe Fabrics "Property Write" API function.
 * @ctrl:	Host NVMe controller instance maintaining the admin
 *		queue used to submit the property read command to
 *		the allocated NVMe controller resource on the target system.
 * @off:	Starting offset value of the targeted property
 *		register (see the fabrics section of the NVMe standard).
 * @val:	Input parameter that contains the value to be
 *		written to the property.
 *
 * Used by the NVMe host system to write a 32-bit capsule property value
 * to an NVMe controller on the target system.
 *
 * ("Capsule property" is an "PCIe register concept" applied to the
 * NVMe fabrics space.)
 *
 * Return:
 *	0: successful write
 *	> 0: NVMe error status code
 *	< 0: Linux errno error code
 */
int nvmqf_reg_write32(struct nvmq_ctrl *ctrl, u32 off, u32 val)
{
	struct nvme_command cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.prop_set.opcode = nvme_fabrics_command;
	cmd.prop_set.fctype = nvme_fabrics_type_property_set;
	cmd.prop_set.attrib = 0;
	cmd.prop_set.offset = cpu_to_le32(off);
	cmd.prop_set.value = cpu_to_le64(val);

	ret = __nvmq_submit_sync_cmd(ctrl->fabrics_q, &cmd, NULL, NULL, 0, 0,
			NVME_QID_ANY, 0, 0, false);
	if (unlikely(ret))
		dev_err(ctrl->device,
			"Property Set error: %d, offset %#x\n",
			ret > 0 ? ret & ~NVME_SC_DNR : ret, off);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmqf_reg_write32);

/**
 * nvmqf_log_connect_error() - Error-parsing-diagnostic print
 * out function for connect() errors.
 *
 * @ctrl: the specific /dev/nvmqX device that had the error.
 *
 * @errval: Error code to be decoded in a more human-friendly
 *	    printout.
 *
 * @offset: For use with the NVMe error code NVME_SC_CONNECT_INVALID_PARAM.
 *
 * @cmd: This is the SQE portion of a submission capsule.
 *
 * @data: This is the "Data" portion of a submission capsule.
 */
static void nvmqf_log_connect_error(struct nvmq_ctrl *ctrl,
		int errval, int offset, struct nvme_command *cmd,
		struct nvmf_connect_data *data)
{
	int err_sctype = errval & (~NVME_SC_DNR);

	switch (err_sctype) {

	case (NVME_SC_CONNECT_INVALID_PARAM):
		if (offset >> 16) {
			char *inv_data = "Connect Invalid Data Parameter";

			switch (offset & 0xffff) {
			case (offsetof(struct nvmf_connect_data, cntlid)):
				dev_err(ctrl->device,
					"%s, cntlid: %d\n",
					inv_data, data->cntlid);
				break;
			case (offsetof(struct nvmf_connect_data, hostnqn)):
				dev_err(ctrl->device,
					"%s, hostnqn \"%s\"\n",
					inv_data, data->hostnqn);
				break;
			case (offsetof(struct nvmf_connect_data, subsysnqn)):
				dev_err(ctrl->device,
					"%s, subsysnqn \"%s\"\n",
					inv_data, data->subsysnqn);
				break;
			default:
				dev_err(ctrl->device,
					"%s, starting byte offset: %d\n",
				       inv_data, offset & 0xffff);
				break;
			}
		} else {
			char *inv_sqe = "Connect Invalid SQE Parameter";

			switch (offset) {
			case (offsetof(struct nvmf_connect_command, qid)):
				dev_err(ctrl->device,
				       "%s, qid %d\n",
					inv_sqe, cmd->connect.qid);
				break;
			default:
				dev_err(ctrl->device,
					"%s, starting byte offset: %d\n",
					inv_sqe, offset);
			}
		}
		break;

	case NVME_SC_CONNECT_INVALID_HOST:
		dev_err(ctrl->device,
			"Connect for subsystem %s is not allowed, hostnqn: %s\n",
			data->subsysnqn, data->hostnqn);
		break;

	case NVME_SC_CONNECT_CTRL_BUSY:
		dev_err(ctrl->device,
			"Connect command failed: controller is busy or not available\n");
		break;

	case NVME_SC_CONNECT_FORMAT:
		dev_err(ctrl->device,
			"Connect incompatible format: %d",
			cmd->connect.recfmt);
		break;

	case NVME_SC_HOST_PATH_ERROR:
		dev_err(ctrl->device,
			"Connect command failed: host path error\n");
		break;

	default:
		dev_err(ctrl->device,
			"Connect command failed, error wo/DNR bit: %d\n",
			err_sctype);
		break;
	} /* switch (err_sctype) */
}

/**
 * nvmqf_connect_admin_queue() - NVMe Fabrics Admin Queue "Connect"
 *				API function.
 * @ctrl:	Host nvmq controller instance used to request
 *              a new NVMe controller allocation on the target
 *              system and  establish an NVMe Admin connection to
 *              that controller.
 *
 * This function enables an NVMe host device to request a new allocation of
 * an NVMe controller resource on a target system as well establish a
 * fabrics-protocol connection of the NVMe Admin queue between the
 * host system device and the allocated NVMe controller on the
 * target system via a NVMe Fabrics "Connect" command.
 *
 * Return:
 *	0: success
 *	> 0: NVMe error status code
 *	< 0: Linux errno error code
 *
 */
int nvmqf_connect_admin_queue(struct nvmq_ctrl *ctrl)
{
	struct nvme_command cmd;
	union nvme_result res;
	struct nvmf_connect_data *data;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.connect.opcode = nvme_fabrics_command;
	cmd.connect.fctype = nvme_fabrics_type_connect;
	cmd.connect.qid = 0;
	cmd.connect.sqsize = cpu_to_le16(NVME_AQ_DEPTH - 1);

	/*
	 * Set keep-alive timeout in seconds granularity (ms * 1000)
	 * and add a grace period for controller kato enforcement
	 */
	cmd.connect.kato = ctrl->kato ?
		cpu_to_le32((ctrl->kato + NVMQ_KATO_GRACE) * 1000) : 0;

	if (ctrl->opts->disable_sqflow)
		cmd.connect.cattr |= NVME_CONNECT_DISABLE_SQFLOW;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	uuid_copy(&data->hostid, &ctrl->opts->host->id);
	data->cntlid = cpu_to_le16(0xffff);
	strncpy(data->subsysnqn, ctrl->opts->subsysnqn, NVMF_NQN_SIZE);
	strncpy(data->hostnqn, ctrl->opts->host->nqn, NVMF_NQN_SIZE);

	ret = __nvmq_submit_sync_cmd(ctrl->fabrics_q, &cmd, &res,
			data, sizeof(*data), 0, NVME_QID_ANY, 1,
			BLK_MQ_REQ_RESERVED | BLK_MQ_REQ_NOWAIT, false);
	if (ret) {
		nvmqf_log_connect_error(ctrl, ret, le32_to_cpu(res.u32),
				       &cmd, data);
		goto out_free_data;
	}

	ctrl->cntlid = le16_to_cpu(res.u16);

out_free_data:
	kfree(data);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmqf_connect_admin_queue);

/**
 * nvmqf_connect_io_queue() - NVMe Fabrics I/O Queue "Connect"
 *			     API function.
 * @ctrl:	Host nvmq controller instance used to establish an
 *		NVMe I/O queue connection to the already allocated NVMe
 *		controller on the target system.
 * @qid:	NVMe I/O queue number for the new I/O connection between
 *		host and target (note qid == 0 is illegal as this is
 *		the Admin queue, per NVMe standard).
 * @poll:	Whether or not to poll for the completion of the connect cmd.
 *
 * This function issues a fabrics-protocol connection
 * of a NVMe I/O queue (via NVMe Fabrics "Connect" command)
 * between the host system device and the allocated NVMe controller
 * on the target system.
 *
 * Return:
 *	0: success
 *	> 0: NVMe error status code
 *	< 0: Linux errno error code
 */
int nvmqf_connect_io_queue(struct nvmq_ctrl *ctrl, u16 qid, u8 pfch_tag, bool poll)
{
	struct nvme_command cmd;
	struct nvmf_connect_data *data;
	union nvme_result res;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.connect.opcode = nvme_fabrics_command;
	cmd.connect.fctype = nvme_fabrics_type_connect;
	cmd.connect.qid = cpu_to_le16(qid);
	cmd.connect.sqsize = cpu_to_le16(ctrl->sqsize);
	cmd.connect.resv3 = pfch_tag;

	if (ctrl->opts->disable_sqflow)
		cmd.connect.cattr |= NVME_CONNECT_DISABLE_SQFLOW;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	uuid_copy(&data->hostid, &ctrl->opts->host->id);
	data->cntlid = cpu_to_le16(ctrl->cntlid);
	strncpy(data->subsysnqn, ctrl->opts->subsysnqn, NVMF_NQN_SIZE);
	strncpy(data->hostnqn, ctrl->opts->host->nqn, NVMF_NQN_SIZE);

	ret = __nvmq_submit_sync_cmd(ctrl->connect_q, &cmd, &res,
			data, sizeof(*data), 0, qid, 1,
			BLK_MQ_REQ_RESERVED | BLK_MQ_REQ_NOWAIT, poll);
	pr_warn("[%d] %p %u ret is %d\n", current->pid, ctrl, qid, ret);
	if (ret) {
		nvmqf_log_connect_error(ctrl, ret, le32_to_cpu(res.u32),
				       &cmd, data);
	}
	kfree(data);
	return ret;
}
EXPORT_SYMBOL_GPL(nvmqf_connect_io_queue);

bool nvmqf_should_reconnect(struct nvmq_ctrl *ctrl)
{
	if (ctrl->opts->max_reconnects == -1 ||
	    ctrl->nr_reconnects < ctrl->opts->max_reconnects)
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(nvmqf_should_reconnect);

/**
 * nvmqf_register_transport() - NVMe Fabrics Library registration function.
 * @ops:	Transport ops instance to be registered to the
 *		common fabrics library.
 *
 * API function that registers the type of specific transport fabric
 * being implemented to the common NVMe fabrics library. Part of
 * the overall init sequence of starting up a fabrics driver.
 */
int nvmqf_register_transport(struct nvmqf_transport_ops *ops)
{
	if (!ops->create_ctrl)
		return -EINVAL;

	down_write(&nvmqf_transports_rwsem);
	list_add_tail(&ops->entry, &nvmqf_transports);
	up_write(&nvmqf_transports_rwsem);

	return 0;
}
EXPORT_SYMBOL_GPL(nvmqf_register_transport);

/**
 * nvmqf_unregister_transport() - NVMe Fabrics Library unregistration function.
 * @ops:	Transport ops instance to be unregistered from the
 *		common fabrics library.
 *
 * Fabrics API function that unregisters the type of specific transport
 * fabric being implemented from the common NVMe fabrics library.
 * Part of the overall exit sequence of unloading the implemented driver.
 */
void nvmqf_unregister_transport(struct nvmqf_transport_ops *ops)
{
	down_write(&nvmqf_transports_rwsem);
	list_del(&ops->entry);
	up_write(&nvmqf_transports_rwsem);
}
EXPORT_SYMBOL_GPL(nvmqf_unregister_transport);

static struct nvmqf_transport_ops *nvmqf_lookup_transport(
		struct nvmqf_ctrl_options *opts)
{
	struct nvmqf_transport_ops *ops;

	lockdep_assert_held(&nvmqf_transports_rwsem);

	list_for_each_entry(ops, &nvmqf_transports, entry) {
		if (strcmp(ops->name, opts->transport) == 0)
			return ops;
	}

	return NULL;
}

/*
 * For something we're not in a state to send to the device the default action
 * is to busy it and retry it after the controller state is recovered.  However,
 * if the controller is deleting or if anything is marked for failfast or
 * nvmq multipath it is immediately failed.
 *
 * Note: commands used to initialize the controller will be marked for failfast.
 * Note: nvmq cli/ioctl commands are marked for failfast.
 */
blk_status_t nvmqf_fail_nonready_command(struct nvmq_ctrl *ctrl,
		struct request *rq)
{
	if (ctrl->state != NVME_CTRL_DELETING &&
	    ctrl->state != NVME_CTRL_DEAD &&
	    !blk_noretry_request(rq) && !(rq->cmd_flags & REQ_NVME_MPATH))
		return BLK_STS_RESOURCE;

	nvmq_req(rq)->status = NVME_SC_HOST_PATH_ERROR;
	blk_mq_start_request(rq);
	nvmq_complete_rq(rq);
	return BLK_STS_OK;
}
EXPORT_SYMBOL_GPL(nvmqf_fail_nonready_command);

bool __nvmqf_check_ready(struct nvmq_ctrl *ctrl, struct request *rq,
		bool queue_live)
{
	struct nvmq_request *req = nvmq_req(rq);

	/*
	 * currently we have a problem sending passthru commands
	 * on the admin_q if the controller is not LIVE because we can't
	 * make sure that they are going out after the admin connect,
	 * controller enable and/or other commands in the initialization
	 * sequence. until the controller will be LIVE, fail with
	 * BLK_STS_RESOURCE so that they will be rescheduled.
	 */
	if (rq->q == ctrl->admin_q && (req->flags & NVME_REQ_USERCMD))
		return false;

	/*
	 * Only allow commands on a live queue, except for the connect command,
	 * which is require to set the queue live in the appropinquate states.
	 */
	switch (ctrl->state) {
	case NVME_CTRL_CONNECTING:
		if (blk_rq_is_passthrough(rq) && nvme_is_fabrics(req->cmd) &&
		    req->cmd->fabrics.fctype == nvme_fabrics_type_connect)
			return true;
		break;
	default:
		break;
	case NVME_CTRL_DEAD:
		return false;
	}

	return queue_live;
}
EXPORT_SYMBOL_GPL(__nvmqf_check_ready);

static const match_table_t opt_tokens = {
	{ NVMQF_OPT_TRANSPORT,		"transport=%s"		},
	{ NVMQF_OPT_TRADDR,		"traddr=%s"		},
	{ NVMQF_OPT_TRSVCID,		"trsvcid=%s"		},
	{ NVMQF_OPT_NQN,			"nqn=%s"		},
	{ NVMQF_OPT_QUEUE_SIZE,		"queue_size=%d"		},
	{ NVMQF_OPT_NR_IO_QUEUES,	"nr_io_queues=%d"	},
	{ NVMQF_OPT_RECONNECT_DELAY,	"reconnect_delay=%d"	},
	{ NVMQF_OPT_CTRL_LOSS_TMO,	"ctrl_loss_tmo=%d"	},
	{ NVMQF_OPT_KATO,		"keep_alive_tmo=%d"	},
	{ NVMQF_OPT_HOSTNQN,		"hostnqn=%s"		},
	{ NVMQF_OPT_HOST_TRADDR,		"host_traddr=%s"	},
	{ NVMQF_OPT_HOST_ID,		"hostid=%s"		},
	{ NVMQF_OPT_DUP_CONNECT,		"duplicate_connect"	},
	{ NVMQF_OPT_DISABLE_SQFLOW,	"disable_sqflow"	},
	{ NVMQF_OPT_HDR_DIGEST,		"hdr_digest"		},
	{ NVMQF_OPT_DATA_DIGEST,		"data_digest"		},
	{ NVMQF_OPT_NR_WRITE_QUEUES,	"nr_write_queues=%d"	},
	{ NVMQF_OPT_NR_POLL_QUEUES,	"nr_poll_queues=%d"	},
	{ NVMQF_OPT_TOS,			"tos=%d"		},
	{ NVMQF_OPT_ERR,			NULL			}
};

static int nvmqf_parse_options(struct nvmqf_ctrl_options *opts,
		const char *buf)
{
	substring_t args[MAX_OPT_ARGS];
	char *options, *o, *p;
	int token, ret = 0;
	size_t nqnlen  = 0;
	int ctrl_loss_tmo = NVMQF_DEF_CTRL_LOSS_TMO;
	uuid_t hostid;

	/* Set defaults */
	opts->queue_size = NVMQF_DEF_QUEUE_SIZE;
	opts->nr_io_queues = num_online_cpus();
	opts->reconnect_delay = NVMQF_DEF_RECONNECT_DELAY;
	opts->kato = NVMQ_DEFAULT_KATO;
	opts->duplicate_connect = false;
	opts->hdr_digest = false;
	opts->data_digest = false;
	opts->tos = -1; /* < 0 == use transport default */

	options = o = kstrdup(buf, GFP_KERNEL);
	if (!options)
		return -ENOMEM;

	uuid_gen(&hostid);

	while ((p = strsep(&o, ",\n")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, opt_tokens, args);
		opts->mask |= token;
		switch (token) {
		case NVMQF_OPT_TRANSPORT:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			kfree(opts->transport);
			opts->transport = p;
			break;
		case NVMQF_OPT_NQN:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			kfree(opts->subsysnqn);
			opts->subsysnqn = p;
			nqnlen = strlen(opts->subsysnqn);
			if (nqnlen >= NVMF_NQN_SIZE) {
				pr_err("%s needs to be < %d bytes\n",
					opts->subsysnqn, NVMF_NQN_SIZE);
				ret = -EINVAL;
				goto out;
			}
			opts->discovery_nqn =
				!(strcmp(opts->subsysnqn,
					 NVME_DISC_SUBSYS_NAME));
			break;
		case NVMQF_OPT_TRADDR:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			kfree(opts->traddr);
			opts->traddr = p;
			break;
		case NVMQF_OPT_TRSVCID:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			kfree(opts->trsvcid);
			opts->trsvcid = p;
			break;
		case NVMQF_OPT_QUEUE_SIZE:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token < NVMQF_MIN_QUEUE_SIZE ||
			    token > NVMQF_MAX_QUEUE_SIZE) {
				pr_err("Invalid queue_size %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			opts->queue_size = token;
			break;
		case NVMQF_OPT_NR_IO_QUEUES:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token <= 0) {
				pr_err("Invalid number of IOQs %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			if (opts->discovery_nqn) {
				pr_debug("Ignoring nr_io_queues value for discovery controller\n");
				break;
			}

			opts->nr_io_queues = min_t(unsigned int,
					num_online_cpus(), token);
			break;
		case NVMQF_OPT_KATO:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}

			if (token < 0) {
				pr_err("Invalid keep_alive_tmo %d\n", token);
				ret = -EINVAL;
				goto out;
			} else if (token == 0 && !opts->discovery_nqn) {
				/* Allowed for debug */
				pr_warn("keep_alive_tmo 0 won't execute keep alives!!!\n");
			}
			opts->kato = token;
			break;
		case NVMQF_OPT_CTRL_LOSS_TMO:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}

			if (token < 0)
				pr_warn("ctrl_loss_tmo < 0 will reconnect forever\n");
			ctrl_loss_tmo = token;
			break;
		case NVMQF_OPT_HOSTNQN:
			if (opts->host) {
				pr_err("hostnqn already user-assigned: %s\n",
				       opts->host->nqn);
				ret = -EADDRINUSE;
				goto out;
			}
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			nqnlen = strlen(p);
			if (nqnlen >= NVMF_NQN_SIZE) {
				pr_err("%s needs to be < %d bytes\n",
					p, NVMF_NQN_SIZE);
				kfree(p);
				ret = -EINVAL;
				goto out;
			}
			nvmqf_host_put(opts->host);
			opts->host = nvmqf_host_add(p);
			kfree(p);
			if (!opts->host) {
				ret = -ENOMEM;
				goto out;
			}
			break;
		case NVMQF_OPT_RECONNECT_DELAY:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token <= 0) {
				pr_err("Invalid reconnect_delay %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			opts->reconnect_delay = token;
			break;
		case NVMQF_OPT_HOST_TRADDR:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			kfree(opts->host_traddr);
			opts->host_traddr = p;
			break;
		case NVMQF_OPT_HOST_ID:
			p = match_strdup(args);
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			ret = uuid_parse(p, &hostid);
			if (ret) {
				pr_err("Invalid hostid %s\n", p);
				ret = -EINVAL;
				kfree(p);
				goto out;
			}
			kfree(p);
			break;
		case NVMQF_OPT_DUP_CONNECT:
			opts->duplicate_connect = true;
			break;
		case NVMQF_OPT_DISABLE_SQFLOW:
			opts->disable_sqflow = true;
			break;
		case NVMQF_OPT_HDR_DIGEST:
			opts->hdr_digest = true;
			break;
		case NVMQF_OPT_DATA_DIGEST:
			opts->data_digest = true;
			break;
		case NVMQF_OPT_NR_WRITE_QUEUES:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token <= 0) {
				pr_err("Invalid nr_write_queues %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			opts->nr_write_queues = token;
			break;
		case NVMQF_OPT_NR_POLL_QUEUES:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token <= 0) {
				pr_err("Invalid nr_poll_queues %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			opts->nr_poll_queues = token;
			break;
		case NVMQF_OPT_TOS:
			if (match_int(args, &token)) {
				ret = -EINVAL;
				goto out;
			}
			if (token < 0) {
				pr_err("Invalid type of service %d\n", token);
				ret = -EINVAL;
				goto out;
			}
			if (token > 255) {
				pr_warn("Clamping type of service to 255\n");
				token = 255;
			}
			opts->tos = token;
			break;
		default:
			pr_warn("unknown parameter or missing value '%s' in ctrl creation request\n",
				p);
			ret = -EINVAL;
			goto out;
		}
	}

	if (opts->discovery_nqn) {
		opts->nr_io_queues = 0;
		opts->nr_write_queues = 0;
		opts->nr_poll_queues = 0;
		opts->duplicate_connect = true;
	}
	if (ctrl_loss_tmo < 0)
		opts->max_reconnects = -1;
	else
		opts->max_reconnects = DIV_ROUND_UP(ctrl_loss_tmo,
						opts->reconnect_delay);

	if (!opts->host) {
		kref_get(&nvmqf_default_host->ref);
		opts->host = nvmqf_default_host;
	}

	uuid_copy(&opts->host->id, &hostid);

out:
	kfree(options);
	return ret;
}

static int nvmqf_check_required_opts(struct nvmqf_ctrl_options *opts,
		unsigned int required_opts)
{
	if ((opts->mask & required_opts) != required_opts) {
		int i;

		for (i = 0; i < ARRAY_SIZE(opt_tokens); i++) {
			if ((opt_tokens[i].token & required_opts) &&
			    !(opt_tokens[i].token & opts->mask)) {
				pr_warn("missing parameter '%s'\n",
					opt_tokens[i].pattern);
			}
		}

		return -EINVAL;
	}

	return 0;
}

bool nvmqf_ip_options_match(struct nvmq_ctrl *ctrl,
		struct nvmqf_ctrl_options *opts)
{
	if (!nvmqf_ctlr_matches_baseopts(ctrl, opts) ||
	    strcmp(opts->traddr, ctrl->opts->traddr) ||
	    strcmp(opts->trsvcid, ctrl->opts->trsvcid))
		return false;

	/*
	 * Checking the local address is rough. In most cases, none is specified
	 * and the host port is selected by the stack.
	 *
	 * Assume no match if:
	 * -  local address is specified and address is not the same
	 * -  local address is not specified but remote is, or vice versa
	 *    (admin using specific host_traddr when it matters).
	 */
	if ((opts->mask & NVMQF_OPT_HOST_TRADDR) &&
	    (ctrl->opts->mask & NVMQF_OPT_HOST_TRADDR)) {
		if (strcmp(opts->host_traddr, ctrl->opts->host_traddr))
			return false;
	} else if ((opts->mask & NVMQF_OPT_HOST_TRADDR) ||
		   (ctrl->opts->mask & NVMQF_OPT_HOST_TRADDR)) {
		return false;
	}

	return true;
}
EXPORT_SYMBOL_GPL(nvmqf_ip_options_match);

static int nvmqf_check_allowed_opts(struct nvmqf_ctrl_options *opts,
		unsigned int allowed_opts)
{
	if (opts->mask & ~allowed_opts) {
		int i;

		for (i = 0; i < ARRAY_SIZE(opt_tokens); i++) {
			if ((opt_tokens[i].token & opts->mask) &&
			    (opt_tokens[i].token & ~allowed_opts)) {
				pr_warn("invalid parameter '%s'\n",
					opt_tokens[i].pattern);
			}
		}

		return -EINVAL;
	}

	return 0;
}

void nvmqf_free_options(struct nvmqf_ctrl_options *opts)
{
	nvmqf_host_put(opts->host);
	kfree(opts->transport);
	kfree(opts->traddr);
	kfree(opts->trsvcid);
	kfree(opts->subsysnqn);
	kfree(opts->host_traddr);
	kfree(opts);
}
EXPORT_SYMBOL_GPL(nvmqf_free_options);

#define NVMQF_REQUIRED_OPTS	(NVMQF_OPT_TRANSPORT | NVMQF_OPT_NQN)
#define NVMQF_ALLOWED_OPTS	(NVMQF_OPT_QUEUE_SIZE | NVMQF_OPT_NR_IO_QUEUES | \
				 NVMQF_OPT_KATO | NVMQF_OPT_HOSTNQN | \
				 NVMQF_OPT_HOST_ID | NVMQF_OPT_DUP_CONNECT |\
				 NVMQF_OPT_DISABLE_SQFLOW)

static struct nvmq_ctrl *
nvmqf_create_ctrl(struct device *dev, const char *buf)
{
	struct nvmqf_ctrl_options *opts;
	struct nvmqf_transport_ops *ops;
	struct nvmq_ctrl *ctrl;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	ret = nvmqf_parse_options(opts, buf);
	if (ret)
		goto out_free_opts;


	request_module("nvmq-%s", opts->transport);

	/*
	 * Check the generic options first as we need a valid transport for
	 * the lookup below.  Then clear the generic flags so that transport
	 * drivers don't have to care about them.
	 */
	ret = nvmqf_check_required_opts(opts, NVMQF_REQUIRED_OPTS);
	if (ret)
		goto out_free_opts;
	opts->mask &= ~NVMQF_REQUIRED_OPTS;

	down_read(&nvmqf_transports_rwsem);
	ops = nvmqf_lookup_transport(opts);
	if (!ops) {
		pr_info("no handler found for transport %s.\n",
			opts->transport);
		ret = -EINVAL;
		goto out_unlock;
	}

	if (!try_module_get(ops->module)) {
		ret = -EBUSY;
		goto out_unlock;
	}
	up_read(&nvmqf_transports_rwsem);

	ret = nvmqf_check_required_opts(opts, ops->required_opts);
	if (ret)
		goto out_module_put;
	ret = nvmqf_check_allowed_opts(opts, NVMQF_ALLOWED_OPTS |
				ops->allowed_opts | ops->required_opts);
	if (ret)
		goto out_module_put;

	ctrl = ops->create_ctrl(dev, opts);
	if (IS_ERR(ctrl)) {
		ret = PTR_ERR(ctrl);
		goto out_module_put;
	}

	module_put(ops->module);
	return ctrl;

out_module_put:
	module_put(ops->module);
	goto out_free_opts;
out_unlock:
	up_read(&nvmqf_transports_rwsem);
out_free_opts:
	nvmqf_free_options(opts);
	return ERR_PTR(ret);
}

static struct class *nvmqf_class;
static struct device *nvmqf_device;
static DEFINE_MUTEX(nvmqf_dev_mutex);

static ssize_t nvmqf_dev_write(struct file *file, const char __user *ubuf,
		size_t count, loff_t *pos)
{
	struct seq_file *seq_file = file->private_data;
	struct nvmq_ctrl *ctrl;
	const char *buf;
	int ret = 0;

	if (count > PAGE_SIZE)
		return -ENOMEM;

	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	mutex_lock(&nvmqf_dev_mutex);
	if (seq_file->private) {
		ret = -EINVAL;
		goto out_unlock;
	}

	ctrl = nvmqf_create_ctrl(nvmqf_device, buf);
	if (IS_ERR(ctrl)) {
		ret = PTR_ERR(ctrl);
		goto out_unlock;
	}

	seq_file->private = ctrl;

out_unlock:
	mutex_unlock(&nvmqf_dev_mutex);
	kfree(buf);
	return ret ? ret : count;
}

static int nvmqf_dev_show(struct seq_file *seq_file, void *private)
{
	struct nvmq_ctrl *ctrl;
	int ret = 0;

	mutex_lock(&nvmqf_dev_mutex);
	ctrl = seq_file->private;
	if (!ctrl) {
		ret = -EINVAL;
		goto out_unlock;
	}

	seq_printf(seq_file, "instance=%d,cntlid=%d\n",
			ctrl->instance, ctrl->cntlid);

out_unlock:
	mutex_unlock(&nvmqf_dev_mutex);
	return ret;
}

static int nvmqf_dev_open(struct inode *inode, struct file *file)
{
	/*
	 * The miscdevice code initializes file->private_data, but doesn't
	 * make use of it later.
	 */
	file->private_data = NULL;
	return single_open(file, nvmqf_dev_show, NULL);
}

static int nvmqf_dev_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq_file = file->private_data;
	struct nvmq_ctrl *ctrl = seq_file->private;

	if (ctrl)
		nvmq_put_ctrl(ctrl);
	return single_release(inode, file);
}

static const struct file_operations nvmqf_dev_fops = {
	.owner		= THIS_MODULE,
	.write		= nvmqf_dev_write,
	.read		= seq_read,
	.open		= nvmqf_dev_open,
	.release	= nvmqf_dev_release,
};

static struct miscdevice nvmqf_misc = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name           = "nvmq-fabrics",
	.fops		= &nvmqf_dev_fops,
};

int nvmqf_init(void)
{
	int ret;

	nvmqf_default_host = nvmqf_host_default();
	if (!nvmqf_default_host)
		return -ENOMEM;

	nvmqf_class = class_create(THIS_MODULE, "nvmq-fabrics");
	if (IS_ERR(nvmqf_class)) {
		pr_err("couldn't register class nvmq-fabrics\n");
		ret = PTR_ERR(nvmqf_class);
		goto out_free_host;
	}

	nvmqf_device =
		device_create(nvmqf_class, NULL, MKDEV(0, 0), NULL, "ctl");
	if (IS_ERR(nvmqf_device)) {
		pr_err("couldn't create nvmq-fabris device!\n");
		ret = PTR_ERR(nvmqf_device);
		goto out_destroy_class;
	}

	ret = misc_register(&nvmqf_misc);
	if (ret) {
		pr_err("couldn't register misc device: %d\n", ret);
		goto out_destroy_device;
	}

	return 0;

out_destroy_device:
	device_destroy(nvmqf_class, MKDEV(0, 0));
out_destroy_class:
	class_destroy(nvmqf_class);
out_free_host:
	nvmqf_host_put(nvmqf_default_host);
	return ret;
}

void nvmqf_exit(void)
{
	misc_deregister(&nvmqf_misc);
	device_destroy(nvmqf_class, MKDEV(0, 0));
	class_destroy(nvmqf_class);
	nvmqf_host_put(nvmqf_default_host);

	BUILD_BUG_ON(sizeof(struct nvmf_common_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_connect_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_property_get_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_property_set_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_connect_data) != 1024);
}

// MODULE_LICENSE("GPL v2");

// module_init(nvmqf_init);
// module_exit(nvmqf_exit);
