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
#define MAX_THREADS 1
#define MAX_BLOCK_NUM 262144
#define N 16
//#define DEBUG
#ifdef DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__) 
#else
#define DEBUG_LOG(...)
#endif
uint32_t  read_block_num;
uint64_t input_buf_size;
uint32_t  read_from;
void* output_buf;
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
bool registed = false;
struct iovec vecs[5];
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
int slm_read_fixed(void *ptr,void* start_address,unsigned int cut_size,int fd,int nsid,unsigned long long starting_bytes)
{
    struct io_uring *ring = (struct io_uring*)ptr;
    cut_size = cut_size/2;
   
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    int i, ret;
 
    struct nvme_uring_cmd *cmd0 = (struct nvme_uring_cmd *)vecs[0].iov_base;
    struct nvme_uring_cmd *cmd1 = (struct nvme_uring_cmd *)vecs[1].iov_base;
   
    cmd0->opcode = 0x2;
    cmd0->data_len = cut_size;
    cmd0->addr = (unsigned long long)start_address;
    cmd0->nsid = nsid;
    cmd0->cdw10 = starting_bytes;
    cmd0->cdw11 = starting_bytes >> 32;
    cmd0->flags = 0;
    cmd0->cdw12 = cut_size;
    cmd0->cdw13 = 0;
    cmd0->cdw14 = 0;
    cmd0->cdw15 = 0;
    cmd0->flags = 0;
    cmd1->opcode = 0x2;
    cmd1->data_len = cut_size;
    cmd1->addr = (unsigned long long)start_address+cut_size;
    cmd1->nsid = nsid;
    starting_bytes += cut_size;
    cmd1->cdw2 = 0;
    cmd1->cdw3 = 0;
    cmd1->cdw10 = starting_bytes;
    cmd1->cdw11 = starting_bytes >> 32;
    cmd1->flags = 0;
    cmd1->cdw12 = cut_size;
    cmd1->cdw13 = 0;
    cmd1->cdw14 = 0;
    cmd1->cdw15 = 0;
    cmd1->flags = 0;

    //nvme_show_command((struct nvme_passthru_cmd*)cmd1);
    //nvme_show_command((struct nvme_passthru_cmd*)cmd0);
   
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_writev(sqe,fd,&(vecs[0]),1,0);
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    
    io_uring_prep_writev(sqe,fd,&(vecs[1]),1,0);
    //io_uring_prep_read_fixed(sqe, fd, vecs[0].iov_base,
                              //vecs[0].iov_len, 0, 0);
    
    //io_uring_prep_read_fixed(sqe, fd, vecs[1].iov_base,
                            //vecs[1].iov_len, 0, 1);

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

int program_exec_async(int fd,unsigned long long read_from_lba,unsigned long long write_to_saddr,int rsid,int pind,unsigned int mem_nsid,void* ptr)
{
    struct io_uring *ring = (struct io_uring*)ptr;
   
    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    int i, ret;
 
    struct nvme_uring_cmd *cmd0 = (struct nvme_uring_cmd *)vecs[2].iov_base;
    struct nvme_uring_cmd *cmd1 = (struct nvme_uring_cmd *)vecs[3].iov_base;
    unsigned long long length = sizeof(union nvme_source_range)*8;
    cmd0->opcode = 0x1;
    cmd0->addr = (unsigned long long)(vecs[4].iov_base);
    cmd0->data_len = length;
    cmd0->nsid = mem_nsid;
    cmd0->cdw2 = length;
    cmd0->cdw3 = length >> 32;
    cmd0->cdw10 = write_to_saddr;
    cmd0->cdw11 = write_to_saddr >> 32;
    cmd0->flags = 0;
    cmd0->cdw12 = (0x3<<8)|7;
    cmd0->cdw13 = 0;
    cmd0->cdw14 = 0;
    cmd0->cdw15 = 0;
    cmd0->flags = 0;
    cmd1->opcode = 0x1;
    cmd1->data_len = 0;
    cmd1->addr = NULL;
    cmd1->nsid = 2;
    cmd1->cdw2 = rsid << 16 | pind;
    cmd1->cdw3 = 0;
    cmd1->cdw10 = 0;
    cmd1->cdw11 = 0;
    cmd1->flags = 0;
    cmd1->cdw12 = 0;
    cmd1->cdw13 = 0;
    cmd1->cdw14 = 0;
    cmd1->cdw15 = 0;
    cmd1->flags = 0;

    //nvme_show_command((struct nvme_passthru_cmd*)cmd1);
    //nvme_show_command((struct nvme_passthru_cmd*)cmd0);
   
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    io_uring_prep_writev(sqe,fd,&(vecs[2]),1,0);
    sqe = io_uring_get_sqe(ring);
    if (!sqe)
    {
        fprintf(stderr, "get sqe failed\n");
        goto err;
    }
    
    io_uring_prep_writev(sqe,fd,&(vecs[3]),1,0);
    //io_uring_prep_read_fixed(sqe, fd, vecs[0].iov_base,
                              //vecs[0].iov_len, 0, 0);
    
    //io_uring_prep_read_fixed(sqe, fd, vecs[1].iov_base,
                            //vecs[1].iov_len, 0, 1);

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
    
    printf("CMD 1 result%d\n",cmd0->rsvd2);
    printf("CMD 2 result%d\n",cmd1->rsvd2);
    //io_uring_unregister_buffers(ring);
    //free(vecs[0].iov_base);
    return 0;
err:
    return 1;
}


int main(int argc, char **argv){
//设置线程数目，需要读取块的个数，从哪个位置开始读取
    read_block_num = 256*256;
    read_from = 0;
    /*
    if (argc > 2) {
        //thread_num = atoi(argv[1]);
        read_block_num = atoi(argv[1]);
        read_from = atoi(argv[2]);
        if(read_block_num > MAX_BLOCK_NUM||read_block_num == 0){
            fprintf(stderr,"Incorrect block num,setting default value to 1");
            read_block_num = 1;
        }
    } else {
        fprintf(stderr,"Maintain the following format: ./program_name <thread_num> <read_block_num> <read_from>\n");
        return 1;
    }*/
    DEBUG_LOG("get size of struct memory_range_descriptor%d\n",sizeof(struct memory_range_descriptor ));
    struct timeval start,end;
    
    input_buf_size = LBA_SIZE*256;
    int ret = posix_memalign((void**)&output_buf,4096,LBA_SIZE*read_block_num);   
    if(ret != 0||output_buf==NULL){
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
    if(io_fd < 1||admin_fd < 1){
        fprintf(stderr,"Failed to open device,exiting...\n");
        free(output_buf);
        return 0;
    }
    unsigned int input_mem_id=0,output_mem_id=0;
   
    /**
     * 在设备上，创建两片内存空间，一个输入，一个输出，大小一样
     */
    ret = nvme_create_slm_ns(admin_fd,&input_mem_id,input_buf_size);
    ret = nvme_create_slm_ns(admin_fd,&output_mem_id,input_buf_size+4096);
    DEBUG_LOG("create_slm_ns: input_mem_id is %lx, output_mem_id is %lx\n", input_mem_id,output_mem_id);
    if(input_mem_id<0||output_mem_id<0){
        fprintf(stderr,"Failed to create slm,exiting...\n");
        free(output_buf);
        return 0;
    }
    /**
     * 创建一个计算程序
     */
    struct hlsacccompute_program p,*program;
    program = &p;
    program->input_channum = 1;
    program->output_channum = 1;
    program->program_id = 1;
    program->apply_operators_id_map[0] = 1;//申请一个1号算子，对应功能是加密
    program->applyops[0].header.cid = 0;
    program->applyops[0].header.opc = APPLY_OPS;
    program->applyops[0].header.ops_num = 1;
    program->applyops[2].apply_ops_payload2.connections_num = 1;
    program->applyops[2].apply_ops_payload2.connections[0].from = 0 << 4 | 0;
    // ffff表示是通道，请求分配一条出口通道
    program->applyops[2].apply_ops_payload2.connections[0].to = 0xf0;
    program->pauseops[0].header.cid = 0;
    program->pauseops[0].header.opc = SUSPEND_OPS;
    program->pauseops[0].header.ops_num = 1;
    program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    program->freeops[0].header.cid = 0;
    program->freeops[0].header.opc = FORCE_FREE_OPS;
    program->freeops[0].header.ops_num = 1;
    program->freeops[1].generic_ops_payload.op_lists[0] = 0;
    program->input_channel_destination[0] = 0;
    program->apply_ops_size = 3;
    program->apply_operators_num = 1;
    program->esti_executed_time = 35;
    program->max_responded_time = program->esti_executed_time * 3;

    int psize = sizeof(struct hlsacccompute_program);
    ret = nvme_load_hlsacc_program(admin_fd,psize,1,2,program);
    if(ret<0){
        fprintf(stderr,"Failed to load program,exiting...\n");
        free(output_buf);
        return 0;
    }
    ret = nvme_activate_program(admin_fd,1,2);
    if(ret<0){
        fprintf(stderr,"Failed to activate program,exiting...\n");
        free(output_buf);
        return 0;
    }
    /**
     * 创建一个内存集，0元素表示输入，1元素表示输出
     */
    union memory_range_set_decriptor mdes[2];
    mdes[0].payload.mnsid = input_mem_id;
    mdes[0].payload.length = input_buf_size;
    mdes[0].payload.starting_byte = 0;
    mdes[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
    mdes[1].payload.mnsid = output_mem_id;
    mdes[1].payload.length = input_buf_size+4096;
    mdes[1].payload.starting_byte = 0;
    mdes[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
    unsigned int rsid = 0;
    ret = nvme_create_memory_range_set(admin_fd,2,&rsid,2,mdes);
    if(ret != 0){
        fprintf(stderr,"Failed to create memory range set.Exiting...\n");
        free(output_buf);
        return 0;
    }
    struct io_uring ring;
	ret = io_uring_queue_init(8, &ring, 0);
	if (ret) {
		fprintf(stderr, "ring setup failed\n");
		return 1;
	}
    if(registed==false){
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

    DEBUG_LOG("create_memory_range_set:Get rsid%d\n",rsid);
    gettimeofday(&start,nullptr);
   
    for(int i=0;i<1;i++){
    /**
     * 从盘里把数据读取到input_mem
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
        union nvme_source_range *sr = (union nvme_source_range*)(vecs[4].iov_base);
        for(int j=0;j<8;j++){
            sr[j].scc.slba = read_from+i*(input_buf_size/LBA_SIZE)+(j*32);
            sr[j].scc.nlb = 31;
            sr[j].scc.snsid = 1;
        }

        //ret = nvme_slm_copy(io_fd,sr,sizeof(union nvme_source_range)*8,0,0x3,8,input_mem_id);
        //if(ret<0){
        //    fprintf(stderr,"Failed to copy block data to slm,exiting...\n");
        //    free(output_buf);
        //    return 0;
       // }

        program_exec_async(uring_fd,read_from+i*(input_buf_size/LBA_SIZE),0,rsid,1,input_mem_id,&ring);
        /**
         * 尝试运行计算程序
         */
        
        
        //struct AccContext* context_data = nullptr;
        //unsigned int res = 0;
        //ret = nvme_execute_hlsacc_program(io_fd,2,rsid,1,context_data,0,0,0,&res);
        //if(ret != 0){
        //    fprintf(stderr,"Failed to execute program.Exiting...\n");
        //    free(output_buf);
        //    return 0;
        //}
        DEBUG_LOG("iter %d result%d\n",i,res);
        /**
         * 把访问的结果从设备内存中拷贝到主机内存
         */
        
        for(int j=0;j<8;j++){
            ret = nvme_slm_read(io_fd,output_mem_id,j*LBA_SIZE*32,LBA_SIZE*32,output_buf+i*(input_buf_size)+j*(LBA_SIZE*32));
            if(ret != 0){
                fprintf(stderr,"Failed to copy data to host.Exiting...\n");
                free(output_buf);
                return 0;
            }
        }
       /*
        for(int j=0;j<4;j++){
            ret = slm_read_fixed(&ring,output_buf+i*(input_buf_size)+j*(LBA_SIZE)*64,LBA_SIZE*64,uring_fd,output_mem_id,j*LBA_SIZE*64);
            //ret = nvme_slm_read(io_fd,output_mem_id,j*LBA_SIZE*32,LBA_SIZE*32,output_buf+i*(input_buf_size)+j*(LBA_SIZE*32));
            if(ret != 0){
                fprintf(stderr,"Failed to copy data to host.Exiting...\n");
                free(output_buf);
                return 0;
            }
        }
        */

        //ret = nvme_slm_read(io_fd,output_mem_id,0,input_buf_size,output_buf+i*(input_buf_size));
      
        //sleep(1);
       
    }
    gettimeofday(&end,nullptr);
    fprintf(stdout,"Finish blowfish encryption,time used %lf s\n",end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000);
    return 0;
}