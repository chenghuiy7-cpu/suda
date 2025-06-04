#include <cassert>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <random>
#include <ap_int.h>
#include <fstream>
#include "grep.hpp"
// 定义常量
#define FILE_ROW_NUM (65536)
#define FILE_COL_NUM (16384)
#define READ_BUF_SIZE (1024 * 1024 * 2)
#define PARAM_STR_LEN (2)  // 只匹配前两个字符
#define MAX_GREP_PARAM_SIZE (32)
#define DATA_BUS_WIDTH (64)
#define TDATA_WIDTH 512
#define TUSER_WIDTH 8
#define TID_WIDTH 8
#define TDEST_WIDTH 8
#define MAX_MATCHES 1000  // 最大匹配数量
// 自定义计时类（用于性能测量）
class Timer {
    private:
        clock_t start_time;
    public:
        Timer() : start_time(clock()) {}
        void reset() { start_time = clock(); }
        double elapsed() const {
            return (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.0;
        }
    };
    
    // 生成随机字符
    char random_char() {
        int min_ascii = 65;  // 'A'
        int max_ascii = 68;  // 'D'
        return min_ascii + rand() % (max_ascii - min_ascii + 1);
    }
    
    // 生成匹配字符串
    void generate_compared_str(char* param_str) {
        for (int i = 0; i < MAX_GREP_PARAM_SIZE; i++) {
            if (i < PARAM_STR_LEN) {
                param_str[i] = random_char();
            } else {
                param_str[i] = 0;
            }
        }
    }
    
    // 读取文件数据
    bool read_file_data(const char* filename, std::vector<char>& buffer) {
        std::ifstream ifs(filename,std::ios::binary);
        if (!ifs.is_open()) {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return false;
        }
        
        // 获取文件大小
        ifs.seekg(0, std::ios::end);
        std::streamsize file_size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        
        // 调整缓冲区大小
        buffer.resize(file_size);
        
        // 读取文件内容
        if (!ifs.read(buffer.data(), file_size)) {
            std::cerr << "读取文件失败: " << filename << std::endl;
            return false;
        }
        
        ifs.close();
        return true;
    }
    
    // CPU实现的匹配逻辑（用于验证结果）
    void cpu_matching(const std::vector<char>& buffer, const char* pattern, std::vector<std::pair<int, int>>& matches) {
        int row = 0;
        int col = 0;
        
        for (size_t i = 0; i < buffer.size() - 1; i++) {
            // 检查匹配
            if (buffer[i] == pattern[0] && buffer[i+1] == pattern[1]) {
                matches.push_back(std::make_pair(row, col));
                // 跳到下一行
                while (i < buffer.size() && buffer[i] != '\n') i++;
            }
            
            // 更新行列
            if (i < buffer.size() && buffer[i] == '\n') {
                row++;
                col = 0;
            } else {
                col++;
            }
        }
    }
    
    // 准备HLS输入数据
    void prepare_hls_input(const std::vector<char>& buffer, const char* pattern, hls::stream<Acc_Data_Pkt>& data_in) {
        // 首先发送模式数据包
        Acc_Data_Pkt pattern_pkt;
        pattern_pkt.data = 0;
        pattern_pkt.data(7, 0) = pattern[0];
        pattern_pkt.data(15, 8) = pattern[1];
        pattern_pkt.user = 0;
        pattern_pkt.last = 0;
        data_in.write(pattern_pkt);
        
        // 然后发送数据包
        size_t buffer_pos = 0;
        while (buffer_pos < buffer.size()) {
            Acc_Data_Pkt data_pkt;
            data_pkt.data = 0;
            data_pkt.user = 0;
            
            // 填充数据包
            for (int i = 0; i < DATA_BUS_WIDTH && buffer_pos < buffer.size(); i++, buffer_pos++) {
                data_pkt.data((i+1)*8-1, i*8) = buffer[buffer_pos];
            }
            
            // 设置last标志
            data_pkt.last = (buffer_pos >= buffer.size());
            
            data_in.write(data_pkt);
        }
        
        // 发送结束命令
        Acc_Data_Pkt end_pkt;
        end_pkt.data = 0;
        end_pkt.user = 0xf0;
        end_pkt.last = 1;
        data_in.write(end_pkt);
    }
    
    // 解析HLS输出
    void parse_hls_output(hls::stream<Acc_Data_Pkt>& data_out, std::vector<std::pair<int, int>>& matches) {
        while (!data_out.empty()) {
            Acc_Data_Pkt output_pkt;
            data_out.read(output_pkt);
            
            // 解析匹配结果
            for (int idx = 0; idx < 512; idx += 64) {
                int row = output_pkt.data(idx + 31, idx).to_int();
                int col = output_pkt.data(idx + 63, idx + 32).to_int();
                
                // 如果是有效结果（非0），添加到匹配列表
                if (row != 0 || col != 0) {
                    matches.push_back(std::make_pair(row, col));
                }
            }
        }
    }
    
