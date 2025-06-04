// 包含必要的头文件 / Include necessary header files
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


// 定义逻辑块地址大小为4KB / Define logical block address size as 4KB
#define LBA_SIZE 4096

// 高精度计时器类 / High precision timer class
class Timer {
    public:
      // 构造函数，初始化计时器 / Constructor, initialize timer
      Timer() : m_beg(clock_::now()) {}
      
      // 重置计时器 / Reset timer
      void reset() { m_beg = clock_::now(); }
    
      // 返回经过的时间（毫秒） / Return elapsed time in milliseconds
      double elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock_::now() -
                                                                     m_beg)
            .count();
      }
    
    private:
      // 定义时钟类型和时间单位 / Define clock type and time unit
      typedef std::chrono::high_resolution_clock clock_;
      typedef std::chrono::duration<double, std::ratio<1>> second_;
      std::chrono::time_point<clock_> m_beg;
    };
    
// 生成随机字符 / Generate a random character
char random_char() {
    int min_ascii = 65;  // 'A'
    int max_ascii = 68;  // 'D'
    return min_ascii + rand() % (max_ascii - min_ascii + 1);
}

// 生成匹配字符串 / Generate a string for pattern matching
void generate_compared_str(char* param_str) {
    for (int i = 0; i < 64; i++) {
        if (i < 2) {
            // 只有前两个字符是随机生成的字母 / Only first two characters are random letters
            param_str[i] = random_char();
        } else {
            // 其余填充为0 / Fill the rest with zeros
            param_str[i] = 0;
        }
    }
}
    

// 以4K边界向上取整的函数 / Function to round up to 4K boundary
inline size_t alignTo4K(size_t size) {
    // 使用位操作实现向上取整 / Use bit operations to round up
    // 4K = 4096 = 2^12，掩码为4095 (0xFFF) / 4K = 4096 = 2^12, mask is 4095 (0xFFF)
    return (size + LBA_SIZE - 1) & ~(LBA_SIZE - 1);
}

// CPU实现的匹配逻辑（用于验证结果）/ CPU implementation of matching logic (for result verification)
void cpu_matching(const char* buffer, const char* pattern, std::vector<std::pair<int, int>>& matches) {
    int row = 0;
    int col = 0;
    int size = 1024*1024*1024; // 1GB的数据大小 / 1GB data size
    for (size_t i = 0; i < size - 1; i++) {
        // 检查匹配 / Check for match
        if (buffer[i] == pattern[0] && buffer[i+1] == pattern[1]) {
            matches.push_back(std::make_pair(row, col));
            // 跳到下一行 / Jump to next line
            while (i < size && buffer[i] != '\n') i++;
        }
        
        // 更新行列 / Update row and column
        if (i < size && buffer[i] == '\n') {
            row++;
            col = 0;
        } else {
            col++;
        }
    }
}

// CPU实现的grep匹配函数 / CPU implementation of grep matching function
void grep_matching(char* result, char* buf, char* param_str) {
    int cnt = 0;
    // 对每个块进行匹配 / Match for each block
    for (int i = 0; i < 65536; i++) {
      result[i] = -1; // 初始化结果为-1（未匹配） / Initialize result as -1 (no match)
      for (int j = 0; j < 16384; j++) {
        bool matched = true;
        if (j + 2 <= 16384) {
          // 检查2个字符是否匹配 / Check if 2 characters match
          for (int k = 0; k < 2; k++) {
            cnt++;
            matched &= (param_str[k] == buf[i*16384+(j + k)]);
          }
          if (matched) {
            // 找到匹配，记录位置并跳出 / Found a match, record position and break
            result[i] = j;
            break;
          }
        }
      }
    }
    printf("COUNT%d\n", cnt);
}

