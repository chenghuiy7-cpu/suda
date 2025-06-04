#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "libnvme.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <cstdio>


#define LBA_SIZE 4096

class Timer {
    public:
      Timer() : m_beg(clock_::now()) {}
      void reset() { m_beg = clock_::now(); }
    
      double elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock_::now() -
                                                                     m_beg)
            .count();
      }
    
    private:
      typedef std::chrono::high_resolution_clock clock_;
      typedef std::chrono::duration<double, std::ratio<1>> second_;
      std::chrono::time_point<clock_> m_beg;
    };
    
// 生成随机字符
char random_char() {
    int min_ascii = 65;  // 'A'
    int max_ascii = 68;  // 'D'
    return min_ascii + rand() % (max_ascii - min_ascii + 1);
}

// 生成匹配字符串
void generate_compared_str(char* param_str) {
    for (int i = 0; i < 64; i++) {
        if (i < 2) {
            param_str[i] = random_char();
        } else {
            param_str[i] = 0;
        }
    }
}
    

// 以4K边界向上取整的函数
inline size_t alignTo4K(size_t size) {
    // 使用位操作实现向上取整
    // 4K = 4096 = 2^12，掩码为4095 (0xFFF)
    return (size + LBA_SIZE - 1) & ~(LBA_SIZE - 1);
}

// CPU实现的匹配逻辑（用于验证结果）
void cpu_matching(const char* buffer, const char* pattern, std::vector<std::pair<int, int>>& matches) {
    int row = 0;
    int col = 0;
    int size = 1024*1024*1024;
    for (size_t i = 0; i < size - 1; i++) {
        // 检查匹配
        if (buffer[i] == pattern[0] && buffer[i+1] == pattern[1]) {
            matches.push_back(std::make_pair(row, col));
            // 跳到下一行
            while (i < size && buffer[i] != '\n') i++;
        }
        
        // 更新行列
        if (i < size && buffer[i] == '\n') {
            row++;
            col = 0;
        } else {
            col++;
        }
    }
}

void grep_matching(char* result,char* buf,char* param_str) {
 
    int cnt = 0;
    for (int i = 0; i < 65536; i++) {
      result[i] = -1;
      for (int j = 0; j < 16384; j++) {
        bool matched = true;
        if (j + 2 <= 16384) {
          for (int k = 0; k < 2; k++) {
            cnt ++;
            matched &= (param_str[k] == buf[i*16384+(j + k)]);
          }
          if (matched) {
            result[i] = j;
            break;
          }
        }
      }
    }
    printf("COUNT%d\n",cnt);
  }

