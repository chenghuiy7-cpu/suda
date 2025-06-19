# SUDA Architecture

## Reviewing the SUDA Architecture Diagram
In the SUDA architecture, users control the SUDA CSD by calling the SUDA API, which further calls functions from libnvme and liburing to pass nvme commands to the kernel mode. The SUDA driver in kernel mode actually contains two parts: nvmq and qdma. nvmq processes nvme commands, such as converting virtual addresses that users want to process into actual physical addresses, and then calls qdma to transfer nvme commands to the SUDA CSD. The nvme command passes through qdma->mcdma, is stored in device memory as a data stream, and notifies the SUDA CSD's software stack. The software stack processes the commands, and if operator calls are needed, it calls the FPGA auxiliary scheduler -> FPGA operator controller -> FPGA operator to complete the computation.

[SUDAARCH](../images/SUDAArch.png)

## Host Software Stack Code Analysis

First are the libnvme and liburing libraries that provide application layer APIs. The main modified code is in `host/api/libnvme/src/nvme/ioctl.h`, `host/api/libnvme/src/nvme/ioctl.c`, and `host/api/libnvme/src/snia/*`. The ioctl.h and ioctl.c files provide compatibility with NVMe CS standard commands, while the files in the snia folder provide compatibility with SNIA computational storage device standard APIs. The APIs in snia are generally re-encapsulations of the ioctl.c and ioctl.h files. We currently only focus on the newly added functions in the ioctl.c and ioctl.h files. For example, the `nvme_operate_memory_range_set` function in `ioctl.h` is responsible for creating a memory set. It can be seen that it creates a data structure that conforms to the nvme command standard, and then passes it to the device with device number fd through `nvme_submit_admin_passthru`.

```c
/**
 * nvme_operate_memory_range_set - Create Memory Range Set For Program
 * @nsid: compute namespace id
 */
static inline int nvme_operate_memory_range_set(
	int fd,
	unsigned int nsid,
	int op,
	int numr,	//Number of Memory Ranges
	unsigned int* rsid,    //Result
	void* mmrange_descri//Memory Range Descriptor
){
	struct nvme_passthru_cmd cmd;
	cmd.nsid = nsid;
	cmd.opcode = nvme_mmrange_set_mgmt;
	cmd.cdw10 = (*rsid) << 16 | op;
	cmd.cdw11 = numr;
	cmd.timeout_ms = 0;
	cmd.metadata_len = 0;
	cmd.flags = 0;
	if(mmrange_descri!=NULL){
		cmd.addr = (__u64)(uintptr_t)mmrange_descri;
		cmd.data_len = (numr) * sizeof(union memory_range_set_decriptor);
	}else{
		cmd.addr = (__u64)NULL;
		cmd.data_len = 0;
	}
	return nvme_submit_admin_passthru(fd, &cmd, rsid);
}
int nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd, __u32 *result)
{
	return nvme_submit_passthru(fd, NVME_IOCTL_ADMIN_CMD, cmd, result);
}
static int nvme_submit_passthru(int fd, unsigned long ioctl_cmd,
				struct nvme_passthru_cmd *cmd, __u32 *result)
{
	struct timeval start;
	struct timeval end;
	int err;

	if (nvme_get_debug())
		gettimeofday(&start, NULL);
	
	err = ioctl(fd, ioctl_cmd, cmd);//Submit to device via IOCTL!

	if (nvme_get_debug()) {
		gettimeofday(&end, NULL);
		nvme_show_command(cmd, err, start, end);
	}
	
	//nvme_show_command(cmd, err, start, end);

	if (err >= 0 && result)
		*result = cmd->result;

	return err;
}
```

For asynchronous operations, the IOCTL method is not used. Kernel 6.0 supports initiating ioctl-like operations on nvme devices using iouring, but SUDA currently uses kernel 5.4. Although asynchronous operations are still implemented using iouring, because version 5.4 only supports asynchronous read and write operations to devices, SUDA specifically created an asynchronous device `ng0n1` to transmit commands by writing nvme commands to the device.

```c++
    int uring_fd = open("/dev/ng0n1", O_RDWR);
    // Initialize IO ring
    struct io_uring ring;
    ret = io_uring_queue_init(8, &ring, 0);
    if (ret) {
        fprintf(stderr, "ring setup failed\n");
        return 1;
    }
    // Prepare one command
    struct nvme_uring_cmd *cmd0 = (struct nvme_uring_cmd *)vecs[0].iov_base;
    // Configure first read command
    cmd0->opcode = 0x2;              // Read operation code
    cmd0->data_len = cut_size;       // Data length
    cmd0->addr = (unsigned long long)start_address; // Start address
    cmd0->nsid = nsid;               // Namespace ID
    cmd0->cdw10 = starting_bytes;    // Low 32 bits of starting bytes
    cmd0->cdw11 = starting_bytes >> 32; // High 32 bits of starting bytes
    cmd0->flags = 0;                 // Flags
    cmd0->cdw12 = cut_size;          // Read length
    cmd0->cdw13 = 0;
    cmd0->cdw14 = 0;
    cmd0->cdw15 = 0;
    cmd0->flags = 0;

    // Get submission queue entry and prepare write command
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_writev(sqe, uring_fd, &(vecs[0]), 1, 0);//Asynchronously write the command to ng0n1 device using writev
    // Submit IO requests
    ret = io_uring_submit(ring);
```

These nvme commands are passed to the nvmq driver for processing. The main code for this part is in `core.c` and `qdma.c` under `host/drivers/nvmq/`. In core.c, `nvmq_dev_ioctl` is responsible for handling requests from the management device (such as /dev/nvmq0), and `nvmq_ioctl` is responsible for handling requests from the IO device (such as /dev/nvmq0n1). Currently, there is no special judgment between the two, so although they should each have their own responsibilities logically, they can actually implement similar functions.

```c++
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

```

The IOCTL method will execute `nvmq_user_cmd`, which will copy the application-initiated command from user mode to kernel mode, and finally call the `nvmq_submit_user_cmd` function to start processing.

The above is the synchronous method. For the asynchronous method, through the writev method of iouring, it will actually trigger the function `nvmq_ns_chr_write_iter`, which will create the corresponding data structure for asynchronous operations and call `nvmq_submit_uring_cmd` to start processing. After simple processing, it is submitted to the work queue for other processes to continue processing:

```c++
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


```

Whether synchronous or asynchronous, they will both execute logic similar to the `nvmq_submit_user_cmd` function. The key is two parts: one is to map the user mode space involved in the nvme command, such as the descriptor needed by nvme to create memory range set, from user mode to kernel mode:

```c++
if(nvme_is_slm_rw(cmd))
		{
			ret = nvmq_blk_rq_map_user(q,req,NULL,ubuffer,bufflen,GFP_KERNEL);
		}else
			ret = blk_rq_map_user(q, req, NULL, ubuffer, bufflen,
				GFP_KERNEL);
```

The above code `blk_rq_map_user` and `nvmq_blk_rq_map_user` do not have essential differences in functionality, but `nvmq_blk_rq_map_user` can implement data copying of more than 128KB.

The other part is to submit the `request` containing the command to the block device layer:
```c++
blk_execute_rq(req->q, disk, req, 0);
	if (nvmq_req(req)->flags & NVME_REQ_CANCELLED)
		ret = -EINTR;
	else
		ret = nvmq_req(req)->status;
	if (result)
		*result = le64_to_cpu(nvmq_req(req)->result.u64);
```