    // 比较结果
    void compare_results(const std::vector<std::pair<int, int>>& cpu_matches, 
                         const std::vector<std::pair<int, int>>& hls_matches) {
        std::cout << "CPU匹配数量: " << cpu_matches.size() << std::endl;
        std::cout << "HLS匹配数量: " << hls_matches.size() << std::endl;
        
        // 检查每个CPU匹配是否在HLS结果中
        int match_count = 0;
        for (const auto& cpu_match : cpu_matches) {
            bool found = false;
            for (const auto& hls_match : hls_matches) {
                if (cpu_match.first == hls_match.first && cpu_match.second == hls_match.second) {
                    found = true;
                    break;
                }
            }
            if (found) {
                match_count++;
            } else {
                std::cout << "未找到匹配: 行=" << cpu_match.first << ", 列=" << cpu_match.second << std::endl;
            }
        }
        
        double match_rate = (cpu_matches.size() > 0) ? 
                             (double)match_count / cpu_matches.size() * 100.0 : 100.0;
        
        std::cout << "匹配率: " << match_rate << "%" << std::endl;
        
        if (match_rate >= 99.0) {
            std::cout << "测试通过！" << std::endl;
        } else {
            std::cout << "测试失败！" << std::endl;
        }
    }
    
    #ifndef __SYNTHESIS__
    // 主测试函数
    int main() {
        // 初始化随机数生成器
        srand(time(NULL));
        
        // 计时器
        Timer timer;
        
        // 生成匹配模式
        char pattern[MAX_GREP_PARAM_SIZE];
        std::cout << "生成匹配模式..." << std::endl;
        timer.reset();
        generate_compared_str(pattern);
        std::cout << "匹配模式: " << pattern[0] << pattern[1] << std::endl;
        std::cout << "用时: " << timer.elapsed() << " ms" << std::endl;
        
        // 读取输入文件
        std::vector<char> buffer;
        std::cout << "读取输入文件..." << std::endl;
        timer.reset();
        if (!read_file_data("/home/lyh/Downloads/INSIDER-System/apps/host/grep/data_gen/grep_input.txt", buffer)) {
            return 1;
        }
        std::cout << "读取完成，文件大小: " << buffer.size() << " 字节" << std::endl;
        std::cout << "用时: " << timer.elapsed() << " ms" << std::endl;
        
        // CPU匹配
        std::vector<std::pair<int, int>> cpu_matches;
        std::cout << "执行CPU匹配..." << std::endl;
        timer.reset();
        cpu_matching(buffer, pattern, cpu_matches);
        std::cout << "CPU匹配完成，找到 " << cpu_matches.size() << " 个匹配" << std::endl;
        std::cout << "用时: " << timer.elapsed() << " ms" << std::endl;
        
        // 准备HLS输入
        hls::stream<Acc_Data_Pkt> data_in("input_stream");
        hls::stream<Acc_Data_Pkt> data_out("output_stream");
        ap_uint<512> context[256];
        
        // 初始化上下文
        GrepContext* grep_context = (GrepContext*)(context);
        memset(grep_context, 0, sizeof(GrepContext));
        grep_context->pattern[0] = pattern[0];
        grep_context->pattern[1] = pattern[1];
        grep_context->prev_row_index = -1;
        
        // 准备输入数据
        std::cout << "准备HLS输入数据..." << std::endl;
        timer.reset();
        prepare_hls_input(buffer, pattern, data_in);
        std::cout << "准备完成，用时: " << timer.elapsed() << " ms" << std::endl;
        
        // 执行HLS匹配
        std::cout << "执行HLS匹配..." << std::endl;
        timer.reset();
        while (!data_in.empty()) {
            SimpleGrep(data_in, data_out, context);
        }
        std::cout << "HLS匹配完成，用时: " << timer.elapsed() << " ms" << std::endl;
        
        // 解析HLS输出
        std::vector<std::pair<int, int>> hls_matches;
        parse_hls_output(data_out, hls_matches);
        
        // 比较结果
        std::cout << "比较结果..." << std::endl;
        compare_results(cpu_matches, hls_matches);
        
        return 0;
    }
    #else
    // 合成版本的主函数（只包含SimpleGrep函数，不包含测试代码）
    int main() {
        hls::stream<Acc_Data_Pkt> data_in;
        hls::stream<Acc_Data_Pkt> data_out;
        ap_uint<512> context[256];
        
        SimpleGrep(data_in, data_out, context);
        
        return 0;
    }
    #endif