int main()
{
    char pattern [64];
    generate_compared_str(pattern);
    Timer timer;
    int fd = nvme_open("nvmq0");
    if(fd==-1){
        printf("Failed to Open NVME Device\n");
        return -1;
    }
    unsigned int tx_id,rx_id;
    int ret = nvme_create_slm_ns(fd,&tx_id,LBA_SIZE*256*1024);
 
    int fd1 = nvme_open("nvmq0n1");
    if(fd1==0){
        printf("failed to open device\n");
    }
    ret = nvme_create_slm_ns(fd,&rx_id,LBA_SIZE*256); 
    char* result_buf;
    posix_memalign((void **)&result_buf, LBA_SIZE*256, LBA_SIZE*256);
    if(result_buf==NULL){
        printf("Failed to allocate data!\n");
    }
    FILE* from_fp,*to_fp;
    from_fp = fopen("../../examples/data/grep_input.txt","r");
   
    to_fp = fopen("/dev/nvmq0n1","a+");
    if(from_fp==NULL||to_fp==NULL){
        fprintf(stderr,"Failed to open file\n");
        exit(1);
    }
    char* file_databuf[128*1024];
    fseek(to_fp,0,SEEK_SET);
    int cur_read = 0;
    struct nvme_io_args args;
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd1;
    args.nsid = 1;
    args.nlb = 31;
    args.slba = 0;
    /*
    while(1){
        size_t nread = fread(file_databuf+cur_read,128*1024,1,from_fp);
        
        if(nread==0||cur_read>=1024*1024*1024){
            if(cur_read>=1024*1024*1024)
                break;
            if(feof(from_fp)){
                break;
            }else{
                fprintf(stderr,"Failed to open data\n");
                exit(1);
            }
        }else{
            //fwrite(file_databuf,128*1024,1,to_fp);
            args.data = file_databuf + cur_read;
            args.data_len = LBA_SIZE * (args.nlb + 1);
            nvme_write(&args);
            cur_read+=nread;
            args.slba += args.nlb + 1;
            args.nlb = 31;
        
        
        }
    }*/
    union memory_range_set_decriptor mdes[2];
    mdes[0].payload.mnsid = tx_id;
    mdes[0].payload.length = LBA_SIZE*256*1024;
    mdes[0].payload.starting_byte = 0;
    mdes[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
    mdes[1].payload.mnsid = rx_id;
    mdes[1].payload.length = LBA_SIZE*257;
    mdes[1].payload.starting_byte = 0;
    mdes[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
    unsigned int rsid = 0;
    ret = nvme_create_memory_range_set(fd,2,&rsid,2,mdes);
    if(ret != 0){
        fprintf(stderr,"Failed to create memory range set.Exiting...\n");
        exit(1);
        return 0;
    }
    printf("LOAD PROGRAM\n");
    struct hlsacccompute_program p,*program;
    memset(&p,0,sizeof(struct hlsacccompute_program));
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
 
    FILE* p_fp;
    p_fp = fopen("../../examples/data/libgrep.so.1.0.0","r");
    if(p_fp==NULL){
        fprintf(stderr,"Failed to open lib!\n");
        exit(1);
    }
    struct stat p_stat;
    fstat(fileno(p_fp),&p_stat);
    fseek(p_fp,0,SEEK_SET);
    char* software_data;
    posix_memalign((void **)&software_data, LBA_SIZE, LBA_SIZE+p_stat.st_size);
    if(software_data==NULL){
        fprintf(stderr,"Failed to copy programdata\n");
        exit(1);
    }
    memcpy(software_data,program,sizeof(struct hlsacccompute_program));
    
    struct hlsacccompute_program* pp = (struct hlsacccompute_program*)software_data;
    pp->software_data = NULL;
    int read_psize = read(fileno(p_fp),((void*)software_data)+LBA_SIZE,p_stat.st_size);
    if(read_psize != p_stat.st_size){
        fprintf(stderr,"Failed to read program size\n");
        exit(1);
    }
  
    ret = nvme_load_hlsacc_program(fd,read_psize+LBA_SIZE,1,2,software_data);
    if(ret!=0){
        printf("ERR Failed to load hlsacc program!\n");
        return -1;
    }
    ret = nvme_activate_program(fd,1,2);
    if(ret!=0){
        printf("ERR Failed to activate hlsacc program!\n");
        return -1;
    }
    union nvme_source_range sr[256];
    for(int x=0;x<2;x++){
        timer.reset();
        
        for(int i=0;i<((1024*1024*1024)/(128*1024*64));i++){
            for(int j=0;j<(64);j++){
                sr[j].scc.slba = (i*(128*1024*64) + j*128*1024)/LBA_SIZE;
                sr[j].scc.nlb = 31;
                sr[j].scc.snsid = 1;
            }
            nvme_slm_copy(fd1,sr,sizeof(union nvme_source_range)*64,i*(128*1024*64),0x3,64,tx_id);
            
        }
        std::cout << "拷贝 用时: " << timer.elapsed() << " ms" << std::endl;
        timer.reset();
        struct GrepContext {
            char pattern[2];                          // 只存储前两个字符作为匹配模式
            int row_index;                            // 当前处理的行
            int col_index;                            // 当前处理的列
            int arr_index;                            // 当前匹配结果数组索引
            int row_indices[512/64];                  // 匹配的行索引
            int col_indices[512/64];                  // 匹配的列索引
            int prev_row_index;                       // 前一个匹配的行索引
        };
        char context_raw[4096];
        GrepContext* context = (GrepContext*)context_raw;
    
        memset(context_raw,0,4096);
        context->pattern[0] = pattern[0];
        context->pattern[1] = pattern[1];
        unsigned int result = 0;
        nvme_execute_hlsacc_programV2(fd1,2,rsid,1,(struct AccContext*)context,1,0,0,&result,1);
        std::cout << "执行程序 用时: " << timer.elapsed() << " ms" << std::endl;
        timer.reset();
        ret = nvme_slm_read(fd1,rx_id,0,4096,(void*)result_buf);
        if(ret != 0){
            printf("Failed to use slm cmd\n");
            return 0;
        }
        std::cout << "读取 用时: " << timer.elapsed() << " ms" << std::endl;
    
    }
    
    printf("run baseline way\n");
    
    timer.reset();
    //memset(input_buf,'A',input_buf_size); 
    void* input_buf;
    posix_memalign((void **)&input_buf, LBA_SIZE*256*1024, LBA_SIZE*256*1024);
    if(result_buf==NULL){
        printf("Failed to allocate data!\n");
    }
    for(int j=0;j<2;j++){
        timer.reset();
        args.slba = 0;
        args.nlb = 31;
        int rcnt = 0;
        for(uint32_t i=0;i<256*1024;){
            rcnt++;
            args.data = input_buf + i*LBA_SIZE * (args.nlb + 1);
            args.data_len = LBA_SIZE * (args.nlb + 1);
            nvme_read(&args);
            i += args.nlb + 1;
            args.slba += args.nlb + 1;
            args.nlb = 31;
        }
        
        std::cout << "拷贝用时: " << timer.elapsed() << " ms" << " rcnt " << rcnt << std::endl;
        //May has bug so copy from slm
        timer.reset();
        //std::vector<std::pair<int, int>> cpu_result;
        
        //grep_matching((char*)result_buf,(char*)input_buf,pattern);
        //cpu_matching((char*)input_buf,pattern,cpu_result);
        //std::cout << "处理用时: " << timer.elapsed() << " ms" << std::endl;
        //timer.reset();
        //for(int i=0;i<(1024*1024*1024/(128*1024));i++){
        //    nvme_slm_read(fd1,tx_id,i*(128*1024),128*1024,input_buf+(i*128*1024));
       // }
        //timer.reset();
        grep_matching((char*)result_buf,(char*)input_buf,pattern);
        //cpu_matching((char*)input_buf,pattern,cpu_result);
        std::cout << "处理用时: " << timer.elapsed() << " ms" << std::endl;
    }
    return 0;
}
