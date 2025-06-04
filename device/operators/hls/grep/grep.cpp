#include "grep.hpp"
// 简化常量
#define DATA_BUS_WIDTH 64
#define OUTPUT_MATCHES_PER_PKT (TDATA_WIDTH / 64)  // 一个数据包可以存储的匹配数量

// 简化版 Grep 实现
void SimpleGrep(Acc_Data &data_in, Acc_Data &data_out, ap_uint<512> context[256]) {
    #pragma HLS INTERFACE mode=axis register_mode=off port=data_in
    #pragma HLS INTERFACE mode=axis register_mode=off port=data_out
    #pragma HLS INTERFACE mode=bram port=context
    
        // 获取上下文
        GrepContext *grep_context = (GrepContext*)(((struct AccContext*)context)->static_data);
        
        // 读取输入数据包
        static Acc_Data_Pkt input_pkt;
        static Acc_Data_Pkt output_pkt;
        bool matched = false;
        static int cnt = 0;
        static int pcnt = 0;
        unsigned int* indices = (unsigned int*)&(output_pkt.data);
        //int c = 0;
        while (pcnt!=16777216) {
            //c++;
            data_in.read(input_pkt);
            // 检查是否是结束命令
            // 提取数据并处理
            char* data_bytes;
            bool has_newline = false;
            data_bytes = (char*)(&(input_pkt.data));
            // 从数据包中提取字节
            // 匹配逻辑 - 只检查前两个字符
            pcnt++;
            if(cnt == 256){
                cnt = 0;
                matched = false;
                grep_context->row_index++;
                grep_context->col_index = 0;
            }else {
                ++cnt;
            }
            for (int i = 0; i < DATA_BUS_WIDTH - 1; i++) {
                if (!matched&&data_bytes[i] == grep_context->pattern[0] && 
                    data_bytes[i+1] == grep_context->pattern[1]) {
                        
                        indices[grep_context->arr_index<<1] = grep_context->row_index;
                        indices[grep_context->arr_index<<1+1] = grep_context->col_index + i;
                        grep_context->arr_index++;
                        matched = true;
                        break;
                }else if(matched){
                        break;
                }
            }  
            grep_context->col_index += DATA_BUS_WIDTH;
            
            output_pkt.last = 1;
            output_pkt.user = 0xf0;
            
            // 当积累了足够的匹配结果时，发送一个数据包
            
            if (grep_context->arr_index == TDATA_WIDTH/64) {
                
                output_pkt.last = false;  // 不是最后一个包
                output_pkt.user = input_pkt.user;
                data_out.write(output_pkt);
                grep_context->arr_index = 0;  // 重置匹配计数器
                continue;
            }
            
            
        }
        data_out.write(output_pkt);
    }