The request is handed over to `nvme_qdma_queue_rq` in `qdma.c` for processing. Please ignore any statements under the `is_kernel` branch that are true, as this part is an outdated design. The `nvme_qdma_queue_rq` function has several key codes, one is the call to `nvme_qdma_map_data`, which converts the address list obtained from the previous map_user into an sgl list, and finally converts it into a prp list for use by the actual nvme command. Additionally, the `nvme_qdma_post_next_rsp` function requests qdma to wait to receive response data for an nvmq command. Furthermore, there is a small trick used by the driver:

```c++
if (queue->qid == 0 || nvme_is_fabrics(c)) {
		// Fill the empty sgl
		if (blk_rq_nr_phys_segments(rq) > 0) {
			ret = nvme_qdma_map_sg_inline(queue, req, c);
			if (unlikely(ret)) {
				pr_err("set inline sgl failed: %d\n", ret);
			}
		}
	} else {
		// Add sgl_buf to qdma_req->sgl
		// qdma_req->sgcnt += qdma_map_page(req->sgl_buf, qdma_req->sgl);
	}
```

If it is a qid 0 command, that is, a management command, then the user mode space parameters involved, such as the descriptor for downloading a program, will directly call the `nvme_qdma_map_sg_inline` function to transfer this space and the nvme command to the SUDA CSD in the same way. If these conditions are not met, then it needs to be processed like a normal NVMe CMD, first transmitting the CMD to the SUDA CSD, and then the SUDA CSD gets the address list of the parameters according to the prp list, and then reads the data from the host memory.

Finally, the function calls `nvme_qdma_post_send` to transmit the nvme command to the SUDA CSD via qdma.

## SUDA CSD Software Stack Code Analysis

The main code of the SUDA CSD software stack that needs attention is in `suda/device/software_stack/nf_spdk/lib/hlsacccompute` and `suda/device/software_stack/nf_spdk/lib/nvmf`. First, the nvme cmd transmitted by qdma is converted into a data stream on the FPGA of the SUDA CSD, transmitted to the CSD's memory through an mcdma (axi_dma), and the software stack allocates at least one poller on each core to check if there are new nvme cmds on the memory. This part of the code can be seen in the `nvmf_mcdma_poller_poll` function in `nvmf/mcdma.c`:

```c++
static int
nvmf_mcdma_poller_poll(struct spdk_nvmf_mcdma_transport *rtransport,
		      struct spdk_nvmf_mcdma_poller *rpoller)
{
	int count = 0;
	struct spdk_nvmf_mcdma_qpair *rqpair;
	uint64_t ticks = spdk_get_ticks();
	bool should_poll_unstarted = false;
	bool has_nvme_queues = !RB_EMPTY(&rpoller->qpairs);
	if (spdk_unlikely(ticks > rpoller->last_poll_ticks + rpoller->poll_unstarted_interval_ticks)) {
		should_poll_unstarted = true;
		rpoller->last_poll_ticks = ticks;
	}

	for(int i = 0; i < rpoller->num_mcdma_qp; i++) {
		if (has_nvme_queues || should_poll_unstarted) {
			struct spdk_mcdma_qp *mcdma_qp = &rpoller->mcdma_qps[i];
			count += spdk_axi_dma_poller(mcdma_qp->tx_ch);
			count += spdk_axi_dma_poller(mcdma_qp->rx_ch);
			}
			}
	RB_FOREACH(rqpair, qpairs_tree, &rpoller->qpairs) {
			nvmf_mcdma_qpair_process_pending(rtransport, rqpair, false);
	}
	return count;
}

```

After detecting the existence of nvme cmd, the poller will call the `nvmf_mcdma_qpair_process_pending` function to process the nvme cmd. This function will sequentially copy the data and payload of the unified command (if the parameter command is an admin command and has parameters, the parameters are sent with the command), until all copying is complete, and then call the `nvmf_mcdma_request_process` function for processing. `nvmf_mcdma_request_process` really starts processing the nvme cmd. As an example, this article will explain a `SLM_READ` operation, which reads data from device memory to host memory. First, focus on the execution of this function in the case where the case is `MCDMA_REQUEST_STATE_READY_TO_EXECUTE`. This part of the code will judge three situations: the size of the data to be copied is less than or equal to 1 block, greater than 1 block and less than or equal to two blocks, or greater than two blocks. For nvme cmd, if the data to be operated is less than or equal to 1 block, the physical address is stored in prp0 of the nvme cmd; if it is two blocks, the physical address of the first block is placed in prp0, and the physical address of the second block is placed in prp1; if it is greater than two blocks, the physical address of the first block is placed in prp0, and the addresses of other blocks are organized into a list, with the physical address of the list placed in prp0. The SUDA software stack allocates an additional 4KB ctx for each nvme cmd to store some intermediate information. Taking a copy of size 1 block as an example, it sets the from_iovecs and to_iovecs of ctx to indicate copying from the prp0 address to the device memory. It then calls the `spdk_thread_send_msg` function to send a request to a specific spdk thread responsible for memory copying.

