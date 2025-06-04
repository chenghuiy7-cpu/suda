// 包含必要的头文件 / Include necessary header files
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libnvme.h>
#include <thread>
#include <time.h>
#include <vector>
#include <atomic>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include "liburing.h"

// 定义逻辑块地址大小为4096字节 / Define logical block address size as 4096 bytes
#define LBA_SIZE 4096

// 调试日志宏定义 / Debug logging macro definition
//#define DEBUG
#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__) 
#else
#define DEBUG_LOG(...)
#endif

// 全局输入和输出缓冲区指针 / Global input and output buffer pointers
void* input_buf;
void* output_buf;



int main(int argc, char **argv){
// 设置线程数目，需要读取块的个数，从哪个位置开始读取 
// Setup thread count, number of blocks to read, and starting position

    // 定义时间变量用于性能测量 / Define time variables for performance measurement
    struct timeval start, end;
    
    // 设置缓冲区大小为一个逻辑块 / Set buffer size to one logical block
    int buf_size = LBA_SIZE;
    
    // 分配对齐的输出缓冲区内存 / Allocate aligned output buffer memory
    int ret = posix_memalign((void**)&output_buf, 4096, buf_size);   
    if(ret != 0 || output_buf == NULL){
        fprintf(stderr, "Failed to malloc memory.Exiting...\n");
        exit(1);
    }
    
    // 分配对齐的输入缓冲区内存 / Allocate aligned input buffer memory
    ret = posix_memalign((void**)&input_buf, 4096, buf_size);   
    if(ret != 0 || input_buf == NULL){
        fprintf(stderr, "Failed to malloc memory.Exiting...\n");
        exit(1);
    }
    
    // NVMe IO参数结构 / NVMe IO parameters structure
    struct nvme_io_args args;
    
    // 打开NVMe设备 / Open NVMe devices
    int io_fd = nvme_open("nvmq0n1");
    int admin_fd = nvme_open("nvmq0");
    int uring_fd = open("/dev/ng0n1", O_RDWR);
    if (uring_fd < 0)
    {
        fprintf(stderr, "Failed to Open NG0N1 Device\n");
        return 1;
    }
   
    DEBUG_LOG("nvme_open: io_fd is %d\n", io_fd);
    
    // 检查设备是否成功打开 / Check if devices were opened successfully
    if(io_fd < 1 || admin_fd < 1){
        fprintf(stderr, "Failed to open device,exiting...\n");
        free(output_buf);
        return 0;
    }
    
    // 定义输入和输出内存ID / Define input and output memory IDs
    unsigned int input_mem_id = 0, output_mem_id = 0;
   
    /**
     * 在设备上，创建两片内存空间，一个输入，一个输出，大小一样
     * Create two memory spaces on device, one for input, one for output, with same size
     */
    ret = nvme_create_slm_ns(admin_fd, &input_mem_id, buf_size);
    ret = nvme_create_slm_ns(admin_fd, &output_mem_id, buf_size);
    DEBUG_LOG("create_slm_ns: input_mem_id is %lx, output_mem_id is %lx\n", input_mem_id, output_mem_id);
    
    // 检查内存空间是否成功创建 / Check if memory spaces were created successfully
    if(input_mem_id < 0 || output_mem_id < 0){
        fprintf(stderr, "Failed to create slm,exiting...\n");
        free(output_buf);
        return 0;
    }
    
    // 初始化输入缓冲区数据 / Initialize input buffer data
    for(int i = 0; i < buf_size; i++){
        char x = (char)i;
        ((char*)input_buf)[i] = x;
    }
    
    // 打印输入数据的前几个字节 / Print first few bytes of input data
    printf("data %d %d %d\n", ((char*)input_buf)[0], ((char*)input_buf)[1], ((char*)input_buf)[2], ((char*)input_buf)[3]);
    
    // 将输入缓冲区数据写入设备内存 / Write input buffer data to device memory
    for(int i = 0; i < buf_size / (4096); i++){
        nvme_slm_write(io_fd, input_mem_id, i * (4096), 4096, input_buf + (4096) * i);
    }
    DEBUG_LOG("Finish slm write\n");
    
    // 开始测量直接从设备内存读取数据的性能 / Start measuring performance of direct reading from device memory
    gettimeofday(&start, nullptr);
    for(int i = 0; i < (buf_size / 4096); i++){
        // 把内存数据以4096B的粒度一次次拷贝到主机 / Copy memory data to host in 4096B chunks
        nvme_slm_read(io_fd, input_mem_id, 0, 4096, output_buf);
        DEBUG_LOG("%d i\n", i);
    }
    DEBUG_LOG("Finish slm read\n");
    
    // 结束测量并输出性能结果 / End measurement and output performance results
    gettimeofday(&end, nullptr);
    fprintf(stdout, "Finish only slm read,time used %lfs copy times%d\n", 
            end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec) / 1000000, 
            buf_size / 64);
    
    // 验证数据正确性 / Verify data correctness
    fprintf(stdout, "Data check \n");
    printf("data %d %d %d\n", ((char*)output_buf)[0], ((char*)output_buf)[1], ((char*)output_buf)[2], ((char*)output_buf)[3]);
    for(int i = 0; i < buf_size; i++){
        char x = (char)i;
        if(((char*)input_buf)[i] != (((char*)output_buf))[i]){
            fprintf(stderr, "data is wrong!\n");
            exit(1);
        }
    }
    DEBUG_LOG("Check Finish\n");
    return 0;
    
    // 以下代码不会执行（因为上面有return语句）/ The code below won't execute (due to the return statement above)
    
    // 清空输出缓冲区 / Clear output buffer
    memset(output_buf, 0, buf_size);
    
    // 开始测量设备内存间拷贝的性能 / Start measuring performance of memory-to-memory copy within device
    gettimeofday(&start, nullptr);
    
    // 分配对齐的源范围数据缓冲区 / Allocate aligned source range data buffer
    void* sr_data;
    ret = posix_memalign((void**)&sr_data, 4096, 4096);   
    
    for(int i = 0; i < (buf_size / (64 * 16)); i++){
        // 把内存数据以64B的粒度先以16次为一批拷贝到设备内存，然后再拷贝回来
        // Copy memory data in 64B chunks, 16 at a time, to device memory, then copy back
        union nvme_source_range *sr = (union nvme_source_range*)(sr_data);
        
        // 设置16个源范围结构 / Set up 16 source range structures
        for(int j = 0; j < 16; j++){
           sr[j].mc.nbyte = 64;                   // 每个拷贝64字节 / Copy 64 bytes each
           sr[j].mc.saddr = i * (64 * 16) + j * 64; // 源地址 / Source address
           sr[j].mc.snsid = input_mem_id;         // 源命名空间ID / Source namespace ID
           sr[j].mc.source_options = 0;           // 源选项 / Source options
        }
        
        // 执行SLM内存间拷贝 / Perform SLM memory-to-memory copy
        ret = nvme_slm_copy(io_fd, sr, sizeof(union nvme_source_range) * 16, 0, 0x4, 16, output_mem_id);
        if(ret < 0){
            fprintf(stderr, "Failed to copy slm data to slm,exiting...\n");
            free(output_buf);
            return 0;
        }
        DEBUG_LOG("Finish slm copy\n");
        
        // 读取拷贝后的数据到主机 / Read copied data to host
        nvme_slm_read(io_fd, output_mem_id, 0, 4096, output_buf);
        DEBUG_LOG("Finish slm read\n");
    }
    
    // 结束测量并输出性能结果 / End measurement and output performance results
    gettimeofday(&end, nullptr);
    fprintf(stdout, "Finish slm copy and slm read,time used %lf s copy times %d\n", 
            end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec) / 1000000, 
            buf_size / (64 * 16));
    return 0;
}