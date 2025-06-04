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

#define LBA_SIZE 4096

//#define DEBUG
#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__) 
#else
#define DEBUG_LOG(...)
#endif

void* input_buf;
void* output_buf;



int main(int argc, char **argv){
//设置线程数目，需要读取块的个数，从哪个位置开始读取
   
    struct timeval start,end;
    
    int buf_size = LBA_SIZE;
    int ret = posix_memalign((void**)&output_buf,4096,buf_size);   
    if(ret != 0||output_buf==NULL){
        fprintf(stderr,"Failed to malloc memory.Exiting...\n");
        exit(1);
    }
    ret = posix_memalign((void**)&input_buf,4096,buf_size);   
    if(ret != 0||input_buf==NULL){
        fprintf(stderr,"Failed to malloc memory.Exiting...\n");
        exit(1);
    }
    struct nvme_io_args args;
    int io_fd = nvme_open("nvmq0n1");
    int admin_fd = nvme_open("nvmq0");
    int uring_fd = open("/dev/ng0n1", O_RDWR);
    if (uring_fd < 0)
    {
        fprintf(stderr, "Failed to Open NG0N1 Device\n");
        return 1;
    }
   
    DEBUG_LOG("nvme_open: io_fd is %d\n", io_fd);
    if(io_fd < 1||admin_fd < 1){
        fprintf(stderr,"Failed to open device,exiting...\n");
        free(output_buf);
        return 0;
    }
    unsigned int input_mem_id=0,output_mem_id=0;
   
    /**
     * 在设备上，创建两片内存空间，一个输入，一个输出，大小一样
     */
    ret = nvme_create_slm_ns(admin_fd,&input_mem_id,buf_size);
    ret = nvme_create_slm_ns(admin_fd,&output_mem_id,buf_size);
    DEBUG_LOG("create_slm_ns: input_mem_id is %lx, output_mem_id is %lx\n", input_mem_id,output_mem_id);
    if(input_mem_id<0||output_mem_id<0){
        fprintf(stderr,"Failed to create slm,exiting...\n");
        free(output_buf);
        return 0;
    }
    for(int i=0;i<buf_size;i++){
        char x = (char)i;
        ((char*)input_buf)[i] = x;
    }
    printf("data %d %d %d\n", ((char*)input_buf)[0], ((char*)input_buf)[1], ((char*)input_buf)[2], ((char*)input_buf)[3]   );
    for(int i=0;i<buf_size/(4096);i++){
        nvme_slm_write(io_fd,input_mem_id,i*(4096),4096,input_buf+(4096)*i);
    }
    DEBUG_LOG("Finish slm write\n");
    gettimeofday(&start,nullptr);
    for(int i=0;i<(buf_size/4096);i++){
        //把内存数据以64B的粒度一次次拷贝到主机
        nvme_slm_read(io_fd,input_mem_id,0,4096,output_buf);
        DEBUG_LOG("%d i\n",i);
    }
    DEBUG_LOG("Finish slm read\n");
    gettimeofday(&end,nullptr);
    fprintf(stdout,"Finish only slm read,time used %lfs copy times%d\n",end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000,buf_size/64);
    
    fprintf(stdout,"Data check \n");
    printf("data %d %d %d\n", ((char*)output_buf)[0], ((char*)output_buf)[1], ((char*)output_buf)[2], ((char*)output_buf)[3]   );
    for(int i=0;i<buf_size;i++){
        char x = (char)i;
        if(((char*)input_buf)[i] != (((char*)output_buf))[i]){
            fprintf(stderr,"data is wrong!\n");
            exit(1);
        }
    }
    DEBUG_LOG("Check Finish\n");
    return 0;
    memset(output_buf,0,buf_size);
    gettimeofday(&start,nullptr);
    void* sr_data;
    ret = posix_memalign((void**)&sr_data,4096,4096);   
    for(int i=0;i<(buf_size/(64*16));i++){
        //把内存数据以64B的粒度先以16次为一批拷贝到设备内存，然后再拷贝回来
        union nvme_source_range *sr = (union nvme_source_range*)(sr_data);
        for(int j=0;j<16;j++){
           sr[j].mc.nbyte = 64;
           sr[j].mc.saddr = i*(64*16) + j*64;
           sr[j].mc.snsid = input_mem_id;
           sr[j].mc.source_options = 0;
        }
        ret = nvme_slm_copy(io_fd,sr,sizeof(union nvme_source_range)*16,0,0x4,16,output_mem_id);
        if(ret<0){
            fprintf(stderr,"Failed to copy slm data to slm,exiting...\n");
            free(output_buf);
            return 0;
        }
        DEBUG_LOG("Finish slm copy\n");
        nvme_slm_read(io_fd,output_mem_id,0,4096,output_buf);
        DEBUG_LOG("Finish slm read\n");
    }
    gettimeofday(&end,nullptr);
    fprintf(stdout,"Finish slm copy and slm read,time used %lf s copy times %d\n",end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000,buf_size/(64*16));
    return 0;
}