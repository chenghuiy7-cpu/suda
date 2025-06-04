/* SPDX-License-Identifier: MIT */
/*
 * Description: check that STDOUT write works
 */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "helpers.h"
#include "liburing.h"


/**
 * struct nvme_uring_cmd - nvme passthrough command structure
 * @opcode:	Operation code, see &enum nvme_io_opcodes and &enum nvme_admin_opcodes
 * @flags:	Not supported: intended for command flags (eg: SGL, FUSE)
 * @rsvd1:	Reserved for future use
 * @nsid:	Namespace Identifier, or Fabrics type
 * @cdw2:	Command Dword 2 (no spec defined use)
 * @cdw3:	Command Dword 3 (no spec defined use)
 * @metadata:	User space address to metadata buffer (NULL if not used)
 * @addr:	User space address to data buffer (NULL if not used)
 * @metadata_len: Metadata buffer transfer length
 * @data_len:	Data buffer transfer length
 * @cdw10:	Command Dword 10 (command specific)
 * @cdw11:	Command Dword 11 (command specific)
 * @cdw12:	Command Dword 12 (command specific)
 * @cdw13:	Command Dword 13 (command specific)
 * @cdw14:	Command Dword 14 (command specific)
 * @cdw15:	Command Dword 15 (command specific)
 * @timeout_ms:	If non-zero, overrides system default timeout in milliseconds
 * @result:	Set on completion to the command's CQE DWORD 0 controller response
 */
struct nvme_uring_cmd {
	__u8	opcode;
	__u8	flags;
	__u16	rsvd1;
	__u32	nsid;
	__u32	cdw2;
	__u32	cdw3;
	__u64	metadata;
	__u64	addr;
	__u32	metadata_len;
	__u32	data_len;
	__u32	cdw10;
	__u32	cdw11;
	__u32	cdw12;
	__u32	cdw13;
	__u32	cdw14;
	__u32	cdw15;
	__u32	timeout_ms;
	__u32	result;
};

static int test_nvme_io_fixed(struct io_uring *ring)
{
	printf("Begin To Open NVME Char dev\n");
    int fd = open("/dev/ng0n1",O_WRONLY);
    if(fd<0){
        fprintf(stderr,"Failed to Open NG0N1 Device\n");
        return 1;
    }
	struct io_uring_cqe *cqe;
	struct io_uring_sqe *sqe;
	struct iovec vecs[2];
	int i, ret;

	posix_memalign(&vecs[0].iov_base, 4096, 4096);
    posix_memalign(&vecs[1].iov_base, 4096, 4096);
    struct nvme_uring_cmd *cmd = vecs[0].iov_base;
	vecs[0].iov_len = sizeof(struct nvme_uring_cmd);

    cmd->opcode = 0x2;
    cmd->data_len = 4096;
    cmd->addr = (unsigned long long)(vecs[1].iov_base);
    cmd->nsid = 1;
    cmd->cdw10 = 0;
    cmd->cdw11 = 0;
    cmd->flags = 0;
    cmd->cdw12 = 0;
    cmd->cdw13 = 0;
    cmd->cdw14 = 0;
    cmd->cdw15 = 0;
    
	ret = io_uring_register_buffers(ring, vecs, 1);
	if (ret) {
		fprintf(stderr, "Failed to register buffers: %d\n", ret);
		return 1;
	}

	sqe = io_uring_get_sqe(ring);
	if (!sqe) {
		fprintf(stderr, "get sqe failed\n");
		goto err;
	}
	io_uring_prep_write_fixed(sqe, fd, vecs[0].iov_base,
					vecs[0].iov_len, 0, 0);
	sqe->user_data = 1;

	ret = io_uring_submit(ring);

	if (ret < 0) {
		fprintf(stderr, "sqe submit failed: %d\n", ret);
		goto err;
	} else if (ret != 1) {
		fprintf(stderr, "Submitted Failed ret%d\n",ret);
		goto err;
	}

	for (i = 0; i < 1; i++) {
		ret = io_uring_wait_cqe(ring, &cqe);
		if (ret < 0) {
			fprintf(stderr, "wait completion %d\n", ret);
			goto err;
		}
		if (cqe->res < 0) {
			fprintf(stderr, "I/O write error on %lu: %s\n",
					(unsigned long) cqe->user_data,
					 strerror(-cqe->res));
			goto err;
		}
		io_uring_cqe_seen(ring, cqe);
	}
	io_uring_unregister_buffers(ring);
	free(vecs[0].iov_base);
	return 0;
err:
	return 1;
}


int main(int argc, char *argv[])
{
	struct io_uring ring;
	int ret;

	if (argc > 1)
		return 0;

	ret = io_uring_queue_init(8, &ring, 0);
	if (ret) {
		fprintf(stderr, "ring setup failed\n");
		return 1;
	}

	ret = test_nvme_io_fixed(&ring);
	if (ret) {
		fprintf(stderr, "test_nvme_io_fixed failed\n");
		return ret;
	}

	return 0;
}