```c++
else if((!nvmf_qpair_is_admin_queue(mcdma_req->req.qpair))&&((mcdma_req->req.cmd->nvme_cmd.nsid & SLM_MASK) != 0)//MEMORY NAMESPACE NSID
			&&(mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_READ||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_WRITE||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_FILL||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_COPY)){
				SPDK_DEBUGLOG(nvmf,"ENTER XX! OPC%d\n",mcdma_req->req.cmd->nvme_cmd.opc);
				struct spdk_nvme_cmd* cmd = &(mcdma_req->req.cmd->nvme_cmd);
				struct spdk_hlsacccompute_dev* dev = &(mcdma_req->qpair->device->compute);
				mcdma_req->req.rsp->nvme_cpl.sqid = rqpair->qpair.qid;
				mcdma_req->req.rsp->nvme_cpl.cid = mcdma_req->req.cmd->nvme_cmd.cid;
				mcdma_req->state = MCDMA_REQUEST_STATE_EXECUTING;
				unsigned long long starting_bytes = cmd->cdw11 << 32 | cmd->cdw10;
				unsigned int read_or_write_length = cmd->cdw12;
				unsigned int bsize = PAGE_SIZE;//= spdk_bdev_get_block_size(ns->bdev);
				void* ns_vaddr,*ns_paddr;
				int ret = spdk_hlsacccompute_devmem_lookup(dev,cmd->nsid&(~SLM_MASK),&(ns_vaddr),&(ns_paddr));
				SPDK_DEBUGLOG(nvmf,"VADDR %llx,PADDR%llx,starting_bytes%llx\n",ns_vaddr,ns_paddr,starting_bytes);
				struct  handc_ctx* ctx = (struct handc_ctx*) mcdma_req->data_buf;
				ctx->impl_thread = spdk_get_thread();
				if(mcdma_req->req.cmd->nvme_cmd.opc == SPDK_NVME_OPC_SLM_WRITE){
                    ...
				}else if(mcdma_req->req.cmd->nvme_cmd.opc == SPDK_NVME_OPC_SLM_READ){
					spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
						(uintptr_t)mcdma_req,"srfetpp");
				
					ctx->device = rqpair->device;
					if(read_or_write_length<=bsize){
						ctx->fsm_state = FETCH_DATA;
						ctx->from_size = 1;
						ctx->to_size = 1;
						ctx->to_iovecs[0].iov_base = NULL;
						ctx->to_iovecs[0].paddr = cmd->dptr.prp.prp1;
						ctx->to_iovecs[0].iov_len = read_or_write_length;
						ctx->from_iovecs[0].iov_base = ns_vaddr+starting_bytes;
						ctx->from_iovecs[0].paddr = spdk_vtophys(ctx->from_iovecs[0].iov_base,NULL);
						ctx->from_iovecs[0].iov_len = read_or_write_length;
						SPDK_DEBUGLOG(nvmf,"Get PRP ADDRESS%llx DATA ELM%llx ADDRESS%llx ELM%c\n",cmd->dptr.prp.prp1,*((uint64_t*)ctx->from_iovecs[0].iov_base),ctx->from_iovecs[0].iov_base,((char*)(ctx->from_iovecs[0].iov_base))[0]);
					}else if(read_or_write_length>bsize&&read_or_write_length<=2*bsize){
						ctx->fsm_state = FETCH_DATA;
						ctx->from_size = 2;
						ctx->to_size = 2;
						ctx->to_iovecs[0].iov_base = NULL;
						ctx->to_iovecs[0].paddr = cmd->dptr.prp.prp1;
						ctx->to_iovecs[0].iov_len = bsize;
						ctx->from_iovecs[0].iov_base = ns_vaddr+starting_bytes;
						ctx->from_iovecs[0].paddr = spdk_vtophys(ctx->from_iovecs[0].iov_base,NULL);
						ctx->from_iovecs[0].iov_len = bsize;
						ctx->to_iovecs[1].iov_base = NULL;
						ctx->to_iovecs[1].paddr = cmd->dptr.prp.prp2;
						ctx->to_iovecs[1].iov_len = read_or_write_length-bsize;
						ctx->from_iovecs[1].iov_base = ns_vaddr+starting_bytes+bsize;
						ctx->from_iovecs[1].paddr = spdk_vtophys(ctx->from_iovecs[1].iov_base,NULL);
						ctx->from_iovecs[1].iov_len = read_or_write_length-bsize;
						SPDK_DEBUGLOG(nvmf,"Get PRP ADDRESS%llx DATA ELM%llx ADDRESS%llx ELM%c\n",cmd->dptr.prp.prp1,*((uint64_t*)ctx->from_iovecs[0].iov_base),ctx->from_iovecs[0].iov_base,((char*)(ctx->from_iovecs[0].iov_base))[0]);
					}
					else{
						ctx->fsm_state = FETCH_PRP;
						ctx->from_size = 1;
						ctx->to_size = 1;
						ctx->from_iovecs[0].iov_base = NULL;
						ctx->from_iovecs[0].paddr = cmd->dptr.prp.prp2;
						ctx->to_iovecs[0].iov_base = mcdma_req->sgl_buf;
						ctx->to_iovecs[0].paddr = spdk_vtophys(mcdma_req->sgl_buf,NULL);
						ctx->from_iovecs[0].iov_len = PAGE_SIZE;
						ctx->to_iovecs[0].iov_len = PAGE_SIZE;
					
					}
					ctx->mcdma_req = mcdma_req;
					ctx->rtransport = rtransport;
					
					spdk_thread_send_msg(rqpair->device->handc_thread,compute_handc_op,ctx);
					continue;

```

The thread responsible for data transfer will run the `compute_handc_op` function, requesting mcdma to execute data transfer:

```c++
void compute_handc_op(void* ctx){
	struct handc_ctx *hc = (struct handc_ctx*) ctx;
	struct spdk_nvmf_mcdma_device* dev = hc->device;
	struct spdk_axi_dma_ctrl ctrl;
	ctrl.tdest = dev->compute_tx_channel->id;
	ctrl.tid = dev->compute_tx_channel->id;
	ctrl.tuser = 0;
	struct spdk_axi_dma_ch* ch;
	int iovcnt;
	//RX RECV
	ch = dev->compute_rx_channel;
	iovcnt = hc->to_size;
	struct spdk_axi_dma_io *io = spdk_simple_pool_get(&ch->io_pool);
    if (!io) {
        SPDK_ERRLOG("Failed to allocate spdk_axi_dma_io\n");
        return -ENOMEM;
    }
    io->ch = ch;
    io->iovs = hc->to_iovecs;
    io->iovcnt = iovcnt;
    io->cb = compute_handc_op_rx_impl;
    io->ctx = ctx;
    io->transfered_length = 0;
	
	
	SPDK_DEBUGLOG(nvmf,"RX IOVCNT%d\n",iovcnt);
    for (int i = 0; i < iovcnt; i++) {
        uint64_t len = io->iovs[i].iov_len;
        io->transfered_length += len;
    }
	hc->total_rx_bytes = io->transfered_length;
	hc->cur_rx_bytes = 0;


    spdk_env_axi_dma_rx_channel_recv(ch->env_ch, hc->to_iovecs, iovcnt, io);
	iovcnt = hc->from_size;
	//TX SEND
	for(int i=0;i<iovcnt;i++)
	{
		ch = dev->compute_tx_channel;
		io = spdk_simple_pool_get(&ch->io_pool);
		if (!io) {
			SPDK_ERRLOG("Failed to allocate spdk_axi_dma_io\n");
			return -ENOMEM;
		}
		io->ch = ch;
		io->iovs = &(hc->from_iovecs[i]);
		
		io->iovcnt = 1;
		io->cb = compute_handc_op_tx_impl;
		io->ctx = ctx;
		io->transfered_length = 0;
		memcpy(&io->ctrl, &ctrl, sizeof(struct spdk_axi_dma_ctrl));
		io->transfered_length = hc->from_iovecs[i].iov_len;
		spdk_env_axi_dma_tx_channel_send(ch->env_ch, &(hc->from_iovecs[i]), 1, io);
	}
	return;
}
```

The thread responsible for memory transfer will continuously poll the completion status of the memory transfer, and upon completion, call the completion function to notify the original thread that the operation has been completed:

```c++
void compute_handc_op_rx_impl(struct spdk_axi_dma_io *io, int status){
	
	struct handc_ctx *hc = (struct handc_ctx*) io->ctx;
	hc->cur_rx_bytes += io->status.transfered_bytes;
	SPDK_DEBUGLOG(nvmf,"CUR RX BYTES GET %d TOTAL BYTES%d\n",hc->cur_rx_bytes,hc->total_rx_bytes);
	//If data transfer has completed
	if(hc->cur_rx_bytes>=hc->total_rx_bytes){
		spdk_axi_dma_io_free(io);
		if(hc->impl_thread!=NULL){
			SPDK_DEBUGLOG(nvmf,"HC CURRXBYTES%d TOTALRXBYTES%d\n",hc->cur_rx_bytes,hc->total_rx_bytes);
			if(hc->to_iovecs[0].iov_base!=NULL){
				unsigned int* data = (unsigned int*)(hc->to_iovecs[0].iov_base);
				SPDK_DEBUGLOG(nvmf,"DATA DUMP %lx %lx %lx %lx\n",data[0],data[1],data[2],data[3]);
			}
			spdk_thread_send_msg(hc->impl_thread,compute_handc_impl,hc);
		}else{
			SPDK_ERRLOG("UNDEFINED OPERATION!\n");
		}
	}
	
	return;
}
void compute_handc_impl(void* ctx){
	struct handc_ctx *hc = (struct handc_ctx*) ctx;
	if(hc->fsm_state==FETCH_PRP){
		hc->fsm_state = FETCH_DATA;
	}else{
		hc->fsm_state = END_FETCH_DATA;
	}
	hc->mcdma_req->state = MCDMA_REQUEST_STATE_EXECUTED;
	nvmf_mcdma_request_process(hc->rtransport,hc->mcdma_req);
	return;
}
```

