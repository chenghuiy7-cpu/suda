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

// 定义常量 / Define constants
#define LBA_SIZE 4096                // 逻辑块地址大小 / Logical Block Address size
#define MAX_THREADS 1                 // 最大线程数 / Maximum number of threads
#define MAX_BLOCK_NUM 262144         // 最大块数 / Maximum number of blocks
#define N 16                         // 定义N为16 / Define N as 16
//#define DEBUG
#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__) 
#else
#define DEBUG_LOG(...)
#endif

// 全局变量声明 / Global variable declarations
uint32_t  read_block_num;            // 读取的块数量 / Number of blocks to read
uint64_t input_buf_size;             // 输入缓冲区大小 / Input buffer size
uint32_t  read_from;                 // 从哪里开始读取 / Starting position to read from
void* output_buf;                    // 输出缓冲区 / Output buffer

/*
int nvme_slm_read(
	int fd,
	int nsid,
	int starting_bytes,
	int read_length,
	void* data

){
	//opcode 02h
	return nvme_io_passthru(
		fd,0x02,0,0,nsid,0,0,starting_bytes,starting_bytes>>32,
		read_length,0,0,0,read_length,data,0,NULL,0,NULL
	);
}
*/

// 标记是否已注册缓冲区 / Flag indicating if buffers are registered
bool registed = false;
// IO向量数组 / IO vector array
struct iovec vecs[5];

// 显示NVMe命令的详细信息 / Display detailed information of NVMe command
void nvme_show_command(struct nvme_passthru_cmd *cmd)
{
    printf("opcode       : %02x\n", cmd->opcode);
    printf("flags        : %02x\n", cmd->flags);
    printf("rsvd1        : %04x\n", cmd->rsvd1);
    printf("nsid         : %08x\n", cmd->nsid);
    printf("cdw2         : %08x\n", cmd->cdw2);
    printf("cdw3         : %08x\n", cmd->cdw3);
    printf("data_len     : %08x\n", cmd->data_len);
    printf("metadata_len : %08x\n", cmd->metadata_len);
    printf("addr         : %"PRIx64"\n", (uint64_t)(uintptr_t)cmd->addr);
    printf("metadata     : %"PRIx64"\n", (uint64_t)(uintptr_t)cmd->metadata);
    printf("cdw10        : %08x\n", cmd->cdw10);
    printf("cdw11        : %08x\n", cmd->cdw11);
    printf("cdw12        : %08x\n", cmd->cdw12);
    printf("cdw13        : %08x\n", cmd->cdw13);
    printf("cdw14        : %08x\n", cmd->cdw14);
    printf("cdw15        : %08x\n", cmd->cdw15);
    printf("timeout_ms   : %08x\n", cmd->timeout_ms);
    printf("result       : %08x\n", cmd->result);
}

