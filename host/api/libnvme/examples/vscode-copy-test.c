#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libnvme.h>
#include <time.h>

#define LBA_SIZE 4096

struct timeval start,end;

static bool g_use_local_buf = false;
struct nvme_kernel kernel;
struct nvme_io_args args;
uint32_t block_nums = 8;
uint32_t test_cycle_num = 1000;
uint32_t copy_count = 30;

static int vscode_copy(int fd, uint64_t src, uint64_t dst, uint64_t size)
{
    //struct nvme_kernel kernel;
    //struct nvme_io_args args;
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
        posix_memalign((void **)&data_buf, size, size);
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

    for (int i = 0; i < copy_count; i++) {
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
    }

    // Execute this kernel on VSCODE
    // gettimeofday(&start,NULL);
    return nvme_kernel_submit(&kernel);
}

int main(int argc, char **argv)
{

    //Initialize
    int fd;
    struct nvme_io_args args;

    // Read first command line argument into block_nums
    if (argc > 3) {
        block_nums = atoi(argv[1]);
        test_cycle_num = atoi(argv[2]);
        copy_count = atoi(argv[3]);
    }

    fd = nvme_open("nvmq0n1");
    printf("nvme_open: fd is %d\n", fd);
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd;
    args.nsid = 1;
    args.nlb = LBA_SIZE*block_nums/LBA_SIZE - 1;
    
    int ret;
    
    //Copy without kernel
    gettimeofday(&start,NULL);
    for(int i=0;i<test_cycle_num;i++){
        struct nvme_io_args read_args;
        struct nvme_io_args write_args;
        read_args = args;
        write_args = args;
        void* srcdatabuf;
        posix_memalign((void **)&srcdatabuf, LBA_SIZE * block_nums, LBA_SIZE * block_nums);

        if(srcdatabuf == NULL){
            fprintf(stderr,"MALLOC DATABUF FAILED!\n");
            continue;
            
        }
        memset(srcdatabuf,'A',LBA_SIZE*block_nums);
        //void* dstdatabuf = malloc(LBA_SIZE*block_nums);
        //memset(dstdatabuf,'B',LBA_SIZE*block_nums);
        read_args.data = srcdatabuf;
        read_args.data_len = LBA_SIZE*block_nums;
        read_args.slba = LBA_SIZE * block_nums * 1000 / LBA_SIZE;
        write_args.data = srcdatabuf;
        write_args.slba = LBA_SIZE * block_nums * 2000 / LBA_SIZE;
        write_args.data_len = LBA_SIZE*block_nums;
        for (int j = 0; j < copy_count; j++) {
            ret = nvme_read(&read_args);
            if(ret) {
                fprintf(stderr,"CYCLE NUM %d NVMe Read Failed: %d\n",i,ret);
                exit(0);
            }
            ret = nvme_write(&write_args);
            if(ret) {
                fprintf(stderr,"CYCLE NUM %d NVMe Write Failed: %d\n",i,ret);
                exit(0);
            }
        }
            
        free(srcdatabuf);
    }
    gettimeofday(&end,NULL);
    printf("COPY Without Kernel.time = %lf s\n", end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
    
    
    //Copy with kernel,using device memory
    gettimeofday(&start,NULL);
    //ret = vscode_copy(fd, 0, LBA_SIZE * block_nums, LBA_SIZE * block_nums);
    //if (ret) {
     //       fprintf(stderr, "CYCLE NUM 0 Kernel failed: %d\n", ret);
       //     exit(0);
    //}
    //g_use_local_buf = true;
    double time_interval = 0.0;
    for(int i=0;i<test_cycle_num;i++){
        ret = vscode_copy(fd, LBA_SIZE * block_nums * 1000 , LBA_SIZE * block_nums * 2000, LBA_SIZE * block_nums);
        //ret = nvme_kernel_submit(&kernel);
         if (ret) {
            fprintf(stderr, "CYCLE NUM %d Kernel failed: %d\n", i, ret);
            exit(0);
        }
        
    }
    gettimeofday(&end,NULL);
    // time_interval += end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000;
    
    // printf("COPY With Kerel And Device Memory.time =%lf s\n", time_interval);
    printf("COPY With Kernel And Device Memory.time = %lf s\n", end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
   
    
    //Copy with kernel,using host memory
    g_use_local_buf = true;
    gettimeofday(&start,NULL);
    for(int i=0;i<test_cycle_num;i++){
        ret = vscode_copy(fd, LBA_SIZE * block_nums * 1000, LBA_SIZE * block_nums * 2000, LBA_SIZE * block_nums);
        if (ret) {
            fprintf(stderr, "CYCLE NUM %d Kernel failed: %d\n", i, ret);
        }
    }
    gettimeofday(&end,NULL);
    printf("COPY With Kernel And Host Memory.time = %lf s\n", end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
}
