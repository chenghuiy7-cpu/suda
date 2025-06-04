#pragma once
#include "hlsacc_types.hpp"
// 简化版 Grep 函数声明
void SimpleGrep(Acc_Data &data_in, Acc_Data &data_out, ap_uint<512> context[256]);
// 简化版 Context 结构
struct GrepContext {
    char pattern[2];                          // 只存储前两个字符作为匹配模式
    int row_index;                            // 当前处理的行
    int col_index;                            // 当前处理的列
    int arr_index;                            // 当前匹配结果数组索引
    int row_indices[TDATA_WIDTH/64];          // 匹配的行索引
    int col_indices[TDATA_WIDTH/64];          // 匹配的列索引
    int prev_row_index;                       // 前一个匹配的行索引
};