The original thread will execute the `nvmf_mcdma_request_process` function based on the execution, at which point the state will enter `MCDMA_REQUEST_STATE_EXECUTED`. In this state, it will automatically convert to the state `MCDMA_REQUEST_STATE_READY_TO_COMPLETE` by default.

```c++
case MCDMA_REQUEST_STATE_EXECUTED:
			spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_EXECUTED, 0, 0,
					  (uintptr_t)mcdma_req, (uintptr_t)rqpair);
                      
			mcdma_req->state = MCDMA_REQUEST_STATE_READY_TO_COMPLETE;
			
			//if kernel has not finished, continue to execute
			//ignore kernel begin and kernel end
			//SPDK_DEBUGLOG(nvmf,"Time Trace6 %lf s\n",(double)spdk_get_ticks()/spdk_get_ticks_hz());
			if((!nvmf_qpair_is_admin_queue(mcdma_req->req.qpair))&&((mcdma_req->req.cmd->nvme_cmd.nsid & SLM_MASK) != 0)//MEMORY NAMESPACE NSID
			&&(mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_READ||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_WRITE||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_FILL||
				mcdma_req->req.cmd->nvme_cmd.opc==SPDK_NVME_OPC_SLM_COPY)){
				SPDK_DEBUGLOG(nvmf,"ENTER XX!\n");
				struct spdk_nvme_cmd* cmd = &(mcdma_req->req.cmd->nvme_cmd);
				struct spdk_hlsacccompute_dev* dev = &(mcdma_req->qpair->device->compute);
				mcdma_req->req.rsp->nvme_cpl.sqid = rqpair->qpair.qid;
				mcdma_req->req.rsp->nvme_cpl.cid = mcdma_req->req.cmd->nvme_cmd.cid;
				unsigned long long starting_bytes = cmd->cdw11 << 32 | cmd->cdw10;
				int read_or_write_length = cmd->cdw12;
				unsigned long long len = (mcdma_req->req.cmd->nvme_cmd.rsvd2) | 
											(mcdma_req->req.cmd->nvme_cmd.rsvd3 << 32);
				unsigned char copy_format = mcdma_req->req.cmd->nvme_cmd.cdw12 >> 8;
				unsigned char nr = mcdma_req->req.cmd->nvme_cmd.cdw12+1;//0 based number of ranges
```

The `MCDMA_REQUEST_STATE_READY_TO_COMPLETE` state will perform final work and send the response of the nvme cmd back to the host:

```c++
case MCDMA_REQUEST_STATE_READY_TO_COMPLETE:
			spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_READY_TO_COMPLETE, 0, 0,
					  (uintptr_t)mcdma_req, (uintptr_t)rqpair);
			rc = request_transfer_out(&mcdma_req->req, &data_posted);
			assert(rc == 0); /* No good way to handle this currently */
			if (rc) {
				mcdma_req->state = MCDMA_REQUEST_STATE_COMPLETED;
			} else {
				mcdma_req->state = MCDMA_REQUEST_STATE_COMPLETING;
			}
			break;
```

Next, taking an important `nvme_execute_program` command as an example, this article will explain the work done by the software stack on the operator. Similarly, in the `nvmf_mcdma_request_process` function, this function creates a `spdk_hlsacccompute_request` structure `request`. The request will call the `program` associated with the request. SUDA currently supports three basic parameters passed in through the user API (more parameters are passed in through context). cparam1 is responsible for transmitting the context mask. Suppose there are 3 operators, cparam1[7:0], cparam1[15:8], and cparam1[23:16] respectively identify whether each operator needs to use the context. If cparam1[15:8] is 1, it means the second operator needs to use the operator. cparam2 is used to store the task priority, and cparam3 is used to store the running method (running in hardware, running in software, free scheduling). If the operator needs parameters, it needs to copy the context from the host, which requires processing through `spdk_thread_send_msg(rqpair->device->handc_thread,compute_handc_op,ctx);`. If no parameters are needed, it directly calls `spdk_thread_send_msg(mdev->compute_thread,hlsacccompute_run_request_unpreempt,request);`. Currently, a fixed priority is configured, where priorities below 120 trigger non-preemptive scheduling, and priorities above 120 must preempt.