// 从SLM固定大小读取数据 / Read fixed size data from SLM
int slm_read_fixed(void *ptr, void* start_address, unsigned int cut_size, int fd, int nsid, unsigned long long starting_bytes)
{
    struct io_uring *ring = (struct io_uring*)ptr;
    cut_size = cut_size/2;
   
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    int i, ret;
 
    // 准备两个命令 / Prepare two commands
    struct nvme_uring_cmd *cmd0 = (struct nvme_uring_cmd *)vecs[0].iov_base;
    struct nvme_uring_cmd *cmd1 = (struct nvme_uring_cmd *)vecs[1].iov_base;
   
    // 设置第一个读取命令 / Configure first read command
    cmd0->opcode = 0x2;              // 读取操作码 / Read operation code
    cmd0->data_len = cut_size;       // 数据长度 / Data length
    cmd0->addr = (unsigned long long)start_address; // 起始地址 / Start address
    cmd0->nsid = nsid;               // 命名空间ID / Namespace ID
    cmd0->cdw10 = starting_bytes;    // 低32位起始字节 / Low 32 bits of starting bytes
    cmd0->cdw11 = starting_bytes >> 32; // 高32位起始字节 / High 32 bits of starting bytes
    cmd0->flags = 0;                 // 标志位 / Flags
    cmd0->cdw12 = cut_size;          // 读取长度 / Read length
    cmd0->cdw13 = 0;
    cmd0->cdw14 = 0;
    cmd0->cdw15 = 0;
    cmd0->flags = 0;

    // 设置第二个读取命令 / Configure second read command
    cmd1->opcode = 0x2;              // 读取操作码 / Read operation code
    cmd1->data_len = cut_size;       // 数据长度 / Data length
    cmd1->addr = (unsigned long long)start_address+cut_size; // 起始地址+cut_size / Start address+cut_size
    cmd1->nsid = nsid;               // 命名空间ID / Namespace ID
    starting_bytes += cut_size;      // 更新起始字节 / Update starting bytes
    cmd1->cdw2 = 0;
    cmd1->cdw3 = 0;
    cmd1->cdw10 = starting_bytes;    // 低32位起始字节 / Low 32 bits of starting bytes
    cmd1->cdw11 = starting_bytes >> 32; // 高32位起始字节 / High 32 bits of starting bytes
    cmd1->flags = 0;                 // 标志位 / Flags
    cmd1->cdw12 = cut_size;          // 读取长度 / Read length
    cmd1->cdw13 = 0;
    cmd1->cdw14 = 0;
    cmd1->cdw15 = 0;
    cmd1->flags = 0;

    //nvme_show_command((struct nvme_passthru_cmd*)cmd1);
    //nvme_show_command((struct nvme_passthru_cmd*)cmd0);
   
    // 获取提交队列入口并准备写入命令 / Get submission queue entry and prepare write command
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_writev(sqe, fd, &(vecs[0]), 1, 0);
    
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    
    io_uring_prep_writev(sqe, fd, &(vecs[1]), 1, 0);
    //io_uring_prep_read_fixed(sqe, fd, vecs[0].iov_base,
                              //vecs[0].iov_len, 0, 0);
    
    //io_uring_prep_read_fixed(sqe, fd, vecs[1].iov_base,
                            //vecs[1].iov_len, 0, 1);

    // 提交IO请求 / Submit IO requests
    ret = io_uring_submit(ring);

    if (ret < 0)
    {
        fprintf(stderr, "sqe submit failed: %d\n", ret);
        goto err;
    }
    else if (ret != 2)
    {
        fprintf(stderr, "Submitted Failed ret%d\n", ret);
        goto err;
    }

    // 等待完成队列事件 / Wait for completion queue events
    for (i = 0; i < 2; i++)
    {
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0)
        {
            fprintf(stderr, "wait completion %d\n", ret);
            goto err;
        }
        if (cqe->res < 0)
        {
            fprintf(stderr, "I/O write error on %lu: %s\n",
                    (unsigned long)cqe->user_data,
                    strerror(-cqe->res));
            goto err;
        }
        io_uring_cqe_seen(ring, cqe);
    }
    //io_uring_unregister_buffers(ring);
    //free(vecs[0].iov_base);
    return 0;
err:
    return 1;
}

