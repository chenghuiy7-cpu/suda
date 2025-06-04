#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libnvme.h>

#define LBA_SIZE 4096



int main()
{
    int fd = nvme_open("nvmq0");
    int* data = malloc(4096);
    if(fd==-1){
        printf("Failed to Open NVME Device\n");
        return -1;
    }
    int id = 0;
    int ret = nvme_create_slm_ns(fd,&id,LBA_SIZE*8);
    printf("Get ret %d %x %x\n",ret,id,id&(~(1<<31)));
    
    if((id&(1<<31))!=0){
        printf("continue to delete slm ns\n");
        ret = nvme_delete_slm_ns(fd,id);
        printf("Get ret %d\n",ret);
    }
    free(data);

    printf("Then Try to reallocate an Namespace\n");
    printf("Try To Read Data From Host Use Memory Namespace Read\n");
   

    int fd1 = nvme_open("nvmq0n1");
    if(fd1==0){
        printf("failed to open device\n");
    }
    ret = nvme_create_slm_ns(fd,&id,LBA_SIZE*8); 
    printf("Get ret %d %x %x\n",ret,id,id&(~(1<<31)));
    char* test_data;
    posix_memalign((void **)&test_data, LBA_SIZE*16, LBA_SIZE*16);
    if(test_data==NULL){
        printf("Failed to allocate data!\n");
    }
    for(int i=0;i<8;i++){
        memset(test_data+4096*i,'D'+i,4096);
    }
    printf("Use SLM WRITE CMD\n");
    ret = nvme_slm_write(fd1,id,0,4096*16,(void*)test_data);
    if(ret != 0){
        printf("Failed to use slm cmd\n");
        return 0;
    }
    
    char* test_data1;// = malloc(4096*8);
    posix_memalign((void **)&test_data1, LBA_SIZE*16, LBA_SIZE*16);
    memset(test_data1,'A',4096);
    for(int i=0;i<8;i++){
        memset(test_data1+4096*i,'A'+i,4096);
    }
    printf("Use SLM READ CMD\n");
    ret = nvme_slm_read(fd1,id,0,4096*16,(void*)test_data1);
    
    //printf("Get Read Data First Bytes%c %llx second%llx\n",test_data1[0],*(uint64_t*)test_data1,(((uint64_t*)test_data1)[1]));
    for(int i=0;i<8;i++){
        for(int j=0;j<4096;j++){
            if(test_data1[i*4096+j]!=('D'+i)){
                printf("Char Failed!\n");
                return -1;
            }
        }
    }
    //return 0;
    printf("Use NVM WRITE CMD\n");
    struct nvme_io_args args;
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd1;
    args.nsid = 1;
    void* srcdatabuf = malloc(4096);
    memset(srcdatabuf,'E',4096);
    args.data = srcdatabuf;
    args.data_len = 4096;
    args.slba = 0 / LBA_SIZE;
    ret = nvme_write(&args);
    if(ret!=0) {
    	fprintf(stderr,"NVMe Write Failed: %d\n",ret);
    }
    printf("Use SLM COPY CMD\n");
    union nvme_source_range *sr = calloc(1,sizeof(union nvme_source_range));
    sr->scc.slba = 0;
    sr->scc.nlb = 0;
    sr->scc.snsid = 1;
    /*
    int fd,
	void* source_range_entries,
	unsigned long long length,
	unsigned long long sdaddr, 
	unsigned char format_sel,
	unsigned char nr,
	int nsid
    */
    nvme_slm_copy(fd1,sr,sizeof(union nvme_source_range),0,0x3,1,id);

    printf("Use NVM COPY CMD\n");
    struct nvme_copy_args copy_args;
    copy_args.args_size = sizeof_args(struct nvme_copy_args, format, __u64);
    copy_args.fd = fd1;
    copy_args.nr = 1;
    copy_args.result = NULL;
    copy_args.nsid = 1;
    
    copy_args.copy = malloc(4096);
    copy_args.format = 4;
    struct  nvme_mc_source_range* copy = copy_args.copy;
    /*
    uint32_t snsid;
	uint32_t reserved0;
	uint64_t saddr;
	uint32_t nbyte;
	uint16_t reserved1;
	uint16_t source_options;
	uint64_t reserved2;
    */
    copy->snsid = id;
    copy->saddr = 4096*2;
    copy->nbyte = 4096;
    nvme_copy(&copy_args);

    printf("Use NVMe READ\n");
    void* dstdatabuf = srcdatabuf;
    ret = nvme_read(&args);
    if(ret!=0) {
    	fprintf(stderr,"NVMe Read Failed: %d\n",ret);
    }
    printf("Get Block Value %llx\n",((unsigned long long*)dstdatabuf)[0]);

    
    union memory_range_set_decriptor mdes[2];
    mdes[0].payload.mnsid = id;
    mdes[0].payload.length = 4096;
    mdes[0].payload.starting_byte = 0;
    mdes[0].payload.flag = MEM_RANGE_DEVICE_MEM;
    mdes[1].payload.mnsid = id;
    mdes[1].payload.length = 4096*2;
    mdes[1].payload.starting_byte = 4096;
    mdes[1].payload.flag = MEM_RANGE_DEVICE_MEM;
    int rsid = 0;
    ret = nvme_create_memory_range_set(fd,2,&rsid,2,mdes);
    if(ret != 0){
        printf("Error Failed to create memory range set!\n");
        return -1;
    }
    printf("Get RSID%d\n",rsid);
    /*
    ret = nvme_delete_memory_range_set(fd,2,rsid);
    if(ret != 0){
        printf("Error Failed to delete memory range set!\n");
        return -1;
    }*/

    struct hlsacccompute_program p,*program;
    program = &p;
    program->input_channum = 1;
    program->output_channum = 1;
    program->program_id = 1;
    program->apply_operators_id_map[0] = 0;
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
    program->apply_ops_size = 3;
    program->apply_operators_num = 1;
    program->esti_executed_time = 35;
    program->max_responded_time = program->esti_executed_time * 3;

    int psize = sizeof(struct hlsacccompute_program);
    char* software_data = malloc(4096*9);
    memcpy(software_data,program,sizeof(struct hlsacccompute_program));
    /*
    printf("LOAD PROGRAM\n");
    ret = nvme_load_hlsacc_program(fd,psize,1,2,program);
    if(ret!=0){
        printf("ERR Failed to load hlsacc program!\n");
        return -1;
    }
    printf("UNLOAD PROGRAM\n");
    ret = nvme_unload_hlsacc_program(fd,1,2);
    if(ret!=0){
        printf("ERR Failed to unload hlsacc program!\n");
        return -1;
    }
    */
    
    struct hlsacccompute_program* pp = software_data;
    printf("OPS SIZE DUMP%d\n",pp->apply_ops_size);
    for(int i=0;i<8;i++){
        for(int j=0;j<4096;j++){
            software_data[(i+1)*4096+j] = 'A' + i;
        }
    }
    printf("SIZEOF ASSCMD%lx\n",sizeof(union AssSchedCmd));
    printf("RELOAD PROGRAM\n");
    psize = 4096*9;
    sleep(2);
    ret = nvme_load_hlsacc_program(fd,psize,1,2,software_data);
    if(ret!=0){
        printf("ERR Failed to load hlsacc program!\n");
       // return -1;
    }
    
    printf("ACTIVATE PROGRAM\n");
    ret = nvme_activate_program(fd,1,2);
    if(ret!=0){
        printf("ERR Failed to load acivate program!\n");
        return -1;
    }
    
    printf("EXECUTE PROGRAM!\n");
    sleep(2);
    int exec_result=2;
    struct AccContext context[3];
    memset(context,0,sizeof(struct AccContext)*3);
    memset(context[0].static_data,'A',2048);
    memset(context[1].static_data,'B',2048);
    memset(context[1].static_data,'C',2048);
    void* context_data = malloc(sizeof(4096*3));
    memcpy(context_data,context,sizeof(struct AccContext));
    memcpy(context_data+4096,&(context[1]),sizeof(struct AccContext));
    memcpy(context_data+4096*2,&(context[2]),sizeof(struct AccContext));
    ret = nvme_execute_hlsacc_program(fd1,2,rsid,1,context[0].static_data,1,0,0,&exec_result);
    if(ret!=0){
        printf("ERR Failed to Execute program!\n");
        return -1;
    }
    printf("Result%d\n",exec_result);
    
    printf("Get Log Pages Cmd\n");
    
    unsigned long* log_page_data = malloc(4096);
    memset(log_page_data,0,4096);
    //ret = nvme_get_log_simple(fd,0x82,4096,(void*)log_page_data);
    ret = nvme_get_nsid_log(fd, false, 0x82, 2, 4096, (void*)log_page_data);
    if(ret!=0){
        printf("ERR Failed to Get Log Pages!\n");
        return -1;
    }
    union nvme_prog_desc_list* desclist = log_page_data;
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

    return 0;
}
