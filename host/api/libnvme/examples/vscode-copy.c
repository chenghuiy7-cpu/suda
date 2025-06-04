#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libnvme.h>

#define LBA_SIZE 4096

static bool g_use_local_buf = false;

static int vscode_copy(int fd, uint64_t src, uint64_t dst, uint64_t size)
{
    struct nvme_kernel kernel;
    struct nvme_io_args args;
    int buf_handle, ret;
    void *data_buf;
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd;
    args.nsid = 1;
    // Initialize this kernel
    ret = nvme_kernel_init(&kernel, fd);
    if (ret) {
        fprintf(stderr, "failed to init kernel: %d\n", ret);
        return ret;
    }

    // Allocate intermedidate data buffer
    if (g_use_local_buf) {
        posix_memalign((void **)&data_buf, 4096, size);
        if (data_buf == NULL) {
            perror("Error allocating memory");
            ret = -1;
        }
    } else {
        ret = nvme_kernel_mem_alloc(&kernel, size / LBA_SIZE, &buf_handle);
        data_buf = (void *)buf_handle;
    }

    if (ret) {
        fprintf(stderr, "failed to alloc mem: %d\n", ret);
        return ret;
    }

    // Read data into intermediate buffer
    args.slba = src / LBA_SIZE;
    args.data = data_buf;
    args.data_len = size / LBA_SIZE;
    ret = nvme_kernel_read(&kernel, &args);
    if (ret) {
        fprintf(stderr, "failed to read data: %d\n", ret);
        return ret;
    }

    // Write data from intermediate buffer
    args.slba = dst / LBA_SIZE;
    args.data = data_buf;
    ret = nvme_kernel_write(&kernel, &args);
    if (ret) {
        fprintf(stderr, "failed to write data: %d\n", ret);
        return ret;
    }

    // Execute this kernel on VSCODE
    return nvme_kernel_submit(&kernel);
}

int main()
{
    int fd = nvme_open("nvmq0n1");
    
    struct nvme_io_args args;
    printf("nvme_open: fd is %d\n", fd);
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd;
    args.nsid = 1;
    void* srcdatabuf = malloc(4096);
    memset(srcdatabuf,'A',4096);
    args.data = srcdatabuf;
    args.data_len = 4096;
    args.slba = 0 / LBA_SIZE;
    int ret;
    ret = nvme_write(&args);
    if(ret) {
    	fprintf(stderr,"NVMe Write Failed: %d\n",ret);
    }
    void* dstdatabuf = malloc(4096);
    memset(dstdatabuf,'B',4096);

    args.data = dstdatabuf;
    args.slba = 4096*8 / LBA_SIZE;
    ret = nvme_write(&args);
    if(ret) {
    	fprintf(stderr,"NVMe Write Failed: %d\n",ret);
    }
    args.slba = 0 / LBA_SIZE;
    ret = nvme_read(&args);
    if(ret) {
    	fprintf(stderr, "NVMe Read Failed: %d\n",ret);
    }
    printf("Before vscode_copy: DSTDATABUF FIRST CHAR:%c\n",((char*)dstdatabuf)[0]);	
    return 0;
    /*
    ret = vscode_copy(fd, 0, 4096 * 8, 4096);
    if (ret) {
        fprintf(stderr, "Kernel failed: %d\n", ret);
    }

    ret = nvme_read(&args);
    if(ret) {
    	fprintf(stderr, "NVMe Read Failed: %d\n",ret);
    }
    printf("After vscode_copy: DSTDATABUF FIRST CHAR:%c\n",((char*)dstdatabuf)[0]);	

    for(int i=0;i<4096;i++){
    	if((((char*)dstdatabuf)[i]) != 'A'){
		fprintf(stderr,"Kernel Execute Failed!\n");
		return 1;
	}
    }*/
}
