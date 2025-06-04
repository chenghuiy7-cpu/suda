#pragma once
#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "constexpr_math.hpp"
#include "memory_access.hpp"
#include "hlsacc_stream.hpp"

#define TDEST_WIDTH 8
#define TDATA_WIDTH 512
#define TUSER_WIDTH 8
#define TID_WIDTH 8
#define FIFO_SIZE 4096// 4KB
#define PAGE_SIZE 4096
#define SQCQ_DEPTH 32
#define SQCQ_PTR_WIDTH (int)constexpr_math::log2_floor(SQCQ_DEPTH)
#define STATIC_VAR_SIZE 2048


#define CTRL_REQ_DONT_READ_CONTEXT 0x1

union CtrlReqCmd;
union CtrlRspRet;


typedef hls::axis<CtrlReqCmd,0,0,TDEST_WIDTH> Ctrlreq_Pkt;
typedef hls::stream<Ctrlreq_Pkt> CtrlReq;
typedef hls::axis<CtrlRspRet,0,TID_WIDTH,0> Ctrlrsp_Pkt;
typedef hls::stream<Ctrlrsp_Pkt> CtrlRsp;



typedef ap_axis<TDATA_WIDTH,TUSER_WIDTH,TID_WIDTH,TDEST_WIDTH> Acc_Data_Pkt;

#ifdef USING_XILINX_STREAM
using Acc_Data = hls::stream<Acc_Data_Pkt>;
#else
using Acc_Data =  hlsacc::stream<TDATA_WIDTH,TUSER_WIDTH,TID_WIDTH,TDEST_WIDTH>;
static_assert(std::is_same<Acc_Data_Pkt,hls::axis<ap_int<TDATA_WIDTH>, TUSER_WIDTH, TID_WIDTH, TDEST_WIDTH>>::value,
"Acc_Data_Pkt type mismatch");
#endif

typedef hls::stream<DataMoverCmd> Context_Cmd;

/// @brief 加速算子内部状态机的状态
enum AccFsmState
{
    IDLE,
    WORKING,
    STOPED,
    RECOVERY,
    SAVE,
    IDONKNOW
};


/// @brief 调度器向加速器发送的请求类型
enum CtrlType
{
    APPLY, //收到APPLY_OPS命令会首先往加速器发送的命令类型
    APPLY_STAGE2,//收到APPLY命令响应后，需要发送的命令类型
    FORCE_FREE,
    QUERY,
    SUSPEND,
    RESUME
};


union CtrlReqCmd
{
    struct{
        CtrlType type:16;
        unsigned short flag:16;
        unsigned short payload_length:16;
        unsigned short resvd:16;
        unsigned long long context_address:64;      //仅在Apply命令的时候启用
    }header;

    struct {
      struct {
        unsigned char from:8;
        unsigned char to:8;        
      }connections[8]; 
    }apply_payload;
};


enum CtrlState
{
    FAILED,
    SUCCESS,
    UNDEFINED_FAILED,
    SUCCESS_STAGE1,
    OPERATOR_REAL_FIN
};


#pragma pack(1)
union CtrlRspRet
{
    struct{
        union{
            struct{
                AccFsmState fsm_state:16;
                unsigned char resvd0[4];
                unsigned long long resvd[7];
            }query_meta;

            struct{
                unsigned char resvd0[5];
                unsigned char need_save:8;
                unsigned int old_bbt:32;
                unsigned int new_bbt:32;
                unsigned long long old_context_address:64;
                unsigned long long new_context_address:64;
                unsigned long long old_static_var_size;
                unsigned long long new_static_var_size;
                unsigned long long resvd[2];
            }apply_meta;

            struct{
                unsigned char resvd0[6];
                unsigned long long resvd1[7];
            }generic_meta;
        }meta;
        CtrlState state:16;
    }header;

    struct{
        unsigned long long data[8];
    }apply_payload;

};
#pragma pack()

enum AssSchedCmdOpcode{
    FORCE_FREE_OPS,   //强行释放一组算子
    APPLY_OPS,        //申请一组算子
    QUERY_OPS,        //查询一组算子
    SUSPEND_OPS,      //挂起一组正在工作的算子
    RESUME_OPS,       //恢复一组挂起算子
};

/// @brief 描述发送给控制器的命令结构
/// @attention 计划一个SQE占据32B
union AssSchedCmd{
    struct{
        unsigned char cid:8;                      //2B
        AssSchedCmdOpcode opc:8;                   //1B
        unsigned char ops_num:8;                   //1B
        unsigned char cmd_length:8;                //1B
        unsigned char resvd[4];                    //3B
    }header;

    struct{
        unsigned long long context_address;
    }apply_ops_payload1;

    struct{
        unsigned short connections_num:16;
        struct{
            unsigned char from:8;
            unsigned char to:8;
        }connections[3];
    }apply_ops_payload2;
    struct{
        struct{                                   
            unsigned char from:8;
            unsigned char to:8;
        }connections[4];
    }apply_ops_payload3;

    struct{
        unsigned char op_lists[8];          
    }generic_ops_payload;

};

typedef CtrlState AssSchedRetState;


union AssSchedRet{
    struct{
        unsigned int cid:8;
        unsigned int ops_num:8;//ops_num only used for query
        AssSchedRetState state:16;  
    }header;

    struct{
       struct{
        unsigned char op:8;
        unsigned char fsm_state:8;
       }state_pair[2];
    }query_payload;
};

/// @brief 描述一个Packet的大小以及TUSER位和TID位的数据
struct StreamBufElmDescriptor{
	unsigned char length:8;
	//unsigned char user;
	unsigned char id:8;
};


/// @brief 每个逻辑调度单元所对应的上下文，存储在内存当中
#pragma pack(1)
struct AccContext{
    //FIFO Descriptor
	struct{
        StreamBufElmDescriptor descriptor[28]; //56B
        unsigned long long descriptor_num; //8B
        struct{//无用数据，现在已经废弃
            	StreamBufElmDescriptor descriptor[28]; //56B
                unsigned long long descriptor_num; //8B
        }rubbish;
        unsigned long long available_buf_num; //8B Optimize Performance
        unsigned long long stream_buf_add_lists[7]; //56B     
    }privated_data;
    //Static Variable Data
    unsigned char static_data[2048];  //2048B
    //Temp Use Data
    //临时被控制器使用的区域
    
};
#pragma pack()


struct StreamBuffer{
    unsigned char buffer[FIFO_SIZE];
};