```c++
}else if((!nvmf_qpair_is_admin_queue(mcdma_req->req.qpair))&&mcdma_req->req.cmd->nvme_cmd.nsid==2&&
			mcdma_req->req.cmd->nvme_cmd.opc == SPDK_NVME_OPC_PROGRAM_EXECUTE){
				mcdma_req->req.rsp->nvme_cpl.sqid = rqpair->qpair.qid;
				mcdma_req->req.rsp->nvme_cpl.cid = mcdma_req->req.cmd->nvme_cmd.cid;
				mcdma_req->state = MCDMA_REQUEST_STATE_EXECUTING;
				SPDK_DEBUGLOG(nvmf,"BEGIN TO EXECUTE PROGRAM!\n");
				spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
					(uintptr_t)mcdma_req, "readyexecprog");
				//Based on current needs, allocate virtual object
				//Request to run the program, register the return callback function
				//First, get a memory range set
				struct spdk_nvme_cmd cmd = mcdma_req->req.cmd->nvme_cmd;
				unsigned short rsid = cmd.rsvd2 >> 16;
				unsigned short pind = cmd.rsvd2;
				unsigned int numr = cmd.rsvd3;
				unsigned int dlen = cmd.mptr;
				//cparam1 ctx initialize sel
				unsigned long long cparam1 = cmd.cdw10 | (cmd.cdw11 << 32);
				//cparam2 request priority (7:0) request type(sw,hw,fusion)(15:8)
				unsigned long long cparam2 = cmd.cdw12 | (cmd.cdw13 << 32);
				unsigned long long cparam3 = cmd.cdw14 | (cmd.cdw15 << 32);

				unsigned char need_init_ctx[8] = {0};
				unsigned char priority = cparam2;

				unsigned long long temp_val = cparam1;
				unsigned char need_init_num = 0;
			

				struct spdk_nvmf_mcdma_device* mdev = mcdma_req->qpair->device;
				void* table = mdev->memrangeset_hash_tables;
				struct memory_range_sets* entry = NULL;
				int ret = spdk_cuckoo_table_lookup(table,rsid,&entry);
				if(ret!=0){
					mcdma_req->state = MCDMA_REQUEST_STATE_EXECUTED;
					mcdma_req->req.rsp->nvme_cpl.status.sc = 0x8B;//Invalid Memory Namespaces
					continue;
				}
				struct spdk_hlsacccompute_program* program = (mdev->compute).program_list[pind];
				if(program==NULL){
					mcdma_req->state = MCDMA_REQUEST_STATE_EXECUTED;
					mcdma_req->req.rsp->nvme_cpl.status.sc = 0x8F;//Invalid Program Pind
					continue;
				}
				struct spdk_hlsacccompute_dev* dev = &(mcdma_req->qpair->device->compute);
				pthread_mutex_lock(&(mdev->compute_mutex));

				struct spdk_hlsacccompute_request* request = spdk_hlsacccompute_create_request(&(mdev->compute),program);
				request->req_cb_args = mcdma_req;
				request->req_cb_fns = hlsacccompute_req_callback;
				request->priority = priority;
				request->dev = dev;
				//SPDK_NOTICELOG("MCDMAREQ %llx rsp%llx\n",mcdma_req,mcdma_req->req.rsp);
				request->run_way = cparam3;
				//SPDK_NOTICELOG("GET REQUEST RUN WAY%d\n",request->run_way);
				int m=0;
				for(int i=0;i<program->apply_operators_num;i++){
					need_init_ctx[i] = temp_val;
					temp_val = temp_val >> 8;
					if(need_init_ctx[i]==1){
						need_init_num++;
					}
				}
				//Lock?? MemPool!
				for(int k=0;k<program->input_channum;k++){
					struct spdk_hlsacccompute_virtual_object* ob = TAILQ_FIRST(&((mdev->compute).vo_pool));
					TAILQ_REMOVE(&((mdev->compute).vo_pool),ob,link);
					memcpy(ob,&(entry->quick_cache[m]),sizeof(struct spdk_hlsacccompute_virtual_object));
					++m;
					SPDK_DEBUGLOG(nvmf,"OB LEN %x OB VADDR%llx CUR USED%d\n",ob->iov_len,ob->iov_base,ob->cur_used);
					TAILQ_INSERT_HEAD(&(request->tx_vos[k]),ob,link);
				}
				for(int k=0;k<program->output_channum;k++){
					struct spdk_hlsacccompute_virtual_object* ob = TAILQ_FIRST(&((mdev->compute).vo_pool));
					TAILQ_REMOVE(&((mdev->compute).vo_pool),ob,link);
					memcpy(ob,&(entry->quick_cache[m]),sizeof(struct spdk_hlsacccompute_virtual_object));
					++m;
					SPDK_DEBUGLOG(nvmf,"OB LEN %x OB VADDR%llx\n",ob->iov_len,ob->iov_base);
					TAILQ_INSERT_HEAD(&(request->rx_vos[k]),ob,link);
				}
				struct  handc_ctx* ctx = (struct handc_ctx*) mcdma_req->data_buf;
				ctx->device = mcdma_req->qpair->device;
				ctx->mcdma_req = mcdma_req;
				ctx->rtransport = rtransport;
				pthread_mutex_unlock(&(mdev->compute_mutex));
				ctx->fsm_state = NON_OP;
				ctx->hls_request = request;
				ctx->impl_thread = spdk_get_thread();
				SPDK_DEBUGLOG(nvmf,"BLOCK CONTEXT NUM%d\n",need_init_num);
				if(request->program->apply_operators_num==0||(cparam1==0)){
					SPDK_DEBUGLOG(hlsacc,"RUN REQUEST PREEMPT%d\n",request->priority);
					mcdma_req->impl_thread = spdk_get_thread();
					if(request->priority<120)//Preempt if priority is high
						spdk_thread_send_msg(mdev->compute_thread,hlsacccompute_run_request_unpreempt,request);
					else
						spdk_thread_send_msg(mdev->compute_thread,hlsacccompute_run_request_preempt,request);
				}else{
					struct spdk_nvme_cmd* cmd = &(mcdma_req->req.cmd->nvme_cmd);
					//SPDK_NOTICELOG("WAITING FOR ACCCONTEXT %llx\n",request->acccontext[0]);	
					if(request->acccontext[1]!=NULL){
						SPDK_DEBUGLOG(nvmf,"WAITING FOR ACCCONTEXT %llx\n",request->acccontext[1]);	
					}
					if(need_init_num==1){	
						ctx->fsm_state = FETCH_DATA;
						ctx->to_size = 1;
						ctx->from_size = 1;
						ctx->from_iovecs[0].iov_base = NULL;
						ctx->from_iovecs[0].paddr = cmd->dptr.prp.prp1;
						ctx->from_iovecs[0].iov_len = 4096;
						int i=0;
						for(i=0;i<8;i++){
							if(need_init_ctx[i]==1) break;
						}
				
						
						ctx->to_iovecs[0].iov_base = (unsigned long long)(request->acccontext[i])+4096;
						ctx->to_iovecs[0].iov_len =  4096;
						ctx->to_iovecs[0].paddr = spdk_vtophys(ctx->to_iovecs[0].iov_base,NULL);
						//SPDK_NOTICELOG("VADDR%llx PADDR%llx\n",ctx->to_iovecs[0].iov_base,ctx->to_iovecs[0].paddr);
						assert(cmd->dptr.prp.prp1!=NULL);
						if(cmd->dptr.prp.prp1==NULL){
							SPDK_ERRLOG("Failed to Fetch Block Context!\n");

						}else{
							SPDK_DEBUGLOG(nvmf,"PRP ADDRESS%llx\n",cmd->dptr.prp.prp1);
						}
						//SPDK_NOTICELOG("FETCH ONE BLOCK CONTEXT");
						spdk_thread_send_msg(rqpair->device->handc_thread,compute_handc_op,ctx);
					}else if(need_init_num==2){
						ctx->fsm_state = FETCH_DATA;
						ctx->to_size = 2;
						ctx->from_size = 2;
						int i=0;
						for(i=0;i<8;i++){
							if(need_init_ctx[i]==1) break;
						}
						ctx->from_iovecs[0].iov_base = NULL;
						ctx->from_iovecs[0].paddr = cmd->dptr.prp.prp1;
						ctx->from_iovecs[0].iov_len =  4096;
						ctx->to_iovecs[0].iov_base = request->acccontext[i]->context.static_data;
						ctx->to_iovecs[0].iov_len =  4096;
						ctx->to_iovecs[0].paddr = spdk_vtophys(ctx->to_iovecs[0].iov_base,NULL);
						for(;i<8;i++){
							if(need_init_ctx[i]==1) break;
						}
						ctx->from_iovecs[1].iov_base = NULL;
						ctx->from_iovecs[1].paddr = cmd->dptr.prp.prp2;
						ctx->from_iovecs[1].iov_len =  4096;
						ctx->to_iovecs[1].iov_base = request->acccontext[i]->context.static_data;
						ctx->to_iovecs[1].iov_len =  4096;
						ctx->to_iovecs[1].paddr = spdk_vtophys(ctx->to_iovecs[1].iov_base,NULL);
						SPDK_DEBUGLOG(nvmf,"FETCH TWO BLOCK CONTEXT");
						spdk_thread_send_msg(rqpair->device->handc_thread,compute_handc_op,ctx);
					}
					else if(need_init_num>2){
						SPDK_DEBUGLOG(nvmf,"FETCH MULTI BLOCK CONTEXT");
						ctx->fsm_state = FETCH_PRP;
						ctx->to_size = 1;
						ctx->from_size = 1;
						ctx->from_iovecs[0].iov_base = NULL;
						ctx->from_iovecs[0].paddr = cmd->dptr.prp.prp2;
						ctx->from_iovecs[0].iov_len = PAGE_SIZE;
						ctx->to_iovecs[0].iov_base = mcdma_req->sgl_buf;
						ctx->to_iovecs[0].iov_len = PAGE_SIZE;
						ctx->to_iovecs[0].paddr = spdk_vtophys(ctx->to_iovecs[0].iov_base,NULL);
						spdk_thread_send_msg(rqpair->device->handc_thread,compute_handc_op,ctx);
					}
				}
				continue;
```

The preemptive and non-preemptive functions are defined as follows:

```c++
void hlsacccompute_run_request_preempt(void* ctx){
	
	struct spdk_hlsacccompute_request* request = ctx;
	
	SPDK_DEBUGLOG(nvmf,"PREEMPT\n");
	spdk_hlsacccompute_run_request(request->dev,request,true);
}

void hlsacccompute_run_request_unpreempt(void* ctx){
	struct spdk_hlsacccompute_request* request = ctx;
	struct spdk_nvmf_mcdma_request* mcdma_req = request->req_cb_args;
	spdk_trace_record(TRACE_MCDMA_REQUEST_STATE_HLS_EXEC, 0, 0,
		(uintptr_t)mcdma_req, "run request ");
	
	SPDK_DEBUGLOG(nvmf,"UNPREEMPT\n");
	spdk_hlsacccompute_run_request(request->dev,request,false);
}
```

