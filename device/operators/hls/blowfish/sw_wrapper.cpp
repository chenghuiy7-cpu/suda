#include "sw_wrapper.h"
#include "blowfish.hpp"

#ifndef USING_XILINX_STREAM
// 静态全局变量
static Acc_Data g_data_in[SW_THREAD_NUM][TX_PORT_NUM];
static Acc_Data g_data_out[SW_THREAD_NUM][RX_PORT_NUM];
static ap_uint<512> g_context[SW_THREAD_NUM][OP_NUM][256];
//static hls::stream<ap_uint<TDEST_WIDTH>> g_operator_done_signal;

// 实现data_in_write函数 - 直接从用户缓冲区写入
extern "C" int  data_in_write(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id) {
    #ifndef __SYNTHESIS__
        if(buffer == NULL)
            return -1;
        g_data_in[thread_id][port_id].reset(buffer,size/64,size/64);
    #endif

    return 0;
}

// 实现data_out_read函数 - 直接写入用户缓冲区
extern "C" int data_out_read(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id) {
    #ifndef __SYNTHESIS__
        if(buffer == NULL)
            return -1;
        g_data_out[thread_id][port_id].reset(buffer,size/64,0);
    #endif
    return 0;
}

// 实现context写入函数
extern "C" void context_write(unsigned long long* data, int size,unsigned int op_id,int thread_id) {
    memcpy((char*)(&(g_context[thread_id][op_id][0])),data,size);
}


// 主运行函数 - 使用用户提供的缓冲区
extern "C" int run(int thread_id) {
    // 运行主处理函数
    //printf("run\n");
    blowfish(g_data_in[thread_id][0], g_data_out[thread_id][0]);
    
    return 0;
}

extern "C" int data_last(unsigned int port_id,int thread_id){
    bool last = g_data_out[thread_id][port_id].last();
    if(last){
        //printf("LAST\n");
    }
    return g_data_out[thread_id][port_id].last();
}

extern "C" void run_with_data(unsigned long long*data_in,unsigned long long* data_out){
    for(int j=0;j<256*256;j++)
        for(int i=0;i<64;i++){
            blowfish_encrypt_512mini(data_in+(i*8),data_out+(i*8));
        }
}

#endif