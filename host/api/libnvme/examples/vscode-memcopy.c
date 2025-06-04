#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libnvme.h>
#include <time.h>

#define LBA_SIZE 4096

static bool g_use_compute = true;
static int g_num_blocks = 1;

#define DATA_SIZE (4096 * g_num_blocks)

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
    ret = nvme_kernel_mem_alloc(&kernel, size / LBA_SIZE, &buf_handle);
    data_buf = (void *)buf_handle;

    // if (ret) {
    //     fprintf(stderr, "failed to alloc mem: %d\n", ret);
    //     return ret;
    // }

    // Write data into disk buffer
    // args.slba = 0 / LBA_SIZE;
    // args.data = (void *)src;
    // args.data_len = size / LBA_SIZE;
    // args.nlb = size / LBA_SIZE;
    // ret = nvme_kernel_write(&kernel, &args);
    // if (ret) {
    //     fprintf(stderr, "failed to read data: %d\n", ret);
    //     return ret;
    // }

    // Read data into intermediate buffer
    args.slba = 0 / LBA_SIZE;
    args.data = g_use_compute ? data_buf : dst;
    args.data_len = size / LBA_SIZE;
    args.nlb = size / LBA_SIZE;
    ret = nvme_kernel_read(&kernel, &args);
    if (ret) {
        fprintf(stderr, "failed to read data: %d\n", ret);
        return ret;
    }

    // Copy data from intermediate buffer
    if (g_use_compute) {
        args.slba = dst;
        args.data = data_buf;
        ret = nvme_kernel_mem_copy(&kernel, &args);
        if (ret) {
            fprintf(stderr, "failed to write data: %d\n", ret);
            return ret;
        }
    }

    // Execute this kernel on VSCODE
    return nvme_kernel_submit(&kernel);
}

void print_bytes(void *data, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        printf("%02x ", ((uint8_t *)data)[i]);
        if (size > 16 && (i+1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    int fd = nvme_open("nvmq0n1");
    int ret;

    if (argc > 1) {
        g_use_compute = atoi(argv[1]);
        if (argc > 2) {
            g_num_blocks = atoi(argv[2]);
        }
    }
    
    struct nvme_io_args args;
    // printf("nvme_open: fd is %d\n", fd);
    memset(&args, 0, sizeof(args));

    void* srcdatabuf;
    posix_memalign((void **)&srcdatabuf, 4096, DATA_SIZE);
    memset(srcdatabuf,'A',DATA_SIZE);
    
    void* dstdatabuf;
    posix_memalign((void **)&dstdatabuf, 4096, DATA_SIZE);
    memset(dstdatabuf,'B',DATA_SIZE);

    // printf("Before vscode_copy: DSTDATABUF FIRST CHAR:%c\n",((char*)dstdatabuf)[0]);	

    struct timeval start,end;
    gettimeofday(&start,NULL);

    for (int i=0;i<10000;i++) {
        ret = vscode_copy(fd, (uintptr_t)srcdatabuf, (uintptr_t)dstdatabuf, DATA_SIZE);
        if (ret) {
            fprintf(stderr, "Kernel failed: %d\n", ret);
        }
    }

    gettimeofday(&end,NULL);
    printf("Read %d KB with %sreorder time = %lf s\n", 4 * g_num_blocks, g_use_compute ? "" : "out ", end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);

    // printf("After vscode_copy: DSTDATABUF FIRST CHAR:%c\n",((char*)dstdatabuf)[0]);	

    print_bytes(dstdatabuf, DATA_SIZE);

    // for(int i=0;i<DATA_SIZE;i++){
    // 	if((((char*)dstdatabuf)[i]) != 'A' + 7){
	// 	fprintf(stderr,"Kernel Execute Failed!\n");
	// 	return 1;
	// }
    // }
}