Next, it will enter the `spdk_hlsacccompute_run_request` function, which is in `lib/hlsacccompute/hlsacccompute.c`. This function will first check the global operator list to confirm if the requested operators are idle. If they are not idle, they are added to the waiting list. If they are idle, operators are allocated. Allocating operators will call the applyops command corresponding to the program. After receiving confirmation of preemption completion, mcdma resources are allocated, opening interfaces for sending/receiving data sources/destinations. Once all preparations are complete, the computation is completed.

```c++
int spdk_hlsacccompute_run_request(struct spdk_hlsacccompute_dev *dev, struct spdk_hlsacccompute_request *request, bool preempt)
{
    request->tracker.responding_time = spdk_get_ticks();
   
    int ret = __spdk_hlsacccompute_run_request(dev,request,preempt);
    if(ret==-1){
        spdk_hlsacccompute_prepare_for_next_sched(request,dev->schedule_strategy);
    }
    return ret;
}

```

First, choose whether to run in hardware or software according to requirements:
```c++

int __spdk_hlsacccompute_run_request(struct spdk_hlsacccompute_dev *dev, struct spdk_hlsacccompute_request *request, bool preempt)
{
    dev->dev_state = HLSDEV_RUN_REQUEST;
    ...
    SPDK_DEBUGLOG(hlsacc,"CURRENT REQUEST ID%d\n",request->request_id);
    if (request->tracker.status == ACC_REQ_APPLYING || request->tracker.status == ACC_REQ_WAITING)
    {
        start_ticks = spdk_get_ticks();
        //SPDK_NOTICELOG("BEGIN REQUEST!\n");
        char sw_name[30];
        struct spdk_cpuset *cpuset;
        // spdk_cpuset_set_cpu()
        switch (request->run_way)
        {
        case 0: // ignored default hardware
            break;
        case 1:
	           //Call software execution method
                spdk_thread_send_msg(spdk_get_thread(), spdk_hlsacccompute_run_easy_sw, request);
            break;
        case 2:
            // hardware
            break;
        default:
            SPDK_ERRLOG("Undefined program type\n");
            return -1;
        }
        for (int i = 0; i < program->apply_operators_num; i++)
        {
         //Check if all operators are idle, if multiple operators with the same functionality are actually deployed, at least one is idle
        }
      
        struct spdk_hlsacccompute_request *need_pause_request[SPDK_HLSACCCOMPUTE_MAX_OPERATOR_CAP];
        int pause_request_num = 0;

```

Then, mark the requests that need to be preempted and request the corresponding MCDMA resources:
```c++
        if (spdk_likely(preempt))
        {
           //If preemption is needed, towards the corresponding
        }
        //TODO There are some design issues here that need to be resolved
        //When preempting requests, the channel appears to be released, but it actually isn't!
        //This could potentially cause serious performance issues in some situations, leading to computation becoming completely stuck, which is an issue that needs to be fixed in the future!
        request->tx_channel_num = request->program->input_channum;
		/*
		* Request the necessary DMA resources
		*/
        for (int i = 0; i < request->program->output_channum; i++)
        {
            struct spdk_hlsacccompute_channel *ch;
            if (request->rx_channel[i] == NULL)
            {
                ch = spdk_hlsacccompute_apply_channel(dev, false);
                request->rx_channel[i] = ch;
                if(ch==NULL)
                    return -1;
            }
            else
                ch = request->rx_channel[i];

            ch->virtual_channel_id = i;
            ch->req = request;
        }
        for (int i = 0; i < request->program->input_channum; i++)
        {
            struct spdk_hlsacccompute_channel *ch;
            if (request->tx_channel[i] == NULL)
            {
                ch = spdk_hlsacccompute_apply_channel(dev, true);
                request->tx_channel[i] = ch;
                if(ch==NULL)
                    return -1;
            }
            else
                ch = request->tx_channel[i];
            ch->virtual_channel_id = i;
            ch->req = request;
            ch->dest_id = (request->dynamic_id_map[request->program->input_channel_destination[i] << 4] << 4) | (request->program->input_channel_destination[i] & 0xF);
        }
```

Then, perform operator remapping. If there are multiple operators with the same functionality, such as 0, 1, 2, and the user only needs to request 1, and only the second one is free, then the operator id that needs to be processed is changed to 1:

```c++
        request->rx_channel_num = request->program->output_channum;
        // Operator remapping, assuming there are multiple operators with the same functionality, with physical numbers 1,2,3
        id_remap(request->program->applyops, request->applyops, request->dynamic_id_map, request->acccontext, request->tx_channel, request->rx_channel, request->program->input_channum, request->program->output_channum);
        id_remap(request->program->pauseops, request->pauseops, request->dynamic_id_map, NULL, NULL, NULL, 0, 0);
        id_remap(request->program->freeops, request->freeops, request->dynamic_id_map, NULL, NULL, NULL, 0, 0);
        request->tracker.waiting_preempt_req = preempt_ops_num;
       // SPDK_NOTICELOG("FIRST REQUEST SETUP\n");

If preemption is needed, first pause the requests occupying the operators:
```c++
	if (preempt_ops_num != 0)
        {
            request->tracker.status = ACC_REQ_PREEMPTING;
             // Generate Pause Request
             for (int i = 0; i < pause_request_num; i++)
             {
                 need_pause_request[i]->next_request = request;
                 spdk_hlsacccompute_pause_request(dev, need_pause_request[i], need_pause_request[i], cqe_recv_norm_cb);
             }
        }
        else
        {
            goto sqe_execute;
        }
        return ret;
    }

```

Count the number of preemptions to determine when all preemptions have been completed:
```c++
    else if (request->tracker.status == ACC_REQ_PREEMPTING)
    {
        request->tracker.waiting_preempt_req--;
```

Request operators, with the key being `execute_sqe`:
```c++
        //SPDK_NOTICELOG("PREEMPTING FINISHED\n");
    sqe_execute:
        if (request->tracker.waiting_preempt_req > 0)
            return 0;
        request->tracker.status = ACC_REQ_EXECUTING;
        TAILQ_INSERT_TAIL(&dev->working_queue, request, link);
        //SPDK_DEBUGLOG(hlsacc,"SUBMIT HW REQID%d\n",request->request_id);
        int tail = (*(dev->bar->ppair_cqtailer));
        //for(int i=0;i<request->program->apply_operators_num;i++){
            //request->config[i]->state = WORKING;
        //    request->config[i]->state = IDLE;
        //}
        // request->acccontext[0]->context.static_data
        int cid = execute_sqe(dev, request->applyops, request, (void *)request, cqe_recv_norm_cb);
        
        // Directly start allocating channels
        for (int i = 0; i < request->program->input_channum; i++)
        {
            struct spdk_hlsacccompute_channel *ch;

            ch = request->tx_channel[i];
            ch->virtual_channel_id = i;
            ch->req = request;
            ch->dest_id = (request->dynamic_id_map[request->program->input_channel_destination[i] << 4] << 4) | (request->program->input_channel_destination[i] & 0xF);
        }
        request->tx_channel_num = request->program->input_channum;
        for (int i = 0; i < request->program->output_channum; i++)
        {
            struct spdk_hlsacccompute_channel *ch = request->rx_channel[i];

            request->rx_channel[i] = ch;
            ch->virtual_channel_id = i;
            ch->req = request;
        }
        request->rx_channel_num = request->program->output_channum;
        
        // Pretend to sleep for a moment, what if the command we sent out already received a result?
        for (int i = 0; i < 64; i++);

        {
            request->tracker.status = ACC_REQ_WAITING_CHANNEL;
            SPDK_DEBUGLOG(hlsacc,"WAITING RUN!\n");
        }
        spdk_hlsacccompute_poll_cq(dev);
    }
