#include "hwgrep.hpp"

// 简化常量
#define DATA_BUS_WIDTH 64
#define OUTPUT_MATCHES_PER_PKT (TDATA_WIDTH / 64)

// HLS 友好版 Grep，实现与 SimpleGrep 完全一致，接口和上下文保持不变
void HwGrep(Acc_Data &data_in, Acc_Data &data_out, ap_uint<512> context[256]) {
    #pragma HLS INTERFACE mode=axis register_mode=off port=data_in
    #pragma HLS INTERFACE mode=axis register_mode=off port=data_out
    #pragma HLS INTERFACE mode=bram port=context
    #pragma HLS INLINE off

    // ----------------------------
    // 从 BRAM 中获取上下文
    // ----------------------------
    GrepContext *grep_context = (GrepContext*)(((struct AccContext*)context)->static_data);

    // ----------------------------
    // 使用局部变量缓存上下文（避免 BRAM 读写冲突）
    // ----------------------------
    char pattern0 = grep_context->pattern[0];
    char pattern1 = grep_context->pattern[1];
    int row_index = grep_context->row_index;
    int col_index = grep_context->col_index;
    int arr_index = grep_context->arr_index;

    bool matched = false;
    int cnt = 0;
    int pcnt = 0;

    // ----------------------------
    // 临时包定义
    // ----------------------------
    static Acc_Data_Pkt input_pkt;
    static Acc_Data_Pkt output_pkt;
    unsigned int* indices = (unsigned int*)&(output_pkt.data);

    // ----------------------------
    // 主循环（完全保留原逻辑）
    // ----------------------------
    while (pcnt != 16777216) {
        #pragma HLS PIPELINE II=1
        if (!data_in.empty()) {
            data_in.read(input_pkt);
            pcnt++;

            char* data_bytes = (char*)(&(input_pkt.data));

            if (cnt == 256) {
                cnt = 0;
                matched = false;
                row_index++;
                col_index = 0;
            } else {
                ++cnt;
            }

            // 匹配逻辑保持一致
            for (int i = 0; i < DATA_BUS_WIDTH - 1; i++) {
                #pragma HLS UNROLL
                if (!matched && data_bytes[i] == pattern0 &&
                    data_bytes[i+1] == pattern1) {
                    indices[arr_index << 1] = row_index;
                    indices[(arr_index << 1) + 1] = col_index + i;
                    arr_index++;
                    matched = true;
                    break;
                } else if (matched) {
                    break;
                }
            }

            col_index += DATA_BUS_WIDTH;
            output_pkt.last = 1;
            output_pkt.user = 0xf0;

            if (arr_index == (TDATA_WIDTH / 64)) {
                output_pkt.last = false;
                output_pkt.user = input_pkt.user;
                data_out.write(output_pkt);
                arr_index = 0;
                continue;
            }
        }
    }

    // ----------------------------
    // 写回上下文（用于抢占恢复）
    // ----------------------------
    grep_context->row_index = row_index;
    grep_context->col_index = col_index;
    grep_context->arr_index = arr_index;

    // 其他上下文字段保持不变
    data_out.write(output_pkt);
}