int main()
{
    // 创建模式匹配字符串 / Create pattern matching string
    char pattern[64];
    generate_compared_str(pattern);
    
    // 创建计时器 / Create timer
    Timer timer;
    
    // 打开NVMe设备进行管理操作 / Open NVMe device for admin operations
    int fd = nvme_open("nvmq0");
    if(fd == -1){
        printf("Failed to Open NVME Device\n");
        return -1;
    }
    
    // 创建SLM命名空间，用于传输和接收数据 / Create SLM namespaces for transmitting and receiving data
    unsigned int tx_id, rx_id;
    int ret = nvme_create_slm_ns(fd, &tx_id, LBA_SIZE*256*1024); // 创建传输命名空间 / Create transmission namespace
 
    // 打开NVMe数据设备 / Open NVMe data device
    int fd1 = nvme_open("nvmq0n1");
    if(fd1 == 0){
        printf("failed to open device\n");
    }
    
    // 创建接收命名空间 / Create receiving namespace
    ret = nvme_create_slm_ns(fd, &rx_id, LBA_SIZE*256); 
    
    // 分配结果缓冲区 / Allocate result buffer
    char* result_buf;
    posix_memalign((void **)&result_buf, LBA_SIZE*256, LBA_SIZE*256);
    if(result_buf == NULL){
        printf("Failed to allocate data!\n");
    }
    
    // 打开输入和输出文件 / Open input and output files
    FILE* from_fp, *to_fp;
    from_fp = fopen("../../examples/data/grep_input.txt", "r");
    to_fp = fopen("/dev/nvmq0n1", "a+");
    if(from_fp == NULL || to_fp == NULL){
        fprintf(stderr, "Failed to open file\n");
        exit(1);
    }
    
    // 文件数据缓冲区 / File data buffer
    char* file_databuf[128*1024];
    fseek(to_fp, 0, SEEK_SET);
    int cur_read = 0;
    
    // 设置NVMe IO参数 / Setup NVMe IO parameters
    struct nvme_io_args args;
    memset(&args, 0, sizeof(args));
    args.args_size = sizeof(args);
    args.fd = fd1;
    args.nsid = 1;
    args.nlb = 31;
    args.slba = 0;
    
    /*
    // 从文件读取数据写入NVMe设备的代码（已注释） / Code to read data from file and write to NVMe device (commented out)
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
    
    // 创建内存范围集描述符 / Create memory range set descriptors
    union memory_range_set_decriptor mdes[2];
    mdes[0].payload.mnsid = tx_id;                 // 传输命名空间ID / Transmission namespace ID
    mdes[0].payload.length = LBA_SIZE*256*1024;    // 传输长度 / Transmission length
    mdes[0].payload.starting_byte = 0;             // 起始字节 / Starting byte
    mdes[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM; // 设备内存标志 / Device memory flag
    
    mdes[1].payload.mnsid = rx_id;                 // 接收命名空间ID / Receiving namespace ID
    mdes[1].payload.length = LBA_SIZE*257;         // 接收长度 / Receiving length
    mdes[1].payload.starting_byte = 0;             // 起始字节 / Starting byte
    mdes[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM; // 设备内存标志 / Device memory flag
    
    // 创建内存范围集 / Create memory range set
    unsigned int rsid = 0;
    ret = nvme_create_memory_range_set(fd, 2, &rsid, 2, mdes);
    if(ret != 0){
        fprintf(stderr, "Failed to create memory range set.Exiting...\n");
        exit(1);
        return 0;
    }
    
    printf("LOAD PROGRAM\n");
    
    // 创建并初始化计算程序结构 / Create and initialize computation program structure
    struct hlsacccompute_program p, *program;
    memset(&p, 0, sizeof(struct hlsacccompute_program));
    program = &p;
    program->input_channum = 1;                    // 输入通道数 / Number of input channels
    program->output_channum = 1;                   // 输出通道数 / Number of output channels
    program->program_id = 1;                       // 程序ID / Program ID
    program->apply_operators_id_map[0] = 0;        // 算子ID映射 / Operator ID mapping
    program->applyops[0].header.cid = 0;           // 通道ID / Channel ID
    program->applyops[0].header.opc = APPLY_OPS;   // 操作码 / Operation code
    program->applyops[0].header.ops_num = 1;       // 操作数量 / Number of operations
    program->applyops[2].apply_ops_payload2.connections_num = 1; // 连接数量 / Number of connections
    program->applyops[2].apply_ops_payload2.connections[0].from = 0 << 4 | 0; // 连接来源 / Connection source
    
    // ffff表示是通道，请求分配一条出口通道 / ffff indicates a channel, requesting an output channel
    program->applyops[2].apply_ops_payload2.connections[0].to = 0xf0; // 连接目标 / Connection destination
    
    program->pauseops[0].header.cid = 0;           // 暂停操作通道ID / Pause operation channel ID
    program->pauseops[0].header.opc = SUSPEND_OPS; // 暂停操作码 / Pause operation code
    program->pauseops[0].header.ops_num = 1;       // 暂停操作数量 / Number of pause operations
    program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    
    program->freeops[0].header.cid = 0;            // 释放操作通道ID / Free operation channel ID
    program->freeops[0].header.opc = FORCE_FREE_OPS; // 强制释放操作码 / Force free operation code
    program->freeops[0].header.ops_num = 1;        // 释放操作数量 / Number of free operations
    program->freeops[1].generic_ops_payload.op_lists[0] = 0;
    
    program->apply_ops_size = 3;                   // 应用操作大小 / Apply operation size
    program->apply_operators_num = 1;              // 应用算子数量 / Number of apply operators
    program->esti_executed_time = 35;              // 估计执行时间 / Estimated execution time
    program->max_responded_time = program->esti_executed_time * 3; // 最大响应时间 / Maximum response time
 
    // 打开程序库文件 / Open program library file
    FILE* p_fp;
    p_fp = fopen("../../examples/data/libgrep.so.1.0.0", "r");
    if(p_fp == NULL){
        fprintf(stderr, "Failed to open lib!\n");
        exit(1);
    }
    
    // 获取文件状态 / Get file status
    struct stat p_stat;
    fstat(fileno(p_fp), &p_stat);
    fseek(p_fp, 0, SEEK_SET);
    
    // 分配软件数据缓冲区 / Allocate software data buffer
    char* software_data;
    posix_memalign((void **)&software_data, LBA_SIZE, LBA_SIZE+p_stat.st_size);
    if(software_data == NULL){
        fprintf(stderr, "Failed to copy programdata\n");
        exit(1);
    }
    
    // 复制程序结构到软件数据 / Copy program structure to software data
    memcpy(software_data, program, sizeof(struct hlsacccompute_program));
    
    // 设置软件数据指针为空 / Set software data pointer to NULL
    struct hlsacccompute_program* pp = (struct hlsacccompute_program*)software_data;
    pp->software_data = NULL;
    
    // 读取程序库文件到软件数据 / Read program library file to software data
    int read_psize = read(fileno(p_fp), ((void*)software_data)+LBA_SIZE, p_stat.st_size);
    if(read_psize != p_stat.st_size){
        fprintf(stderr, "Failed to read program size\n");
        exit(1);
    }
  
    // 加载HLSACC程序 / Load HLSACC program
    ret = nvme_load_hlsacc_program(fd, read_psize+LBA_SIZE, 1, 2, software_data);
    if(ret != 0){
        printf("ERR Failed to load hlsacc program!\n");
        return -1;
    }
    
    // 激活程序 / Activate program
    ret = nvme_activate_program(fd, 1, 2);
    if(ret != 0){
        printf("ERR Failed to activate hlsacc program!\n");
        return -1;
    }
    
    // 源范围数组，用于SLM复制 / Source range array for SLM copy
    union nvme_source_range sr[256];
    
    // 运行两次测试 / Run two tests
    for(int x = 0; x < 2; x++){
        timer.reset();
        
        // 从设备读取数据到SLM / Copy data from device to SLM
        for(int i = 0; i < ((1024*1024*1024)/(128*1024*64)); i++){
            for(int j = 0; j < (64); j++){
                // 设置源范围参数 / Set source range parameters
                sr[j].scc.slba = (i*(128*1024*64) + j*128*1024)/LBA_SIZE; // 源LBA / Source LBA
                sr[j].scc.nlb = 31;                                       // 块数量-1 / Number of blocks minus 1
                sr[j].scc.snsid = 1;                                      // 源命名空间ID / Source namespace ID
            }
            // 执行SLM复制 / Perform SLM copy
            nvme_slm_copy(fd1, sr, sizeof(union nvme_source_range)*64, i*(128*1024*64), 0x3, 64, tx_id);
            
        }
        std::cout << "拷贝 用时: " << timer.elapsed() << " ms" << std::endl;
        timer.reset();
        
        // 定义grep上下文结构 / Define grep context structure
        struct GrepContext {
            char pattern[2];                          // 只存储前两个字符作为匹配模式 / Store only first two characters as matching pattern
            int row_index;                            // 当前处理的行 / Current row being processed
            int col_index;                            // 当前处理的列 / Current column being processed
            int arr_index;                            // 当前匹配结果数组索引 / Current matching result array index
            int row_indices[512/64];                  // 匹配的行索引 / Row indices of matches
            int col_indices[512/64];                  // 匹配的列索引 / Column indices of matches
            int prev_row_index;                       // 前一个匹配的行索引 / Previous matching row index
        };
        
        // 初始化上下文 / Initialize context
        char context_raw[4096];
        GrepContext* context = (GrepContext*)context_raw;
    
        memset(context_raw, 0, 4096);
        context->pattern[0] = pattern[0];             // 设置模式第一个字符 / Set first character of pattern
        context->pattern[1] = pattern[1];             // 设置模式第二个字符 / Set second character of pattern
        
        // 执行结果 / Execution result
        unsigned int result = 0;
        
        // 执行HLSACC程序 / Execute HLSACC program
        nvme_execute_hlsacc_programV2(fd1, 2, rsid, 1, (struct AccContext*)context, 1, 0, 0, &result, 1);
        std::cout << "执行程序 用时: " << timer.elapsed() << " ms" << std::endl;
        timer.reset();
        
        // 从SLM读取结果 / Read result from SLM
        ret = nvme_slm_read(fd1, rx_id, 0, 4096, (void*)result_buf);
        if(ret != 0){
            printf("Failed to use slm cmd\n");
            return 0;
        }
        std::cout << "读取 用时: " << timer.elapsed() << " ms" << std::endl;
    
    }
    
    printf("run baseline way\n");
    
    // 重置计时器 / Reset timer
    timer.reset();
    
    // 分配输入缓冲区 / Allocate input buffer
    void* input_buf;
    posix_memalign((void **)&input_buf, LBA_SIZE*256*1024, LBA_SIZE*256*1024);
    if(result_buf == NULL){
        printf("Failed to allocate data!\n");
    }
    
    // 运行两次基准测试 / Run two baseline tests
    for(int j = 0; j < 2; j++){
        timer.reset();
        args.slba = 0;
        args.nlb = 31;
        int rcnt = 0;
        
        // 从设备读取数据到主机内存 / Read data from device to host memory
        for(uint32_t i = 0; i < 256*1024;){
            rcnt++;
            args.data = input_buf + i*LBA_SIZE * (args.nlb + 1);
            args.data_len = LBA_SIZE * (args.nlb + 1);
            nvme_read(&args);
            i += args.nlb + 1;
            args.slba += args.nlb + 1;
            args.nlb = 31;
        }
        
        std::cout << "拷贝用时: " << timer.elapsed() << " ms" << " rcnt " << rcnt << std::endl;
        
        // 可能有bug所以从SLM复制 / May have bug so copy from SLM
        timer.reset();
        
        // 在CPU上执行grep匹配 / Perform grep matching on CPU
        grep_matching((char*)result_buf, (char*)input_buf, pattern);
        
        std::cout << "处理用时: " << timer.elapsed() << " ms" << std::endl;
    }
    return 0;
}