```

After the operator request is completed, call the DMA channel to receive and send data:
```c++
    else if (request->tracker.status == ACC_REQ_WAITING_CHANNEL)
    {
        //SPDK_NOTICELOG("ENTERING RUN!\n");
        end_ticks = spdk_get_ticks();
        uint64_t duration_us = (end_ticks-start_ticks)/(spdk_get_ticks_hz()/1000000);
        //SPDK_NOTICELOG("DURATION US%llx\n",duration_us);
        dev->dev_state = HLSDEV_IDLE;
        for (int i = 0; i < request->rx_channel_num; i++)
        {
            struct spdk_hlsacccompute_channel *ch = request->rx_channel[i];
            //SPDK_NOTICELOG("channelid%dRX VOS DATA BASEADD%llx LEN %d CUR_USED %ld\n",ch->channel_id,request->rx_vos[0].tqh_first->iov_base,request->rx_vos[0].tqh_first->iov_len,request->rx_vos[0].tqh_first->cur_used);
            ch->channel_recv(ch);
        }
        spdk_wmb();
        for (int i = 0; i < request->tx_channel_num; i++)
        {
            struct spdk_hlsacccompute_channel *ch = request->tx_channel[i];
         
            //SPDK_NOTICELOG("channelid%dTX VOS DATA BASEADD%llx LEN %d CUR_USED %ld\n",ch->channel_id,request->tx_vos[0].tqh_first->iov_base,request->tx_vos[0].tqh_first->iov_len,request->tx_vos[0].tqh_first->cur_used);
            ch->channel_send(ch);
        }
        request->tracker.status = ACC_REQ_EXECUTING;
        // Modify the state of opconfig
        for (int i = 0; i < request->program->apply_operators_num; i++)
        {
            request->op_elm[i]->state = WORKING;
            //SPDK_DEBUGLOG(hlsacc,"ELM ADDRESS%llx\n",request->op_elm[i]);
        }
        //SPDK_DEBUGLOG(hlsacc,"CHANGE STATE!\n");
        SPDK_DEBUGLOG(hlsacc,"TX CHANNEL NUM %d RX_CHANNEL_NUM!\n",request->tx_channel_num,request->rx_channel_num);
    }
    return ret;
}
```

### Computation Completion Flag
Additionally, we need to focus on how to determine if a request being executed has finished. Currently, MCDMA sends AXIS data streams to operators, where normal data's TUSER signal is 0, and an invalid packet representing the end has a TUSER signal of 0xff. This packet is not processed by the operator but is directly passed to the next level, eventually propagating to the MCDMA receive channel. The SUDA software stack checks if a TUSER signal has been received, and if the signal is 0xff, it indicates that the computation has finished. This is why the receive space must be one page larger than the maximum space the operator is expected to output.
[lastprob](../images/lastprob.png)

### Post-Computation Cleanup Work

After receiving the TUSER signal, the software stack calls the callback function for cleanup, which further calls the `spdk_hlsacccompute_free_request` function to reclaim resources and trigger a configurable scheduling function:
```c++
int spdk_hlsacccompute_free_request(struct spdk_hlsacccompute_dev *dev, struct spdk_hlsacccompute_request *request, bool sendsqe)
{
  
	// Reclaim resources needed for requesting operators
        ...
    // Automatically trigger scheduling
    spdk_hlsacccompute_schedule_request(dev);
    return 0;
}
```

The `spdk_hlsacccompute_schedule_request` function checks the waiting queue to determine if any requests can be initiated. If not, it calls the `spdk_hlsacccompute_prepare_for_next_sched` function, which implements a **configurable scheduler** for reordering requests.
```c++
// Whether there is a need to add a new_request type to mark the generation of a new type
void spdk_hlsacccompute_schedule_request(struct spdk_hlsacccompute_dev *dev)
{
    if(dev->dev_state != HLSDEV_IDLE){
        dev->waiting_queue_ptr = 0;
        return;
    }
    dev->dev_state = HLSDEV_SCHEDING;
    TAILQ_HEAD(, spdk_hlsacccompute_request)
    re_sched_queue;
    struct spdk_hlsacccompute_request *request;
    TAILQ_INIT(&re_sched_queue);
    //struct spdk_hlsaccompute_request* last_req = TAILQ_LAST()
    while (!TAILQ_EMPTY(&dev->waiting_queue[dev->waiting_queue_ptr]))
    {
        //SPDK_DEBUGLOG(hlsacc,"SCHED\n");
        request = TAILQ_FIRST(&dev->waiting_queue[dev->waiting_queue_ptr]);
        TAILQ_REMOVE(&dev->waiting_queue[dev->waiting_queue_ptr], request, link);
        int ret = 0;
        if(request->priority < 220){
            //SPDK_DEBUGLOG(hlsacc,"DONOT PREEMPT\n");
            ret = __spdk_hlsacccompute_run_request(dev, request, false);
        }else{
            //SPDK_DEBUGLOG(hlsacc,"NEED PREEMPT\n");
            ret = __spdk_hlsacccompute_run_request(dev, request, true);
        }
        if (ret == -1)
        {
            TAILQ_INSERT_TAIL(&re_sched_queue, request, link);
        }
    }
    while (!TAILQ_EMPTY(&re_sched_queue))
    {
        request = TAILQ_FIRST(&(re_sched_queue));
        TAILQ_REMOVE(&re_sched_queue, request, link);
        spdk_hlsacccompute_prepare_for_next_sched(request, dev->schedule_strategy);
    }
    dev->waiting_queue_ptr = 0;
    dev->dev_state = HLSDEV_IDLE;
}

