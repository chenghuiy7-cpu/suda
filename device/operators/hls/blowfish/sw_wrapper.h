#ifndef ACC_EXAMPLE_PLUS_WRAPPER_H
#define ACC_EXAMPLE_PLUS_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#define TX_PORT_NUM 1
#define RX_PORT_NUM 1
#define OP_NUM 1
#define SW_THREAD_NUM 2
// 配置结构体
typedef struct {
    unsigned long long* input_buffer;   // 用户输入缓冲区（512位对齐）
    unsigned long long* output_buffer;  // 用户输出缓冲区（512位对齐）W
    unsigned int input_size;           // 输入大小（以512位为单位）
    unsigned int output_size;          // 输出大小（以512位为单位）
} acc_buffer_config_t;

// 对外暴露的函数接口
__attribute__((visibility("default"))) int  data_in_write(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id);
__attribute__((visibility("default"))) int  data_out_read(unsigned long long* buffer, unsigned int size, unsigned int port_id,int thread_id);
__attribute__((visibility("default"))) void context_write(unsigned long long* data, int size,unsigned int op_id,int thread_id);
__attribute__((visibility("default"))) int  run(int thread_id);
__attribute__((visibility("default"))) int  data_last(unsigned int port_id,int thread_id);
/**
 * @deprecated
 */
__attribute__((visibility("default"))) void run_with_data(unsigned long long*data_in,unsigned long long* data_out);
#ifdef __cplusplus
}
#endif

#endif // ACC_EXAMPLE_PLUS_WRAPPER_H