// 异步执行程序 / Execute program asynchronously
int program_exec_async(int fd, unsigned long long read_from_lba, unsigned long long write_to_saddr, int rsid, int pind, unsigned int mem_nsid, void* ptr)
{
    struct io_uring *ring = (struct io_uring*)ptr;
   
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    int i, ret;
 
    // 准备两个命令 / Prepare two commands
    struct nvme_uring_cmd *cmd0 = (struct nvme_uring_cmd *)vecs[2].iov_base;
    struct nvme_uring_cmd *cmd1 = (struct nvme_uring_cmd *)vecs[3].iov_base;
    unsigned long long length = sizeof(union nvme_source_range)*8;
    
    // 设置第一个命令 - 写入 / Configure first command - write
    cmd0->opcode = 0x1;              // 写入操作码 / Write operation code
    cmd0->addr = (unsigned long long)(vecs[4].iov_base); // 数据地址 / Data address
    cmd0->data_len = length;         // 数据长度 / Data length
    cmd0->nsid = mem_nsid;           // 内存命名空间ID / Memory namespace ID
    cmd0->cdw2 = length;             // 长度低32位 / Low 32 bits of length
    cmd0->cdw3 = length >> 32;       // 长度高32位 / High 32 bits of length
    cmd0->cdw10 = write_to_saddr;    // 写入地址低32位 / Low 32 bits of write address
    cmd0->cdw11 = write_to_saddr >> 32; // 写入地址高32位 / High 32 bits of write address
    cmd0->flags = 0;                 // 标志位 / Flags
    cmd0->cdw12 = (0x3<<8)|7;        // 控制字 / Control word
    cmd0->cdw13 = 0;
    cmd0->cdw14 = 0;
    cmd0->cdw15 = 0;
    cmd0->flags = 0;
    
    // 设置第二个命令 - 程序执行 / Configure second command - program execution
    cmd1->opcode = 0x1;              // 写入操作码 / Write operation code
    cmd1->data_len = 0;              // 无数据 / No data
    cmd1->addr = NULL;               // 无地址 / No address
    cmd1->nsid = 2;                  // 命名空间ID / Namespace ID
    cmd1->cdw2 = rsid << 16 | pind;  // 内存范围集ID和程序索引 / Memory range set ID and program index
    cmd1->cdw3 = 0;
    cmd1->cdw10 = 0;
    cmd1->cdw11 = 0;
    cmd1->flags = 0;                 // 标志位 / Flags
    cmd1->cdw12 = 0;
    cmd1->cdw13 = 0;
    cmd1->cdw14 = 0;
    cmd1->cdw15 = 0;
    cmd1->flags = 0;

    //nvme_show_command((struct nvme_passthru_cmd*)cmd1);
    //nvme_show_command((struct nvme_passthru_cmd*)cmd0);
   
    // 获取提交队列入口并准备写入命令 / Get submission queue entry and prepare write command
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_writev(sqe, fd, &(vecs[2]), 1, 0);
    
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    
    io_uring_prep_writev(sqe, fd, &(vecs[3]), 1, 0);
    //io_uring_prep_read_fixed(sqe, fd, vecs[0].iov_base,
                              //vecs[0].iov_len, 0, 0);
    
    //io_uring_prep_read_fixed(sqe, fd, vecs[1].iov_base,
                            //vecs[1].iov_len, 0, 1);

    // 提交IO请求 / Submit IO requests
    ret = io_uring_submit(ring);

    if (ret < 0)
    {
        fprintf(stderr, "sqe submit failed: %d\n", ret);
        goto err;
    }
    else if (ret != 2)
    {
        fprintf(stderr, "Submitted Failed ret%d\n", ret);
        goto err;
    }

    // 等待完成队列事件 / Wait for completion queue events
    for (i = 0; i < 2; i++)
    {
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0)
        {
            fprintf(stderr, "wait completion %d\n", ret);
            goto err;
        }
        if (cqe->res < 0)
        {
            fprintf(stderr, "I/O write error on %lu: %s\n",
                    (unsigned long)cqe->user_data,
                    strerror(-cqe->res));
            goto err;
        }
        io_uring_cqe_seen(ring, cqe);
    }
    
    // 打印命令执行结果 / Print command execution results
    printf("CMD 1 result%d\n", cmd0->rsvd2);
    printf("CMD 2 result%d\n", cmd1->rsvd2);
    //io_uring_unregister_buffers(ring);
    //free(vecs[0].iov_base);
    return 0;
err:
    return 1;
}