```

Configurable scheduling function:

```c++
void spdk_hlsacccompute_prepare_for_next_sched(struct spdk_hlsacccompute_request *request, int sched_strategy)
{
    //SPDK_DEBUGLOG(hlsacc,"REQUEST%d PREPARE FOR NEXT SCHED%d\n",request->request_id,sched_strategy);
    SPDK_DEBUGLOG(hlsacc,"PREPARE FOR NEXT SCHED request id%d\n",request->request_id);
    sched_strategy = SCHED_BASIC_PRIORITY_PREEMPT;
    if (sched_strategy == SCHED_FCFS||sched_strategy == SCHED_BASIC_PRIORITY_PREEMPT)
    {
        struct spdk_hlsacccompute_dev *dev = request->dev;
        if(request->dev!=NULL)
        TAILQ_INSERT_TAIL(&(dev->waiting_queue[(dev->waiting_queue_ptr + 0) % SPDK_HLSACCCOMPUTE_SLICE_WINDOWS]), request, link);
        request->tracker.status = ACC_REQ_WAITING;
    } else if (sched_strategy == SCHED_BASIC_SOFTWARE_COCACULATE){
        struct spdk_hlsacccompute_dev *dev = request->dev;
        int oldway = request->run_way;
        request->run_way=1;
        int ret = __spdk_hlsacccompute_run_request(request->dev,request,false);
        if(ret == -1){
            request->run_way=oldway;
            if(request->dev!=NULL)
            TAILQ_INSERT_TAIL(&(dev->waiting_queue[(dev->waiting_queue_ptr + 0) % SPDK_HLSACCCOMPUTE_SLICE_WINDOWS]), request, link);
            request->tracker.status = ACC_REQ_WAITING;
        }
    
    }

    return 0;
}
```

## SUDA CSD Hardware Analysis

In the FPGA operator pool, operators are connected to data stream interconnects, with each operator having a separate controller (Operator) managed by an upper-level auxiliary scheduler (AssScheduler).

### Controller Structure

#### Task Switching Mechanism Based on Data Processing Granularity

This paper uses a task switching mechanism based on data processing granularity, dividing the data to be processed according to a certain granularity. When the operator has processed a certain granularity of data, the operator is required to save intermediate variables to the data cache. At this point, a switching point is inserted, allowing task switching. The granularity switching mechanism designed in this paper has four stages:

1. **Waiting for the preemption window.** When the scheduler initiates a switching request, the controller receives the request and waits for the switching point to arrive.
2. **Pausing IO transactions and operator work.** When the controller confirms that the switching point has arrived, it pauses data flow into/out of the operator and pauses the operator's computation.
3. **Switching context.** The controller packages and sends out data from the queue and data cache, then the controller receives new context information, parses the information, and writes the data back to the queue and data cache.
4. **Resuming IO transactions and operator work.** According to the requirements of the new task, resume the input/output data flow of the operator and resume the operator's work.

The first stage requires modifying the operator to insert preemption windows during its execution. When the operator reaches the preemption window, it needs to send a signal to notify the controller that preemption is now allowed. If the controller has received the switching request and the operator's permission to preempt, it moves to the next stage. In this design, in the second stage, when the scheduler initiates a switching request, the scheduler terminates the upstream data flow, but the data flowing towards the operator from upstream cannot be directly cleared. The controller first allows data to flow into the operator's FIFO and prohibits the operator from working, preventing the operator from continuing to read data from the FIFO and leaving the preemption window. In the third stage, the controller needs to check the data in the data cache and multiple operator FIFOs, save it in the context, and restore data from the new context. The FIFO contains more than one data packet, so when saving FIFO data to the context, not only must the FIFO sequence number to which each data belongs be distinguished, but also the number of data packets and the length of each data packet must be recorded. In the fourth stage, the controller only needs to directly activate the operator, while the inflow of upstream data flow is left to the higher-level scheduler.

Different operations in different stages have different computation and data requirements. To save resources, this paper summarizes the number of computations and registers required for different operations in different stages, as shown in the table. This design ensures that the controller state machine executes only one operation at a time and reuses resources as much as possible, ensuring that the controller uses as few adders and registers as possible.

###### Resource statistics for different stages of context switching

| **Stage** | **Work** | **Operator Requirements** | **Register Requirements** |
|---------|---------|--------------|--------------|
| Waiting for preemption window | Counting unfinished transaction signals | Needs one (recording the number of unfinished transactions) | Needs one |
| Pausing IO transactions and operator work | Stopping FIFO pop, resetting the operator | Not needed | Not needed |
| Switching context | Saving data cache. Reading data of a specified size from the data cache and sending it sequentially | Needs two (saving data and reading size) | Needs one |
|  | Receiving data cache. Receiving data of a specified size and writing it to the data cache sequentially | Needs two (saving data and writing size) | Needs one |
|  | Counting the depth of the 0th to Nth FIFO | Needs two (recording total FIFO depth and the currently selected FIFO sequence number) | Needs one |
|  | Reading data packets from the Mth FIFO (starting from 0) | Needs two (recording data packet length and data packet sequence number) | Needs one |
|  | Recording the saved data packet length and selecting to record the next data packet | Needs two (recording data packet length and data packet sequence number) | Needs one |
|  | Updating the FIFO sequence number M to be saved | Needs one (recording the current FIFO sequence number) | Needs one |
|  | Restoring data packets from the Mth FIFO (starting from 0) | Needs two (recording data packet length and data packet sequence number) | Needs one |
|  | Selecting to restore the next data packet | Needs two (recording data packet length and data packet sequence number) | Needs one |
|  | Updating the FIFO sequence number M to be restored | Needs one (recording the current FIFO sequence number) | Needs one |
| Resuming IO transactions and operator work | Activating the operator, allowing FIFO pop | Not needed | Not needed |

[State Transition](../images/controllerflow.png)

### Auxiliary Scheduler Logic

[Auxiliary Scheduler Structure](../images/assflow.png)

Since a user's requested computation task may involve requesting multiple operators, the SoC software stack needs to check the status of multiple operators sequentially to ensure that all operators are requested before the task executes. In this paper, polling is used for implementation. From the previous sections, it can be observed that the software stack in this paper has considerable pressure on polling, so there is a need to reduce polling tasks as much as possible. This paper designed an auxiliary scheduler where the software scheduler only needs to package multiple controller commands into one scheduling command for the auxiliary scheduler, which will parse the software stack commands and further control the operators' controllers. For ease of management and control, this paper adopted an SQE/CQE form similar to NVMe for submitting and completing scheduling commands. The auxiliary scheduler exposes two ring buffers to the software stack: a submission buffer and a completion buffer. The software stack writes scheduling commands to the submission buffer, and the auxiliary scheduler writes scheduling results to the completion buffer.

The figure below shows the SQE and CQE structure of the auxiliary scheduler.
[SCQE](../images/assisentcmd.png)

An SQE consists of a command header and multiple command payloads. The command header includes the following elements:

1. cid. The ID of the command. The auxiliary scheduler will write the CID back to the CQE, and the software scheduler calls the corresponding callback function based on the CID.
2. opc. The operation code, which is the same as the operations supported by the operator controller.
3. ops num. Records how many computation units need to be batch controlled.
4. cmd len. Records the length of the SQE.

A batch APPLY SQE, in addition to the command header, requires command payload 1 to command payload 3 to be added sequentially for each additional computation unit requested, telling the auxiliary scheduler the context address of the operator and where the operator's data output should be routed. In this research's implementation, at most one computation unit can have seven data outputs. For commands other than APPLY, only one command payload 4 is needed, where the op list in command payload 4 records the operator numbers to be batch processed. This research's implementation is designed to support batch processing of up to eight computation units at once. For completion CQEs, regardless of the type of command, this research only defined the cid field and the state field to track the completion status of the command.

For command processing, the auxiliary scheduler completes commands in the submission ring buffer sequentially, but the batch processing of computation units is parallel. The figure shows the processing of the auxiliary scheduler. Internally, the auxiliary scheduler has three pointers for the submission ring buffer: sq_header and sq_tailer are directly exposed to the SoC, while inner_sq_tailer is maintained internally. When the computation control plane writes a command to the auxiliary scheduler, it also updates the sq_tailer pointer. The auxiliary scheduler continuously compares sq_tailer and inner_sq_tailer until they are different, indicating that a new command has arrived. The auxiliary scheduler then parses the command and converts it into requests sent to the corresponding computation units. Since computation unit controllers process requests at different speeds, responses may arrive out of order. For this reason, the auxiliary scheduler internally maintains an outstanding_cmd_num variable to record the number of requests that have not yet been completed. When all responses have been received, outstanding_cmd_num is 0, indicating that all commands have been processed. At this point, the CQE is written to the completion ring buffer, and the cq_tailer pointer is updated. The software scheduler, similar to the auxiliary scheduler, maintains an inner_cq_tailer pointer and continuously polls, comparing whether the values of cq_tailer and inner_cq_tailer are the same. If they are different, it indicates that a response has been received, at which point the CQE is read and the correct callback function is executed based on the cid.