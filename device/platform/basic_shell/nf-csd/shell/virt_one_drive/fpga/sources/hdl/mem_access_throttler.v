
//当一个用户试图访问内存，他需要先发送命令，然后再收到/发出数据，这个模块保证不同用户的命令和数据能够正确绑定
//也就是说，用户1先发出命令，那么用户1可以接收数据，用户2不能成功发出命令或者接收/发出数据
//当用户1将命令接收完成后，用户2才可以发送命令，当用户2发送命令，用户1不能成功发出命令或者接收/发出数据

`define CMD_TDATA_WIDTH  (MEM_TYPE == 0? 8'd80 : 8'd161)
`define CMD_TUSER_WIDTH  (MEM_TYPE == 0? 8'd0  : 8'd32)
    
module two_user_wr_mem_access_throttler 
#(
    parameter DATA_TDATA_WIDTH = 10'd512,
    parameter DATA_TID_WIDTH = 10'd0,
    parameter DATA_TUSER_WIDTH = 10'd1,
    parameter MEM_TYPE = 1'b0 //0 ps_mem 1 host_mem
    
)
(
    
    s_axis_cmd_channel1_tdata,
    s_axis_cmd_channel1_tuser,
    s_axis_cmd_channel1_tvalid,
    s_axis_cmd_channel1_tready,

    m_axis_cmd_channel_tdata,
    m_axis_cmd_channel_tuser,
    m_axis_cmd_channel_tvalid,
    m_axis_cmd_channel_tready,

    m_axis_data_channel1_tdata,
    m_axis_data_channel1_tkeep,
    m_axis_data_channel1_tstrb,
    m_axis_data_channel1_tid,
    m_axis_data_channel1_tuser,
    m_axis_data_channel1_tvalid,
    m_axis_data_channel1_tlast,
    m_axis_data_channel1_tready,

    s_axis_cmd_channel2_tdata,
    s_axis_cmd_channel2_tuser,
    s_axis_cmd_channel2_tvalid,
    s_axis_cmd_channel2_tready,

    m_axis_data_channel2_tdata,
    m_axis_data_channel2_tkeep,
    m_axis_data_channel2_tstrb,
    m_axis_data_channel2_tid,
    m_axis_data_channel2_tuser,
    m_axis_data_channel2_tvalid,
    m_axis_data_channel2_tlast,
    m_axis_data_channel2_tready,

    s_axis_data_channel_tdata,
    s_axis_data_channel_tkeep,
    s_axis_data_channel_tstrb,
    s_axis_data_channel_tid,
    s_axis_data_channel_tuser,
    s_axis_data_channel_tvalid,
    s_axis_data_channel_tlast,
    s_axis_data_channel_tready,

    clk,
    user_resetn

);
    
    
    input [`CMD_TDATA_WIDTH - 1:0] s_axis_cmd_channel1_tdata;
    input [`CMD_TUSER_WIDTH - 1:0] s_axis_cmd_channel1_tuser;
    input s_axis_cmd_channel1_tvalid;
    output s_axis_cmd_channel1_tready;

    output [`CMD_TDATA_WIDTH - 1:0] m_axis_cmd_channel_tdata;
    output [`CMD_TUSER_WIDTH - 1:0] m_axis_cmd_channel_tuser;
    output m_axis_cmd_channel_tvalid;
    input m_axis_cmd_channel_tready;

    input [DATA_TDATA_WIDTH - 1:0] m_axis_data_channel1_tdata;
    input [DATA_TDATA_WIDTH /8 - 1:0] m_axis_data_channel1_tkeep;
    input [DATA_TDATA_WIDTH/8 - 1:0] m_axis_data_channel1_tstrb;
    input [DATA_TID_WIDTH - 1:0] m_axis_data_channel1_tid;
    input [DATA_TUSER_WIDTH-1:0] m_axis_data_channel1_tuser;
    input m_axis_data_channel1_tvalid;
    input m_axis_data_channel1_tlast;
    output m_axis_data_channel1_tready;

    input [`CMD_TDATA_WIDTH - 1:0] s_axis_cmd_channel2_tdata;
    input [`CMD_TUSER_WIDTH - 1:0] s_axis_cmd_channel2_tuser;
    input s_axis_cmd_channel2_tvalid;
    output s_axis_cmd_channel2_tready;

    input [DATA_TDATA_WIDTH - 1:0] m_axis_data_channel2_tdata;
    input [DATA_TDATA_WIDTH /8-1:0] m_axis_data_channel2_tkeep;
    input [DATA_TDATA_WIDTH /8-1:0] m_axis_data_channel2_tstrb;
    input [DATA_TID_WIDTH - 1:0] m_axis_data_channel2_tid;
    input [DATA_TUSER_WIDTH - 1:0] m_axis_data_channel2_tuser;
    input m_axis_data_channel2_tvalid;
    input m_axis_data_channel2_tlast;
    output m_axis_data_channel2_tready;

    output [DATA_TDATA_WIDTH -1:0] s_axis_data_channel_tdata;
    output [DATA_TDATA_WIDTH /8-1:0] s_axis_data_channel_tkeep;
    output [DATA_TDATA_WIDTH /8-1:0] s_axis_data_channel_tstrb;
    output [DATA_TID_WIDTH -1:0] s_axis_data_channel_tid;
    output [DATA_TUSER_WIDTH -1:0] s_axis_data_channel_tuser;
    output s_axis_data_channel_tvalid;
    output s_axis_data_channel_tlast;
    input s_axis_data_channel_tready;

    input clk;
    input user_resetn;
    localparam WAIT_CMD = 2'b10, RECV_DATA = 2'b01;

    reg [1:0] fsm_state;
    reg valid_user;
    reg [23:0] data_recvd_count;
    reg [23:0] total_data_count;

    always @(posedge clk) begin
        if(~user_resetn)
            valid_user <= 1'b0;
        else if(fsm_state == WAIT_CMD) begin
            if(s_axis_cmd_channel1_tvalid)
                valid_user <= 1'b0;
            else 
                valid_user <= 1'b1;
        end
    end

    always @(posedge clk) begin
        if(~user_resetn)
            data_recvd_count <= 0;
        else if(fsm_state == WAIT_CMD)
            data_recvd_count <= 0;
        else if(fsm_state == RECV_DATA && 
        ((m_axis_data_channel1_tready && m_axis_data_channel1_tvalid && m_axis_data_channel1_tlast) 
        ||  (m_axis_data_channel2_tready && m_axis_data_channel2_tvalid && m_axis_data_channel2_tlast)))
            data_recvd_count <= data_recvd_count + 1;
    end

    always @(posedge clk) begin
        if(~user_resetn)
            fsm_state <= WAIT_CMD;
        else if(fsm_state == WAIT_CMD) begin
            if((s_axis_cmd_channel1_tvalid && s_axis_cmd_channel1_tready) || (s_axis_cmd_channel2_tvalid && s_axis_cmd_channel2_tready))
                fsm_state <= RECV_DATA;
        end
        else if(fsm_state == RECV_DATA && data_recvd_count == total_data_count)begin
            fsm_state <= WAIT_CMD;
        end
    end

    wire [22:0] count_bits;
    if(MEM_TYPE == 1'b0)
        assign count_bits = m_axis_cmd_channel_tdata[22:12];
    else 
        assign count_bits = {2'b0,m_axis_cmd_channel_tdata[159:152]}; //Page?
    

    always @(posedge clk) begin
        if(~user_resetn)
            total_data_count <= 0;
        else if(s_axis_cmd_channel1_tvalid)
            total_data_count <= count_bits;
        else if(s_axis_cmd_channel2_tvalid)
            total_data_count <= count_bits;
    end


    assign m_axis_cmd_channel_tvalid = s_axis_cmd_channel1_tvalid || s_axis_cmd_channel2_tvalid;
    assign s_axis_cmd_channel1_tready = (fsm_state == WAIT_CMD) && m_axis_cmd_channel_tready;
    assign s_axis_cmd_channel2_tready = (fsm_state == WAIT_CMD) && (!s_axis_cmd_channel1_tvalid) && m_axis_cmd_channel_tready;
    assign m_axis_cmd_channel_tdata = (s_axis_cmd_channel1_tvalid) ? s_axis_cmd_channel1_tdata : s_axis_cmd_channel2_tvalid ? s_axis_cmd_channel2_tdata : 0;
    assign m_axis_cmd_channel_tuser = (s_axis_cmd_channel1_tvalid) ? s_axis_cmd_channel1_tuser : s_axis_cmd_channel2_tvalid  ? s_axis_cmd_channel2_tuser : 0;
    

    assign s_axis_data_channel_tvalid = (fsm_state == RECV_DATA) && (m_axis_data_channel1_tvalid || m_axis_data_channel2_tvalid);
    assign m_axis_data_channel1_tready = (fsm_state == RECV_DATA) && (valid_user == 1'b0) && s_axis_data_channel_tready;
    assign m_axis_data_channel2_tready = (fsm_state == RECV_DATA) && (valid_user == 1'b1) && s_axis_data_channel_tready;
    assign s_axis_data_channel_tdata = (valid_user == 1'b0) ? m_axis_data_channel1_tdata : m_axis_data_channel2_tdata;
    assign s_axis_data_channel_tuser = (valid_user == 1'b0) ? m_axis_data_channel1_tuser : m_axis_data_channel2_tuser;
    assign s_axis_data_channel_tid = (valid_user == 1'b0) ? m_axis_data_channel1_tid : m_axis_data_channel2_tid;
    assign s_axis_data_channel_tkeep = (valid_user == 1'b0)  ? m_axis_data_channel1_tkeep : m_axis_data_channel2_tkeep;
    assign s_axis_data_channel_tstrb = (valid_user == 1'b0) ?  m_axis_data_channel1_tstrb : m_axis_data_channel2_tstrb;
    assign s_axis_data_channel_tlast = (valid_user == 1'b0) ?  m_axis_data_channel1_tlast : m_axis_data_channel2_tlast;
    
endmodule


module two_user_rd_mem_access_throttler 
#(
    parameter DATA_TDATA_WIDTH = 10'd512,
    parameter DATA_TID_WIDTH = 10'd0,
    parameter DATA_TUSER_WIDTH = 10'd1,
    parameter MEM_TYPE = 1'b0 //0 ps_mem 1 host_mem
    
)
(
    
    s_axis_cmd_channel1_tdata,
    s_axis_cmd_channel1_tuser,
    s_axis_cmd_channel1_tvalid,
    s_axis_cmd_channel1_tready,

    m_axis_cmd_channel_tdata,
    m_axis_cmd_channel_tuser,
    m_axis_cmd_channel_tvalid,
    m_axis_cmd_channel_tready,

    m_axis_data_channel1_tdata,
    m_axis_data_channel1_tkeep,
    m_axis_data_channel1_tstrb,
    m_axis_data_channel1_tid,
    m_axis_data_channel1_tuser,
    m_axis_data_channel1_tvalid,
    m_axis_data_channel1_tlast,
    m_axis_data_channel1_tready,

    s_axis_cmd_channel2_tdata,
    s_axis_cmd_channel2_tuser,
    s_axis_cmd_channel2_tvalid,
    s_axis_cmd_channel2_tready,

    m_axis_data_channel2_tdata,
    m_axis_data_channel2_tkeep,
    m_axis_data_channel2_tstrb,
    m_axis_data_channel2_tid,
    m_axis_data_channel2_tuser,
    m_axis_data_channel2_tvalid,
    m_axis_data_channel2_tlast,
    m_axis_data_channel2_tready,

    s_axis_data_channel_tdata,
    s_axis_data_channel_tkeep,
    s_axis_data_channel_tstrb,
    s_axis_data_channel_tid,
    s_axis_data_channel_tuser,
    s_axis_data_channel_tvalid,
    s_axis_data_channel_tlast,
    s_axis_data_channel_tready,

    clk,
    user_resetn

);
    
    
    input [`CMD_TDATA_WIDTH - 1:0] s_axis_cmd_channel1_tdata;
    input [`CMD_TUSER_WIDTH - 1:0] s_axis_cmd_channel1_tuser;
    input s_axis_cmd_channel1_tvalid;
    output s_axis_cmd_channel1_tready;

    output [`CMD_TDATA_WIDTH - 1:0] m_axis_cmd_channel_tdata;
    output [`CMD_TUSER_WIDTH - 1:0] m_axis_cmd_channel_tuser;
    output m_axis_cmd_channel_tvalid;
    input m_axis_cmd_channel_tready;

    output [DATA_TDATA_WIDTH - 1:0] m_axis_data_channel1_tdata;
    output [DATA_TDATA_WIDTH /8 - 1:0] m_axis_data_channel1_tkeep;
    output [DATA_TDATA_WIDTH/8 - 1:0] m_axis_data_channel1_tstrb;
    output [DATA_TID_WIDTH - 1:0] m_axis_data_channel1_tid;
    output [DATA_TUSER_WIDTH-1:0] m_axis_data_channel1_tuser;
    output m_axis_data_channel1_tvalid;
    output m_axis_data_channel1_tlast;
    input m_axis_data_channel1_tready;

    input [`CMD_TDATA_WIDTH - 1:0] s_axis_cmd_channel2_tdata;
    input [`CMD_TUSER_WIDTH - 1:0] s_axis_cmd_channel2_tuser;
    input s_axis_cmd_channel2_tvalid;
    output s_axis_cmd_channel2_tready;

    output [DATA_TDATA_WIDTH - 1:0] m_axis_data_channel2_tdata;
    output [DATA_TDATA_WIDTH /8-1:0] m_axis_data_channel2_tkeep;
    output [DATA_TDATA_WIDTH /8-1:0] m_axis_data_channel2_tstrb;
    output [DATA_TID_WIDTH - 1:0] m_axis_data_channel2_tid;
    output [DATA_TUSER_WIDTH - 1:0] m_axis_data_channel2_tuser;
    output m_axis_data_channel2_tvalid;
    output m_axis_data_channel2_tlast;
    input m_axis_data_channel2_tready;

    input [DATA_TDATA_WIDTH -1:0] s_axis_data_channel_tdata;
    input [DATA_TDATA_WIDTH /8-1:0] s_axis_data_channel_tkeep;
    input [DATA_TDATA_WIDTH /8-1:0] s_axis_data_channel_tstrb;
    input [DATA_TID_WIDTH -1:0] s_axis_data_channel_tid;
    input [DATA_TUSER_WIDTH -1:0] s_axis_data_channel_tuser;
    input s_axis_data_channel_tvalid;
    input s_axis_data_channel_tlast;
    output s_axis_data_channel_tready;

    input clk;
    input user_resetn;
    localparam WAIT_CMD = 2'b10, SEND_DATA = 2'b01;

    reg [1:0] fsm_state;
    reg valid_user;
    reg [22:0] data_send_count;
    reg [22:0] total_data_count;

    always @(posedge clk) begin
        if(~user_resetn)
            valid_user <= 1'b0;
        else if(fsm_state == WAIT_CMD) begin
            if(s_axis_cmd_channel1_tvalid)
                valid_user <= 1'b0;
            else 
                valid_user <= 1'b1;
        end
    end

    always @(posedge clk) begin
        if(~user_resetn)
            data_send_count <= 0;
        else if(fsm_state == WAIT_CMD)
            data_send_count <= 0;
        else if((fsm_state == SEND_DATA) && 
        ((m_axis_data_channel1_tready && m_axis_data_channel1_tvalid && m_axis_data_channel1_tlast) 
        ||  (m_axis_data_channel2_tready && m_axis_data_channel2_tvalid && m_axis_data_channel2_tlast)))
           data_send_count <= data_send_count + 1;
    end

    always @(posedge clk) begin
        if(~user_resetn)
            fsm_state <= WAIT_CMD;
        else if(fsm_state == WAIT_CMD) begin
            if((s_axis_cmd_channel1_tvalid && s_axis_cmd_channel1_tready) || (s_axis_cmd_channel2_tvalid && s_axis_cmd_channel2_tready))
                fsm_state <= SEND_DATA;
        end
        else if(fsm_state == SEND_DATA && data_send_count == total_data_count)begin
            fsm_state <= WAIT_CMD;
        end
    end

    wire [22:0] count_bits;
    if(MEM_TYPE == 1'b0)
        assign count_bits = m_axis_cmd_channel_tdata[22:12];
    else 
        assign count_bits = {2'b0,m_axis_cmd_channel_tdata[159:152]};
    

    always @(posedge clk) begin
        if(~user_resetn)
            total_data_count <= 0;
        else if(s_axis_cmd_channel1_tvalid)
            total_data_count <= count_bits;
        else if(s_axis_cmd_channel2_tvalid)
            total_data_count <= count_bits;
    end

   

    assign m_axis_cmd_channel_tvalid = s_axis_cmd_channel1_tvalid || s_axis_cmd_channel2_tvalid;
    assign s_axis_cmd_channel1_tready = (fsm_state == WAIT_CMD) && m_axis_cmd_channel_tready;
    assign s_axis_cmd_channel2_tready = (fsm_state == WAIT_CMD) && (!s_axis_cmd_channel1_tvalid) && m_axis_cmd_channel_tready;
    assign m_axis_cmd_channel_tdata = (s_axis_cmd_channel1_tvalid) ? s_axis_cmd_channel1_tdata : s_axis_cmd_channel2_tvalid ? s_axis_cmd_channel2_tdata : 0;
    assign m_axis_cmd_channel_tuser = (s_axis_cmd_channel1_tvalid) ? s_axis_cmd_channel1_tuser : s_axis_cmd_channel2_tvalid  ? s_axis_cmd_channel2_tuser : 0;
    
    assign m_axis_data_channel1_tdata = s_axis_data_channel_tdata;
    assign m_axis_data_channel2_tdata = s_axis_data_channel_tdata;
    assign m_axis_data_channel1_tid = s_axis_data_channel_tid;
    assign m_axis_data_channel2_tid = s_axis_data_channel_tid;
    assign m_axis_data_channel1_tkeep = s_axis_data_channel_tkeep;
    assign m_axis_data_channel2_tkeep = s_axis_data_channel_tkeep;
    assign m_axis_data_channel1_tstrb = s_axis_data_channel_tstrb;
    assign m_axis_data_channel2_tstrb = s_axis_data_channel_tstrb;
    assign m_axis_data_channel1_tuser = s_axis_data_channel_tuser;
    assign m_axis_data_channel2_tuser = s_axis_data_channel_tuser;
    assign m_axis_data_channel1_tlast = s_axis_data_channel_tlast;
    assign m_axis_data_channel2_tlast = s_axis_data_channel_tlast;
    
    assign m_axis_data_channel1_tvalid = s_axis_data_channel_tvalid && (valid_user == 1'b0) && (fsm_state == SEND_DATA);
    assign m_axis_data_channel2_tvalid = s_axis_data_channel_tvalid && (valid_user == 1'b1) && (fsm_state == SEND_DATA);

    assign s_axis_data_channel_tready = (fsm_state ==  SEND_DATA) && ((valid_user == 1'b0) && m_axis_data_channel1_tready) || ((valid_user == 1'b1) && m_axis_data_channel2_tready);

endmodule