int main(int argc, char **argv){
// 设置线程数目，需要读取块的个数，从哪个位置开始读取 / Set thread count, number of blocks to read, and starting position
    read_block_num = 256*256;
    read_from = 0;
   
    DEBUG_LOG("get size of struct memory_range_descriptor%d\n", sizeof(struct memory_range_descriptor));
    struct timeval start, end;
    
    // 设置输入缓冲区大小并分配输出缓冲区 / Set input buffer size and allocate output buffer
    input_buf_size = LBA_SIZE*256;
    int ret = posix_memalign((void**)&output_buf, 4096, LBA_SIZE*read_block_num);   
    if(ret != 0 || output_buf == NULL){
        fprintf(stderr, "Failed to malloc memory.Exiting...\n");
        exit(1);
    }
    
    // 打开NVMe设备 / Open NVMe devices
    struct nvme_io_args args;
    int io_fd = nvme_open("nvmq0n1");
    int admin_fd = nvme_open("nvmq0");
    int uring_fd = open("/dev/ng0n1", O_RDWR);
    if (uring_fd < 0)
    {
        fprintf(stderr, "Failed to Open NG0N1 Device\n");
        return 1;
    }
    //unsigned long* log_page_data = (unsigned long*)malloc(4096);
    //memset(log_page_data,0,4096);
    //ret = nvme_get_log_simple(fd,0x82,4096,(void*)log_page_data);
    /*
    ret = nvme_get_nsid_log(admin_fd, false, NVME_LOG_LID_COMPUTE, 2, 4096, (void*)log_page_data);
    if(ret!=0){
        fprintf(stderr,"Failed to Get Log Pages!Exiting...\n");
        return -1;
    }
    union nvme_prog_desc_list* desclist = (union nvme_prog_desc_list*)(log_page_data);
    
    printf("LIST NUMBER:%d\n",desclist[0].header.numd);
    for(int i=0;i<16;i++){
        if(desclist[i+1].data.peocc!=0){
            printf("GET PROGRAM PIND:%d ACTIVATED:%s TYPE:%s\n",i,desclist[i+1].data.activation?"TRUE":"FALSE"
            ,desclist[i+1].data.program_type==fusion_program?"FUSION":"OPLIB");
        }
    }
    for(int i=0;i<32;i++){
        if(desclist[i+17].data.pit!=0){
            printf("GET PROGRAM PIND:%d ACTIVATED:%s TYPE:%s",i,desclist[i+17].data.activation?"TRUE":"FALSE"
            ,desclist[i+17].data.program_type==fusion_program?"FUSION":"OPLIB");
            char op_name[9];
            for(int j=0;j<8;j++){
                op_name[j] = desclist[i+17].data.pid;
                desclist[i+17].data.pid = desclist[i+17].data.pid >> 8;
            }
            op_name[8] = '\0';
            printf("OP NAME:%s\n",op_name);
        }
    }
    */
    DEBUG_LOG("nvme_open: io_fd is %d\n", io_fd);
    if(io_fd < 1 || admin_fd < 1){
        fprintf(stderr, "Failed to open device,exiting...\n");
        free(output_buf);
        return 0;
    }
    
    // 内存ID / Memory IDs
    unsigned int input_mem_id=0, output_mem_id=0;
   
    /**
     * 在设备上，创建两片内存空间，一个输入，一个输出，大小一样
     * Create two memory spaces on device, one for input, one for output, with same size
     */
    ret = nvme_create_slm_ns(admin_fd, &input_mem_id, input_buf_size);
    ret = nvme_create_slm_ns(admin_fd, &output_mem_id, input_buf_size+4096);
    DEBUG_LOG("create_slm_ns: input_mem_id is %lx, output_mem_id is %lx\n", input_mem_id, output_mem_id);
    if(input_mem_id < 0 || output_mem_id < 0){
        fprintf(stderr, "Failed to create slm,exiting...\n");
        free(output_buf);
        return 0;
    }
    
    /**
     * 创建一个计算程序
     * Create a computation program
     */
    struct hlsacccompute_program p, *program;
    program = &p;
    program->input_channum = 1;                   // 输入通道数 / Input channel count
    program->output_channum = 1;                  // 输出通道数 / Output channel count
    program->program_id = 1;                      // 程序ID / Program ID
    program->apply_operators_id_map[0] = 1;       // 申请一个1号算子，对应功能是加密 / Request operator 1 for encryption
    program->applyops[0].header.cid = 0;          // 通道ID / Channel ID
    program->applyops[0].header.opc = APPLY_OPS;  // 操作码 / Operation code
    program->applyops[0].header.ops_num = 1;      // 操作数量 / Operation count
    program->applyops[2].apply_ops_payload2.connections_num = 1; // 连接数量 / Connection count
    program->applyops[2].apply_ops_payload2.connections[0].from = 0 << 4 | 0; // 来源 / Source
    // ffff表示是通道，请求分配一条出口通道 / ffff indicates channel, requesting an output channel
    program->applyops[2].apply_ops_payload2.connections[0].to = 0xf0; // 目标 / Destination
    program->pauseops[0].header.cid = 0;                // 通道ID / Channel ID
    program->pauseops[0].header.opc = SUSPEND_OPS;      // 暂停操作 / Suspend operation
    program->pauseops[0].header.ops_num = 1;            // 操作数量 / Operation count
    program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    program->freeops[0].header.cid = 0;                 // 通道ID / Channel ID
    program->freeops[0].header.opc = FORCE_FREE_OPS;    // 强制释放操作 / Force free operation
    program->freeops[0].header.ops_num = 1;             // 操作数量 / Operation count
    program->freeops[1].generic_ops_payload.op_lists[0] = 0;
    program->input_channel_destination[0] = 0;
    program->apply_ops_size = 3;                        // 应用操作大小 / Apply operation size
    program->apply_operators_num = 1;                   // 应用操作数量 / Apply operation count
    program->esti_executed_time = 35;                   // 估计执行时间 / Estimated execution time
    program->max_responded_time = program->esti_executed_time * 3; // 最大响应时间 / Maximum response time

    int psize = sizeof(struct hlsacccompute_program);
    // 加载程序 / Load program
    ret = nvme_load_hlsacc_program(admin_fd, psize, 1, 2, program);
    if(ret != 0){
        fprintf(stderr, "Failed to load program: %d, exiting...\n", ret);
        free(output_buf);
        return 0;
    }
    
    // 激活程序 / Activate program
    ret = nvme_activate_program(admin_fd, 1, 2);
    if(ret != 0){
        fprintf(stderr, "Failed to activate program: %d, exiting...\n", ret);
        free(output_buf);
        return 0;
    }
    
    /**
     * 创建一个内存集，0元素表示输入，1元素表示输出
     * Create a memory range set, element 0 for input, element 1 for output
     */
    union memory_range_set_decriptor mdes[2];
    mdes[0].payload.mnsid = input_mem_id;         // 输入内存ID / Input memory ID
    mdes[0].payload.length = input_buf_size;      // 长度 / Length
    mdes[0].payload.starting_byte = 0;            // 起始字节 / Starting byte
    mdes[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM; // 设备内存标志 / Device memory flag
    mdes[1].payload.mnsid = output_mem_id;        // 输出内存ID / Output memory ID
    mdes[1].payload.length = input_buf_size+4096; // 长度 / Length
    mdes[1].payload.starting_byte = 0;            // 起始字节 / Starting byte
    mdes[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM; // 设备内存标志 / Device memory flag
    unsigned int rsid = 0;
    
    // 创建内存范围集 / Create memory range set
    ret = nvme_create_memory_range_set(admin_fd, 2, &rsid, 2, mdes);
    if(ret != 0){
        fprintf(stderr, "Failed to create memory range set.Exiting...\n");
        free(output_buf);
        return 0;
    }
    
    // 初始化IO环 / Initialize IO ring
    struct io_uring ring;
    ret = io_uring_queue_init(8, &ring, 0);
    if (ret) {
        fprintf(stderr, "ring setup failed\n");
        return 1;
    }
    
    // 如果未注册，分配内存并注册缓冲区 / If not registered, allocate memory and register buffers
    if(registed == false){
        posix_memalign(&vecs[0].iov_base, 4096, 4096);
        posix_memalign(&vecs[1].iov_base, 4096, 4096);
        posix_memalign(&vecs[2].iov_base, 4096, 4096);
        posix_memalign(&vecs[3].iov_base, 4096, 4096);
        posix_memalign(&vecs[4].iov_base, 4096, 4096);
        vecs[0].iov_len = 512;//sizeof(struct nvme_uring_cmd);
        vecs[1].iov_len = 512;//sizeof(struct nvme_uring_cmd);
        vecs[2].iov_len = 512;//sizeof(struct nvme_uring_cmd);
        vecs[3].iov_len = 512;//sizeof(struct nvme_uring_cmd);
        //ret = io_uring_register_buffers(&ring, vecs, 2);
        if (ret)
        {
            fprintf(stderr, "Failed to register buffers: %d\n", ret);
            return 1;
        }
        registed = true;
    }

    DEBUG_LOG("create_memory_range_set:Get rsid%d\n", rsid);
    gettimeofday(&start, nullptr);
   
    for(int i = 0; i < 1; i++){
    /**
     * 从盘里把数据读取到input_mem
     * Read data from disk to input_mem
     *//*
        for(int j=0;j<8;j++){
            union nvme_source_range sr;
            sr.scc.slba = read_from+i*(input_buf_size/LBA_SIZE)+(j*32);
            sr.scc.nlb = 31;
            sr.scc.snsid = 1;
            ret = nvme_slm_copy(io_fd,&sr,sizeof(union nvme_source_range),j*LBA_SIZE*32,0x3,1,input_mem_id);
            if(ret<0){
                fprintf(stderr,"Failed to copy block data to slm,exiting...\n");
                free(output_buf);
                return 0;
            }
        }*/
        
        // 设置源范围 / Set source range
        union nvme_source_range *sr = (union nvme_source_range*)(vecs[4].iov_base);
        for(int j = 0; j < 8; j++){
            sr[j].scc.slba = read_from+i*(input_buf_size/LBA_SIZE)+(j*32);
            sr[j].scc.nlb = 31;
            sr[j].scc.snsid = 1;
        }

        ret = nvme_slm_copy(io_fd,sr,sizeof(union nvme_source_range)*8,0,0x3,8,input_mem_id);
        if(ret<0){
            fprintf(stderr,"Failed to copy block data to slm,exiting...\n");
            free(output_buf);
            return 0;
       }

        // 异步执行程序 / Execute program asynchronously
        //program_exec_async(uring_fd, read_from+i*(input_buf_size/LBA_SIZE), 0, rsid, 1, input_mem_id, &ring);
        /**
         * 尝试运行计算程序
         * Try to run computation program
         */
        
        
        struct AccContext* context_data = nullptr;
        unsigned int res = 0;
        ret = nvme_execute_hlsacc_program(io_fd,2,rsid,1,context_data,0,0,0,&res);
        if(ret != 0){
            fprintf(stderr,"Failed to execute program.Exiting...\n");
            free(output_buf);
            return 0;
        }
        DEBUG_LOG("iter %d result%d\n", i, res);
        /**
         * 把访问的结果从设备内存中拷贝到主机内存
         * Copy the result from device memory to host memory
         */
        
        for(int j = 0; j < 8; j++){
            // 从SLM读取数据到主机内存 / Read data from SLM to host memory
            ret = nvme_slm_read(io_fd, output_mem_id, j*LBA_SIZE*32, LBA_SIZE*32, output_buf+i*(input_buf_size)+j*(LBA_SIZE*32));
            if(ret != 0){
                fprintf(stderr, "Failed to copy data to host.Exiting...\n");
                free(output_buf);
                return 0;
            }
        }
      
       
    }
    
    // 计算并输出执行时间 / Calculate and output execution time
    gettimeofday(&end, nullptr);
    fprintf(stdout, "Finish blowfish encryption,time used %lf s\n", end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
    return 0;
}
