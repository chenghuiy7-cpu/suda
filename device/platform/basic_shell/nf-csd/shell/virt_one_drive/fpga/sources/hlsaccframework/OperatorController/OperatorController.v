`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/07/2024 09:39:37 PM
// Design Name: 
// Module Name: OperatorController
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////

`define CTRL_REQ_TDATA_WIDTH 128
`define DESCRIPTOR_START_ADDRESS 0
`define STATIC_VAR_START_ADDRESSS 3
`define FIFO_BUF_START_ADDRESS 2
`define BRAM_WIDTH 512
`define OP_DEBUG 1 
module OperatorController
#(
    parameter BRAM_DEPTH = 2240,
    parameter BRAM_STATIC_VAR_DEPTH = 'd2048,
    parameter FIFO_DEPTH = 4096,
    parameter MEM_PAGE_SIZE = 4096,
    parameter DATA_WIDTH = 512,
    parameter FIFO_NUM = 1,
    parameter DESTID_WIDTH = 8,
    parameter KEEP_WIDTH = ((DATA_WIDTH+7)/8),
    parameter USER_WIDTH = 8
)
(
     /*
     * AXI input
     */
    input  wire [FIFO_NUM*DATA_WIDTH-1:0]       s_axis_inside_tdata,
    input  wire [FIFO_NUM*KEEP_WIDTH-1:0]       s_axis_inside_tkeep,
    input  wire [FIFO_NUM-1:0]                  s_axis_inside_tvalid,
    output wire [FIFO_NUM-1:0]                  s_axis_inside_tready,
    input  wire [FIFO_NUM-1:0]                  s_axis_inside_tlast,
    input  wire [FIFO_NUM*USER_WIDTH-1:0]       s_axis_inside_tuser,
    
    
    
    input  wire [FIFO_NUM*DATA_WIDTH-1:0]       s_axis_outside_tdata,
    input  wire [FIFO_NUM*KEEP_WIDTH-1:0]       s_axis_outside_tkeep,
    input  wire [FIFO_NUM-1:0]                  s_axis_outside_tvalid,
    output wire [FIFO_NUM-1:0]                  s_axis_outside_tready,
    input  wire [FIFO_NUM*USER_WIDTH-1:0]       s_axis_outside_tuser,
    input  wire [FIFO_NUM-1:0]                  s_axis_outside_tlast,

    /*
     * AXI output
     */
    output wire [FIFO_NUM*DATA_WIDTH-1:0]       m_axis_inside_tdata,
    output wire [FIFO_NUM*KEEP_WIDTH-1:0]       m_axis_inside_tkeep,
    output wire [FIFO_NUM-1:0]                  m_axis_inside_tvalid,
    input  wire [FIFO_NUM-1:0]                  m_axis_inside_tready,
    output wire [FIFO_NUM-1:0]                  m_axis_inside_tlast,
    output wire [FIFO_NUM*DESTID_WIDTH-1:0]     m_axis_inside_tid,
    output wire [FIFO_NUM*DESTID_WIDTH-1:0]     m_axis_inside_tdest,
    output wire [FIFO_NUM*USER_WIDTH-1:0]       m_axis_inside_tuser,
    
    
    
    output wire [FIFO_NUM*DATA_WIDTH-1:0]       m_axis_outside_tdata,
    output wire [FIFO_NUM*KEEP_WIDTH-1:0]       m_axis_outside_tkeep,
    output wire [FIFO_NUM-1:0]                  m_axis_outside_tvalid,
    input  wire [FIFO_NUM-1:0]                  m_axis_outside_tready,
    output wire [FIFO_NUM-1:0]                  m_axis_outside_tlast,
    output wire [FIFO_NUM*DESTID_WIDTH-1:0]     m_axis_outside_tdest,
    output  wire [FIFO_NUM*USER_WIDTH-1:0]       m_axis_outside_tuser,

        
    /*
    *CtrlInput
    */
    input  wire [(`CTRL_REQ_TDATA_WIDTH)-1:0]  ctrl_req_from_ctrl_tdata,
    input  wire [(`CTRL_REQ_TDATA_WIDTH)/8-1:0]  ctrl_req_from_ctrl_tkeep,
    input  wire                   ctrl_req_from_ctrl_tvalid,
    output wire                   ctrl_req_from_ctrl_tready,
    input  wire                   ctrl_req_from_ctrl_tlast,
    
    /*
    *CtrlOutput,Contain Context Data
    */
    output wire [DATA_WIDTH-1:0]  ctrl_rsp_to_ctrl_tdata,
    output wire [KEEP_WIDTH-1:0]  ctrl_rsp_to_ctrl_tkeep,
    output wire                   ctrl_rsp_to_ctrl_tvalid,
    input  wire                   ctrl_rsp_to_ctrl_tready,
    output wire                   ctrl_rsp_to_ctrl_tlast,
    output wire [3:0]  ctrl_rsp_to_ctrl_tid,
    
    
    /*
    *Context Data Input
    */
    input  wire [DATA_WIDTH-1:0]  recovery_context_from_ctrl_tdata,
    input  wire [KEEP_WIDTH-1:0]  recovery_context_from_ctrl_tkeep,
    input  wire                   recovery_context_from_ctrl_tvalid,
    output wire                   recovery_context_from_ctrl_tready,
    input  wire                   recovery_context_from_ctrl_tlast,
    
    
    
    input wire clk,
    input wire resetn,
    input wire [3:0] op_id,
    
    
    /*
    *Operator Control Signal
    */
    output wire ap_clk,
    output ap_rst_n,
    output ap_start,
    input ap_done,
    input ap_ready,
    input ap_idle,


    /*
    *Bram Data Control
    */
    //input  wire [$clog2(BRAM_DEPTH/(`BRAM_WIDTH/8))+1:0] bram_address_in,
    input  wire [$clog2(BRAM_DEPTH)+1:0] bram_address_in,
    input  wire bram_ce_in,
    input  wire [`BRAM_WIDTH/8-1:0] bram_we_in,
    input  wire [`BRAM_WIDTH-1:0] bram_d_in,
    output wire [`BRAM_WIDTH-1:0] bram_q_in,
    output wire [$clog2(BRAM_DEPTH)+1:0] bram_address_out,
    output wire bram_ce_out,
    output wire [`BRAM_WIDTH/8-1:0] bram_we_out,
    output wire [`BRAM_WIDTH-1:0] bram_d_out,
    input  wire [`BRAM_WIDTH-1:0] bram_q_out,


    /*
    * Done Stream Signal
    */
    input wire done_stream_tvalid,
    output wire done_stream_tready,
    input wire [7:0] done_stream_tdata//Invalid

    /*
    * Debug Signal
    */
    `ifdef OP_DEBUG
    ,
    output wire fsm_convert_valid,
    output wire [21:0] fsm_state_tracer,
    output wire [63:0] time_us_counter
    `endif 


    
    );
    wire [DATA_WIDTH-1:0]                   s_axis_inside_tdata_inner[FIFO_NUM-1:0];
    wire [KEEP_WIDTH-1:0]                   s_axis_inside_tkeep_inner[FIFO_NUM-1:0];
    wire                                    s_axis_inside_tvalid_inner[FIFO_NUM-1:0];
    wire                                    s_axis_inside_tready_inner[FIFO_NUM-1:0];
    wire                                    s_axis_inside_tlast_inner[FIFO_NUM-1:0];
    wire [USER_WIDTH-1:0]                   s_axis_inside_tuser_inner[FIFO_NUM-1:0];
    
    
    
    wire [DATA_WIDTH-1:0]                   s_axis_outside_tdata_inner[FIFO_NUM-1:0];
    wire [KEEP_WIDTH-1:0]                   s_axis_outside_tkeep_inner[FIFO_NUM-1:0];
    wire                                    s_axis_outside_tvalid_inner[FIFO_NUM-1:0];
    wire                                    s_axis_outside_tready_inner[FIFO_NUM-1:0];
    wire                                    s_axis_outside_tlast_inner[FIFO_NUM-1:0];
    wire [USER_WIDTH-1:0]                   s_axis_outside_tuser_inner[FIFO_NUM-1:0];

    /*
     * AXI output
     */
    wire [DATA_WIDTH-1:0]                   m_axis_inside_tdata_inner[FIFO_NUM-1:0];
    wire [KEEP_WIDTH-1:0]                   m_axis_inside_tkeep_inner[FIFO_NUM-1:0];
    wire                                    m_axis_inside_tvalid_inner[FIFO_NUM-1:0];
    wire                                    m_axis_inside_tready_inner[FIFO_NUM-1:0];
    wire                                    m_axis_inside_tlast_inner[FIFO_NUM-1:0];
    wire [DESTID_WIDTH-1:0]                 m_axis_inside_tdest_inner[FIFO_NUM-1:0];
    wire [DESTID_WIDTH-1:0]                 m_axis_inside_tid_inner[FIFO_NUM-1:0];
    wire [USER_WIDTH-1:0]                   m_axis_inside_tuser_inner[FIFO_NUM-1:0];
    
    
    
    wire [DATA_WIDTH-1:0]                   m_axis_outside_tdata_inner[FIFO_NUM-1:0];
    wire [KEEP_WIDTH-1:0]                   m_axis_outside_tkeep_inner[FIFO_NUM-1:0];
    wire                                    m_axis_outside_tvalid_inner[FIFO_NUM-1:0];
    wire                                    m_axis_outside_tready_inner[FIFO_NUM-1:0];
    wire                                    m_axis_outside_tlast_inner[FIFO_NUM-1:0];
    wire [DESTID_WIDTH-1:0]                 m_axis_outside_tdest_inner[FIFO_NUM-1:0];
    wire [USER_WIDTH-1:0]                   m_axis_outside_tuser_inner[FIFO_NUM-1:0];

    
    reg ap_clk_en;
    reg ap_start_inner;
    reg ap_rst_n_inner;
    
    localparam INIT                              = 21'b1,//1
               RECV_REQ_HEADER                   = 21'b10,//2
               RECV_APPLY_REQ_PAYLOAD            = 21'b100,//4
               WAIT_SUSPEND                      = 21'b1000,//8
               SEND_APPLY_RSP_SUCCESS_STAGE1     = 21'b10000,//10
               RECV_REQ_HEADER_APPLY_STAGE2      = 21'b100000,//20
               SEND_ADDRESS_LISTS                = 21'b1000000,//40
               SEND_RSP_HEADER                   = 21'b10000000,//80
               PREPARE_STATIC_VARS               = 21'b100000000,//100
               SEND_STATIC_VARS                  = 21'b1000000000,//200
               RECV_STATIC_VARS                  = 21'b10000000000,//400
               SEND_FIFO_DATA                    = 21'b100000000000,//800
               SEND_FIFO_DESCRIPTOR              = 21'b1000000000000,//1000
               GET_RECO_FIFO_DESCRIPTOR          = 21'b10000000000000,//2000
               UPDATE_RECO_FIFO_SEL              = 21'b100000000000000,//4000
               RECV_FIFO_DATA                    = 21'b1000000000000000,//8000
               ACTIVE_OP                         = 21'b10000000000000000,//10000
               CACULATE_FIFO_BUF_NUM             = 21'b100000000000000000,//20000
               UPDATE_SAVE_FIFO_SEL              = 21'b1000000000000000000,//40000
               UPDATE_CACUL_FIFO_SEL             = 21'b10000000000000000000,//80000
               SEND_DONE_RSP_HEADER              = 21'b100000000000000000000; //10000

    localparam IDLE = 1'b0,
               WORKING = 1'b1;

    localparam APPLY=3'b0,
               APPLY_STAGE2=3'b1,
               FORCE_FREE=3'b10,
               QUERY=3'b11,
               SUSPEND=3'b100,
               RESUME=3'b101;

    localparam ONE_PORT_TRANSFER_TIMES = DATA_WIDTH/`BRAM_WIDTH,
               STATIC_VAR_TRANSFER_TIMES = BRAM_DEPTH*8/DATA_WIDTH;
    reg [20:0] fsm_state;
    reg [20:0] next_fsm_state;
   
    /*Req Ctrl Signal*/
    wire [2:0] req_type_temp;
    reg [2:0] req_type;
    wire [39:0] context_address_temp;
    reg [39:0] curr_context_address;
    reg [39:0] old_context_address;
    wire [3:0] connections_from[FIFO_NUM-1:0];
    wire [7:0] connections_to[FIFO_NUM-1:0];
    reg [7:0]  fifo_dest[FIFO_NUM-1:0];

    /*Rsp Ctrl Signal*/
    wire [2:0]  rsp_state;
    wire [22:0] old_bbt;
    wire [22:0] new_bbt; 
    wire need_save;

    wire [19:0]  adder_A;
    wire [19:0]  adder_B;
    wire [19:0]  adder_Out;

    assign old_bbt = {3'd0,adder_Out};
    assign need_save = op_state==WORKING;
    assign new_bbt = 23'd2240;

    reg op_state;

    reg [DATA_WIDTH-1:0] data_buffer;
    reg [31:0] data_buffer1;
    reg [19:0] data_buffer2;


    wire [7:0] fifo_descriptor_length;
    wire [7:0] fifo_descriptor_id;
    wire [7:0] fifo_descriptor_sel_for_reco; 
    wire [7:0] fifo_descriptor_sel_for_save;
    wire [3:0] fifo_sel;
    wire [7:0] curr_recv_pkt_size;
    wire [$clog2(BRAM_DEPTH/(`BRAM_WIDTH/8))+1:0] bram_index;
    wire [19:0] unfinshed_task_counter;
    wire [$clog2(FIFO_DEPTH):0] fifo_depth[FIFO_NUM-1:0];
    wire [19:0] data_buf_count;

    `ifdef OP_DEBUG
    reg [63:0] cycle_counter_inner;
    reg [63:0] us_counter_inner;
    always @(posedge clk)begin
        if(~resetn||cycle_counter_inner==250)
            cycle_counter_inner <= 0;
        else 
            cycle_counter_inner <= cycle_counter_inner + 1;
    end
    always @(posedge clk)begin
        if(~resetn)
            us_counter_inner <= 0;
        else if(cycle_counter_inner == 250)
            us_counter_inner <= us_counter_inner+1;
    end
    assign time_us_counter = us_counter_inner;
    assign fsm_convert_valid = next_fsm_state != fsm_state;
    assign fsm_state_tracer = fsm_state;
    `endif 
  
    /*BRAM Port Ctrl*/
    assign bram_address_out = ({{($clog2(BRAM_DEPTH)+2){fsm_state == RECV_STATIC_VARS}}&
                              bram_index,6'b000000}) | ({($clog2(BRAM_DEPTH)+2){fsm_state == RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND}}&
                              bram_address_in) | {({($clog2(BRAM_DEPTH)+2){fsm_state == GET_RECO_FIFO_DESCRIPTOR||
                              fsm_state==RECV_FIFO_DATA||fsm_state==UPDATE_RECO_FIFO_SEL}}&
                              `DESCRIPTOR_START_ADDRESS),6'd0}|
                              {({($clog2(BRAM_DEPTH)+2){fsm_state == SEND_RSP_HEADER||
                              fsm_state == CACULATE_FIFO_BUF_NUM || fsm_state == UPDATE_CACUL_FIFO_SEL||
                              fsm_state == RECV_REQ_HEADER_APPLY_STAGE2}})&
                              `FIFO_BUF_START_ADDRESS,6'd0}|
                              {({($clog2(BRAM_DEPTH)+2){fsm_state == SEND_ADDRESS_LISTS}})&
                              `STATIC_VAR_START_ADDRESSS,6'd0}|
                              {({($clog2(BRAM_DEPTH)+2){fsm_state == SEND_STATIC_VARS}})&
                              adder_Out,6'd0};
    assign bram_ce_out = {{fsm_state == RECV_STATIC_VARS||fsm_state==GET_RECO_FIFO_DESCRIPTOR||fsm_state==RECV_FIFO_DATA||
                          fsm_state==UPDATE_RECO_FIFO_SEL||fsm_state==SEND_RSP_HEADER||fsm_state==UPDATE_CACUL_FIFO_SEL||
                          fsm_state == CACULATE_FIFO_BUF_NUM||fsm_state==SEND_STATIC_VARS||fsm_state==SEND_ADDRESS_LISTS}}&
                              1'b1 | {fsm_state == RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND}&
                              bram_ce_in;
    assign bram_we_out = {64{(fsm_state == RECV_STATIC_VARS)&recovery_context_from_ctrl_tvalid}}
                         |({64{fsm_state == RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND}}&
                              bram_we_in) | 64'b0;
    assign bram_q_in = bram_q_out;
    assign bram_d_out = {(`BRAM_WIDTH){fsm_state == RECV_STATIC_VARS}}&
                              recovery_context_from_ctrl_tdata | 
                              {(`BRAM_WIDTH){fsm_state == RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND}}&
                              bram_d_in;

    assign {curr_recv_pkt_size,
           fifo_descriptor_sel_for_reco
           } = data_buffer1[15:0];

    assign fifo_sel = data_buffer1[3:0];
    assign fifo_descriptor_sel_for_save = data_buffer1[11:4];
    assign data_buf_count = data_buffer[19:0];

    assign ap_clk = clk;

    assign unfinshed_task_counter = data_buffer1[19:0];

    assign bram_index = data_buffer2[$clog2(BRAM_DEPTH/(`BRAM_WIDTH/8))+1:0];

    assign fifo_descriptor_id = bram_q_out[fifo_descriptor_sel_for_reco*16+15-:8];
    assign fifo_descriptor_length = bram_q_out[fifo_descriptor_sel_for_reco*16+7-:8];
    
    assign adder_A = {20{fsm_state==WAIT_SUSPEND||
                        fsm_state==RECV_REQ_HEADER||
                        fsm_state==SEND_STATIC_VARS||
                        fsm_state==RECV_APPLY_REQ_PAYLOAD}}&data_buffer1[19:0]|
                     {20{fsm_state==RECV_FIFO_DATA}}&{12'b0,curr_recv_pkt_size}|
                     {20{fsm_state==UPDATE_RECO_FIFO_SEL}}&{12'b0,fifo_descriptor_sel_for_reco}|
                     {20{fsm_state==UPDATE_CACUL_FIFO_SEL}}&{16'b0,fifo_sel}|
                     {20{fsm_state==CACULATE_FIFO_BUF_NUM}}&{data_buf_count}|
                     {20{fsm_state==SEND_FIFO_DATA}}&{12'b0,data_buffer[16*fifo_descriptor_sel_for_save+7-:8]}|
                     {20{fsm_state==UPDATE_SAVE_FIFO_SEL}}&{data_buffer1[19:0]}|
                     {20{fsm_state==SEND_RSP_HEADER}}&{data_buffer[19:0]}|
                     {20{fsm_state==RECV_STATIC_VARS}}&{data_buffer2};
    assign adder_B =  {20{(fsm_state==RECV_REQ_HEADER||
                          fsm_state==WAIT_SUSPEND||
                          fsm_state==RECV_APPLY_REQ_PAYLOAD)}}&({20{(((!(ap_start&ap_ready))||fsm_state==WAIT_SUSPEND)&ap_done)}}
                          |({19'b0,(ap_start&ap_ready&!ap_done)||(ap_idle&!ap_done&(work_en))}))| 
                    {{19{1'b0}},(((fsm_state==RECV_STATIC_VARS||fsm_state==RECV_FIFO_DATA)&&
                        recovery_context_from_ctrl_tvalid&&
                        recovery_context_from_ctrl_tready)||(fsm_state == UPDATE_RECO_FIFO_SEL||
                        fsm_state==UPDATE_CACUL_FIFO_SEL)||
                        (fsm_state==SEND_STATIC_VARS&&ctrl_rsp_to_ctrl_tready&&
                        ctrl_rsp_to_ctrl_tvalid)||(fsm_state==SEND_FIFO_DATA))}|
                        {20{fsm_state==CACULATE_FIFO_BUF_NUM}}&{fifo_depth[fifo_sel]}|
                        {20{fsm_state==UPDATE_SAVE_FIFO_SEL}}&{15'b0,1'b1,3'b0,fifo_depth[fifo_sel]==0}|
                        ({20{fsm_state==SEND_RSP_HEADER}}&(BRAM_STATIC_VAR_DEPTH+64)); //这里此后可能需要更新
    assign adder_Out = adder_A + adder_B;
   
    always @(posedge clk or negedge resetn) begin
        if(~resetn)
        begin
            op_state <= IDLE;
        end
        else if(fsm_state==RECV_FIFO_DATA)
        begin
            op_state <= WORKING; 
        end
        else if(fsm_state==RECV_REQ_HEADER&&
                ctrl_req_from_ctrl_tvalid&&
                req_type_temp==FORCE_FREE)
        begin
            op_state <= IDLE;
        end
    end

    always @(posedge clk or negedge resetn) begin
        if(~resetn)
        begin
            fsm_state <= INIT;
        end
        else begin
            fsm_state <= next_fsm_state; 
        end
    end

    always @(posedge clk)begin
        case (fsm_state)
            RECV_REQ_HEADER:begin
                data_buffer <= 0;
                data_buffer1 <= {12'b0,adder_Out};
                data_buffer2 <= 0;
            end
            RECV_APPLY_REQ_PAYLOAD:begin
                data_buffer <= 0;
                data_buffer1 <= {12'b0,adder_Out};
                data_buffer2 <= 0;
            end
            WAIT_SUSPEND:begin
                data_buffer <= 0;
                data_buffer1 <= {1'b1,11'b0,adder_Out};//用第一位给SEND_DONE_RSP判断上一个状态是RECV_REQ_HEADER还是WAIT_SUSPEND
                data_buffer2 <= 0;
            end
            SEND_DONE_RSP_HEADER:begin
                data_buffer <= 0;
                data_buffer1 <= {data_buffer1[31]&!done_stream_tvalid,data_buffer1[30:0]};
                data_buffer2 <= 0;
            end
            ACTIVE_OP: begin
                data_buffer <= 0;
                data_buffer1 <= 0;
                data_buffer2 <= 0;
            end
            SEND_RSP_HEADER: begin
                data_buffer <= {512{{op_state==WORKING&req_type==0}}}&{data_buffer};;
                data_buffer1 <= 0;
                data_buffer2 <= 0;
            end
            RECV_STATIC_VARS:begin
                data_buffer <= 0;
                data_buffer1 <= 0;
                data_buffer2 <= adder_Out;
            end
            GET_RECO_FIFO_DESCRIPTOR:begin
                data_buffer1 <= 0;
                data_buffer <= bram_q_out;
                data_buffer2 <= 0;
            end
            RECV_FIFO_DATA:begin
                data_buffer <= bram_q_out;
                data_buffer1 <= {16'b0,adder_Out[7:0],data_buffer1[7:0]};
                data_buffer2 <= 0;
            end
            UPDATE_RECO_FIFO_SEL:begin
                data_buffer <= data_buffer;
                data_buffer1 <= {24'b0,adder_Out[7:0]};
                data_buffer2 <= 0;
            end
            INIT:begin
                data_buffer <= 0;
                data_buffer1 <= 0;
                data_buffer2 <= 0;
            end 
            CACULATE_FIFO_BUF_NUM:begin
                data_buffer  <= {data_buffer[511:64],44'b0,adder_Out};
                data_buffer1 <= data_buffer1;
                data_buffer2 <= 0;
            end
            UPDATE_CACUL_FIFO_SEL:begin
                data_buffer  <= {bram_q_out[511:64],data_buffer[63:0]};
                data_buffer1 <= {12'b0,adder_Out};
                data_buffer2 <= 0;
            end
            UPDATE_SAVE_FIFO_SEL:begin
                data_buffer[511-:64] <= (data_buffer2[USER_WIDTH-1-:4]!=0)?data_buffer[511-:64]:{60'b0,adder_Out[7:4]};
                data_buffer1 <= {16'b0,data_buffer1[15:8],adder_Out[7:0]};
                data_buffer2 <= 0;
            end
            SEND_FIFO_DATA:begin
                data_buffer[16*fifo_descriptor_sel_for_save+15-:16] <= ({16{ctrl_rsp_to_ctrl_tready&&
                                                                    ctrl_rsp_to_ctrl_tvalid}}
                                                                    &{4'b0,fifo_sel,adder_Out[7:0]})|
                                                                    ({16{!(ctrl_rsp_to_ctrl_tready&&
                                                                    ctrl_rsp_to_ctrl_tvalid)}}&
                                                                    data_buffer[16*fifo_descriptor_sel_for_save+15-:16]);
                data_buffer1 <= data_buffer1;
                data_buffer2 <= ctrl_rsp_to_ctrl_tvalid?{12'b0,m_axis_inside_tuser_inner[fifo_sel]}:0;
            end
            SEND_ADDRESS_LISTS:begin
                data_buffer <= data_buffer;
                data_buffer1 <= `STATIC_VAR_START_ADDRESSS;
                data_buffer2 <= 0;
            end
            SEND_STATIC_VARS:begin
                data_buffer <= 0;
                data_buffer1 <= {12'b0,adder_Out};
                data_buffer2 <= 0;
            end
            RECV_REQ_HEADER_APPLY_STAGE2:begin
                data_buffer <= 0;
                data_buffer1 <= 0;
                data_buffer2 <= 0;
            end
        endcase
    end

    wire work_en;
    assign work_en = ((ap_rst_n_inner&&~((req_type_temp==FORCE_FREE)&&ctrl_req_from_ctrl_tvalid)));

    always @(posedge clk)begin
        case (fsm_state)
            ACTIVE_OP:begin
                ap_clk_en <= 1'b1;
                ap_rst_n_inner <= 1'b1;
                ap_start_inner <= 1'b0;
            end
            WAIT_SUSPEND:begin
                ap_clk_en <= adder_Out!=0;
                ap_start_inner <= ap_start_inner&!ap_ready;
                ap_rst_n_inner <= adder_Out!=0;
            end
            RECV_REQ_HEADER:begin
                ap_start_inner <= op_state==WORKING&&work_en;                                
                ap_clk_en <= work_en;
                ap_rst_n_inner <= work_en;
            end
            INIT:begin
                ap_clk_en <= 1'b0;
                ap_start_inner <= 1'b0;
                ap_rst_n_inner <= 1'b0;
            end
        endcase
    end

    assign recovery_context_from_ctrl_tready = fsm_state==RECV_STATIC_VARS||
                                               (fsm_state==RECV_FIFO_DATA&&s_axis_outside_tready_inner[
                                                fifo_descriptor_id
                                               ])||fsm_state==RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND||
                                               fsm_state==SEND_APPLY_RSP_SUCCESS_STAGE1;//有时候fifo传输完还会有一些冗余数据
                                               //需要在这些阶段将其消化掉

    assign rsp_state = {fsm_state==SEND_DONE_RSP_HEADER,fsm_state==SEND_APPLY_RSP_SUCCESS_STAGE1,
                        fsm_state==SEND_APPLY_RSP_SUCCESS_STAGE1||fsm_state==SEND_RSP_HEADER};

    assign done_stream_tready = (fsm_state==RECV_REQ_HEADER||fsm_state==WAIT_SUSPEND);

    assign ctrl_req_from_ctrl_tready = (fsm_state==RECV_REQ_HEADER&&!(done_stream_tvalid)||
                                        fsm_state==RECV_REQ_HEADER_APPLY_STAGE2||
                                        fsm_state==RECV_APPLY_REQ_PAYLOAD);
    assign ctrl_rsp_to_ctrl_tvalid = (fsm_state==SEND_APPLY_RSP_SUCCESS_STAGE1||
                                     fsm_state==SEND_RSP_HEADER||
                                     fsm_state==SEND_STATIC_VARS||
                                     fsm_state==SEND_ADDRESS_LISTS||
                                     fsm_state==SEND_DONE_RSP_HEADER||
                                     fsm_state==SEND_FIFO_DESCRIPTOR)||(
                                        fsm_state==SEND_FIFO_DATA&&
                                        m_axis_inside_tvalid_inner[fifo_sel]
                                     );
    wire [63:0] bbt_static_var_size;
    assign bbt_static_var_size = BRAM_STATIC_VAR_DEPTH;
    assign ctrl_rsp_to_ctrl_tdata = ({DATA_WIDTH{fsm_state==SEND_APPLY_RSP_SUCCESS_STAGE1||
                                    fsm_state==SEND_RSP_HEADER||fsm_state==SEND_DONE_RSP_HEADER}}&
                                    {13'b0,rsp_state,128'b0,bbt_static_var_size,bbt_static_var_size,24'b0,curr_context_address,
                                    24'b0,old_context_address,9'b0,new_bbt,
                                    9'b0,old_bbt,7'b0,need_save,40'b0})|
                                    ({DATA_WIDTH{fsm_state==SEND_FIFO_DESCRIPTOR}}&
                                    data_buffer)|({DATA_WIDTH{fsm_state==SEND_STATIC_VARS}}&
                                    bram_q_out)|({DATA_WIDTH{fsm_state==SEND_FIFO_DATA}}&
                                    m_axis_inside_tdata_inner[fifo_sel])|
                                    ({DATA_WIDTH{fsm_state==SEND_ADDRESS_LISTS}}&{data_buffer[511:64],(64'b0|(
                                    data_buffer[63:$clog2(MEM_PAGE_SIZE)]+(data_buffer[$clog2(MEM_PAGE_SIZE)-1:0]!=0)))});
    assign ctrl_rsp_to_ctrl_tkeep = 64'hffffffffffffffff;
    assign ctrl_rsp_to_ctrl_tid = op_id;
    assign ctrl_rsp_to_ctrl_tlast = (~((req_type==APPLY)&&(op_state==WORKING)&&(fsm_state!=SEND_FIFO_DESCRIPTOR)))||
                                    (fsm_state == SEND_APPLY_RSP_SUCCESS_STAGE1);

    always @(posedge clk or negedge resetn)begin
        if(~resetn)begin
            req_type <= 0;
        end
        else if(fsm_state == RECV_REQ_HEADER&&ctrl_req_from_ctrl_tvalid)
        begin
            req_type <= req_type_temp;
        end
    end

    assign req_type_temp = ctrl_req_from_ctrl_tdata[2:0];
    assign context_address_temp = ctrl_req_from_ctrl_tdata[(`CTRL_REQ_TDATA_WIDTH-25)-:40];

    /*上下文地址更新，当收到APPLY信号时更新上下文地址*/
    always @(posedge clk or negedge resetn)begin
        if(~resetn)begin
            old_context_address <= 0;
            curr_context_address <= 0;
        end
        else if(fsm_state == RECV_REQ_HEADER&&
                ctrl_req_from_ctrl_tvalid&&
                req_type_temp==0)
        begin
            old_context_address <= curr_context_address;
            curr_context_address <= context_address_temp;
        end
    end
    
    always @(*) begin
        case (fsm_state)
            INIT:
                next_fsm_state = RECV_REQ_HEADER;
            RECV_REQ_HEADER:
                if(done_stream_tvalid)
                    next_fsm_state = SEND_DONE_RSP_HEADER;
                else if(ctrl_req_from_ctrl_tvalid) 
                    next_fsm_state = {21{req_type_temp==APPLY}}&RECV_APPLY_REQ_PAYLOAD|
                                     {21{req_type_temp==SUSPEND}}&WAIT_SUSPEND|
                                     {21{req_type_temp==RESUME||req_type_temp==FORCE_FREE}}&SEND_RSP_HEADER;
                else
                    next_fsm_state = RECV_REQ_HEADER;
            SEND_DONE_RSP_HEADER:
                if(ctrl_rsp_to_ctrl_tvalid)
                    next_fsm_state = ({21{!data_buffer1[31]}}&RECV_REQ_HEADER)|({21{data_buffer1[31]}}&(
                        req_type==APPLY?SEND_APPLY_RSP_SUCCESS_STAGE1:SEND_RSP_HEADER
                    ));
                else
                    next_fsm_state = SEND_DONE_RSP_HEADER;
            WAIT_SUSPEND:
                if(adder_Out==0)
                    next_fsm_state = ({21{done_stream_tvalid}}&(SEND_DONE_RSP_HEADER))|({21{(!done_stream_tvalid)}}&(
                    req_type==APPLY?SEND_APPLY_RSP_SUCCESS_STAGE1:SEND_RSP_HEADER));
                else
                    next_fsm_state = WAIT_SUSPEND;
            RECV_APPLY_REQ_PAYLOAD:
                if(ctrl_req_from_ctrl_tvalid)
                    next_fsm_state = WAIT_SUSPEND;
                else
                    next_fsm_state = RECV_APPLY_REQ_PAYLOAD;
            SEND_APPLY_RSP_SUCCESS_STAGE1:
                if(ctrl_rsp_to_ctrl_tready)
                    next_fsm_state = RECV_REQ_HEADER_APPLY_STAGE2;
                else
                    next_fsm_state = SEND_APPLY_RSP_SUCCESS_STAGE1;
            RECV_REQ_HEADER_APPLY_STAGE2:
                if(ctrl_req_from_ctrl_tvalid)
                    next_fsm_state = op_state==WORKING&req_type==0?CACULATE_FIFO_BUF_NUM:
                                   SEND_RSP_HEADER;
                else
                    next_fsm_state = RECV_REQ_HEADER_APPLY_STAGE2;
            SEND_RSP_HEADER:
                if(ctrl_rsp_to_ctrl_tready)
                    next_fsm_state = ({21{op_state==WORKING&req_type==0}}&SEND_ADDRESS_LISTS)|
                    ({21{op_state==IDLE&req_type==0}}&RECV_STATIC_VARS)|
                    ({21{req_type!=0&&req_type!=RESUME}}&RECV_REQ_HEADER)|
                    ({21{req_type==RESUME}}&ACTIVE_OP);
                else
                    next_fsm_state = SEND_RSP_HEADER;
            RECV_STATIC_VARS:
                if(adder_Out==BRAM_DEPTH/(`BRAM_WIDTH/8))
                    next_fsm_state = {21{op_state==WORKING&req_type==0}}&SEND_FIFO_DATA| 
                                     {21{op_state==IDLE}}&GET_RECO_FIFO_DESCRIPTOR;
                else
                    next_fsm_state = RECV_STATIC_VARS;
            GET_RECO_FIFO_DESCRIPTOR:
                next_fsm_state = RECV_FIFO_DATA;
            UPDATE_RECO_FIFO_SEL:
                if(adder_Out==data_buffer[467:448])
                    next_fsm_state = ACTIVE_OP;
                else
                    next_fsm_state = RECV_FIFO_DATA;
            RECV_FIFO_DATA:
                if(bram_q_out[467:448]==0)
                    next_fsm_state = ACTIVE_OP;
                else if(adder_Out[7:0]==fifo_descriptor_length)
                    next_fsm_state = UPDATE_RECO_FIFO_SEL;
                else
                    next_fsm_state = RECV_FIFO_DATA;
            ACTIVE_OP:
                next_fsm_state = RECV_REQ_HEADER;
            CACULATE_FIFO_BUF_NUM:  //加载BUF地址和数目的信息之后，不停计算当前FIFO的深度
                next_fsm_state = UPDATE_CACUL_FIFO_SEL;
            UPDATE_CACUL_FIFO_SEL:
                if(adder_Out[3:0]==FIFO_NUM)
                    next_fsm_state = SEND_RSP_HEADER;
                else
                    next_fsm_state = CACULATE_FIFO_BUF_NUM;
            SEND_ADDRESS_LISTS:     //计算完成之后，发送BUF的地址列表，然后刷新buf，设置fifo descriptor
                if(ctrl_rsp_to_ctrl_tready)
                    next_fsm_state = SEND_STATIC_VARS;
                else
                    next_fsm_state = SEND_ADDRESS_LISTS;
            SEND_STATIC_VARS:
                if(ctrl_rsp_to_ctrl_tready&adder_Out==STATIC_VAR_TRANSFER_TIMES)
                    next_fsm_state = RECV_STATIC_VARS;
                else
                    next_fsm_state = SEND_STATIC_VARS;
            SEND_FIFO_DATA:
                if((m_axis_inside_tlast_inner[fifo_sel]&ctrl_rsp_to_ctrl_tready)||
                    fifo_depth[fifo_sel]==0)
                    next_fsm_state = UPDATE_SAVE_FIFO_SEL;
                else
                    next_fsm_state = SEND_FIFO_DATA;
            UPDATE_SAVE_FIFO_SEL:
                if(adder_Out[3:0]==FIFO_NUM)
                    next_fsm_state = SEND_FIFO_DESCRIPTOR;
                else
                    next_fsm_state = SEND_FIFO_DATA;
            SEND_FIFO_DESCRIPTOR:
                if(ctrl_rsp_to_ctrl_tready)
                    next_fsm_state = GET_RECO_FIFO_DESCRIPTOR;
                else
                    next_fsm_state = SEND_FIFO_DESCRIPTOR;
            default: 
                next_fsm_state = INIT;
        endcase
    end

    assign ap_start = ap_start_inner & !(fsm_state==WAIT_SUSPEND&ap_ready);
    assign ap_rst_n = ap_rst_n_inner;

    genvar i;
    
    generate 
        for (i = 0; i < FIFO_NUM ; i = i + 1)
        begin
            axis_fifo #(
            .DEPTH(FIFO_DEPTH),
            .DATA_WIDTH(DATA_WIDTH),
            .ID_ENABLE(0),
            .DEST_ENABLE(0),
            .USER_ENABLE(1),
            .LAST_ENABLE(1),
            .USER_WIDTH(8)) fifo(
                .clk(clk),
                .rst(!resetn),
                .s_axis_tdata(s_axis_outside_tdata_inner[i]),
                .s_axis_tvalid(s_axis_outside_tvalid_inner[i]),
                .s_axis_tready(s_axis_outside_tready_inner[i]),
                .s_axis_tlast(s_axis_outside_tlast_inner[i]),
                .s_axis_tkeep({KEEP_WIDTH{1'b1}}),
                .s_axis_tuser(s_axis_outside_tuser_inner[i]),
                .m_axis_tdata(m_axis_inside_tdata_inner[i]),
                .m_axis_tvalid(m_axis_inside_tvalid_inner[i]),
                .m_axis_tready(m_axis_inside_tready_inner[i]),
                .m_axis_tlast(m_axis_inside_tlast_inner[i]),
                .m_axis_tkeep(),
                .m_axis_tuser(m_axis_inside_tuser_inner[i]),
                .status_depth(fifo_depth[i])
            );

            assign m_axis_outside_tdata_inner[i] = s_axis_inside_tdata_inner[i];
            assign m_axis_outside_tkeep_inner[i] = {KEEP_WIDTH{1'b1}};
            assign m_axis_outside_tlast_inner[i] = s_axis_inside_tlast_inner[i];
            assign m_axis_outside_tvalid_inner[i] = s_axis_inside_tvalid_inner[i];
            assign m_axis_outside_tuser_inner[i] = s_axis_inside_tuser_inner[i];
            assign s_axis_inside_tready_inner[i] = m_axis_outside_tready_inner[i];

            assign m_axis_inside_tkeep_inner[i] = {KEEP_WIDTH{1'b1}};
            
            assign m_axis_outside_tdest_inner[i] = fifo_dest[i];
            assign m_axis_outside_tdest[(i+1)*DESTID_WIDTH-1-:DESTID_WIDTH] = m_axis_outside_tdest_inner[i];

            assign m_axis_outside_tdata[(i+1)*DATA_WIDTH-1-:DATA_WIDTH] = m_axis_outside_tdata_inner[i];
            assign m_axis_outside_tkeep[(i+1)*KEEP_WIDTH-1-:KEEP_WIDTH] = m_axis_outside_tkeep_inner[i];
            assign m_axis_outside_tuser[(i+1)*USER_WIDTH-1-:USER_WIDTH] = m_axis_outside_tuser_inner[i];
            assign m_axis_outside_tlast[i] = m_axis_outside_tlast_inner[i];
            assign m_axis_outside_tvalid[i] = m_axis_outside_tvalid_inner[i];
            assign m_axis_outside_tready_inner[i] = m_axis_outside_tready[i];

            assign m_axis_inside_tdata[(i+1)*DATA_WIDTH-1-:DATA_WIDTH] = m_axis_inside_tdata_inner[i];
            assign m_axis_inside_tkeep[(i+1)*KEEP_WIDTH-1-:KEEP_WIDTH] = m_axis_inside_tkeep_inner[i];
            assign m_axis_inside_tuser[(i+1)*USER_WIDTH-1-:USER_WIDTH] = m_axis_inside_tuser_inner[i];
            assign m_axis_inside_tlast[i] = m_axis_inside_tlast_inner[i];
            assign m_axis_inside_tvalid[i] = m_axis_inside_tvalid_inner[i]&(fsm_state!=SEND_FIFO_DATA);
            assign m_axis_inside_tdest_inner[i] = 0;
            assign m_axis_inside_tid_inner[i] = 0;
            assign m_axis_inside_tdest[(i+1)*DESTID_WIDTH-1-:DESTID_WIDTH] = m_axis_inside_tdest_inner[i];
            assign m_axis_inside_tid[(i+1)*DESTID_WIDTH-1-:DESTID_WIDTH] = m_axis_inside_tid_inner[i];

            assign m_axis_inside_tready_inner[i] = ((fsm_state==SEND_FIFO_DATA)&ctrl_rsp_to_ctrl_tready)|
                                                    ((fsm_state!=SEND_FIFO_DATA)&m_axis_inside_tready[i]&ap_start);

            assign s_axis_inside_tdata_inner[i] = s_axis_inside_tdata[(i+1)*DATA_WIDTH-1-:DATA_WIDTH];
            assign s_axis_inside_tkeep_inner[i] = s_axis_inside_tkeep[(i+1)*KEEP_WIDTH-1-:KEEP_WIDTH];
            assign s_axis_inside_tlast_inner[i] = s_axis_inside_tlast[i];
            assign s_axis_inside_tvalid_inner[i] = s_axis_inside_tvalid[i];
            assign s_axis_inside_tuser_inner[i] = s_axis_inside_tuser[(i+1)*USER_WIDTH-1-:USER_WIDTH];
            assign s_axis_inside_tready[i] = s_axis_inside_tready_inner[i];

            assign s_axis_outside_tdata_inner[i] = fsm_state==RECV_FIFO_DATA?recovery_context_from_ctrl_tdata:
                                                s_axis_outside_tdata[(i+1)*DATA_WIDTH-1-:DATA_WIDTH];
            assign s_axis_outside_tkeep_inner[i] = s_axis_outside_tkeep[(i+1)*KEEP_WIDTH-1-:KEEP_WIDTH];//没有使用
            assign s_axis_outside_tlast_inner[i] = fsm_state==RECV_FIFO_DATA?adder_Out[7:0]==fifo_descriptor_length:
                                                    s_axis_outside_tlast[i];
            assign s_axis_outside_tvalid_inner[i] = (fsm_state==RECV_FIFO_DATA&recovery_context_from_ctrl_tvalid)|
                                                (((fsm_state==RECV_REQ_HEADER|fsm_state==WAIT_SUSPEND)&(op_state==WORKING))&
                                                s_axis_outside_tvalid[i]);
            assign s_axis_outside_tready[i] = s_axis_outside_tready_inner[i]&(((fsm_state==RECV_REQ_HEADER|
                                              fsm_state==WAIT_SUSPEND)&(op_state==WORKING)));
            assign s_axis_outside_tuser_inner[i] = fsm_state==RECV_FIFO_DATA?0:s_axis_outside_tuser[(i+1)*USER_WIDTH-1-:USER_WIDTH];


            assign connections_from[i] = ctrl_req_from_ctrl_tdata[(i*16+7)-:4];
            assign connections_to[i] = ctrl_req_from_ctrl_tdata[(i*16+15)-:8];

        
            always @(posedge clk)begin
                if(fsm_state==RECV_APPLY_REQ_PAYLOAD)
                    fifo_dest[i] = connections_to[i];
            end
        end

       
    endgenerate

    /*
    BUFGCE BUFGCE_inst(
        .O(ap_clk),
        .CE(ap_clk_en),
        .I(clk)
    );*/
endmodule
