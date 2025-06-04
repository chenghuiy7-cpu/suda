
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 03/06/2025 10:19:53 AM
// Design Name: 
// Module Name: axi_stream_router
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


module axi_stream_router (
    // 输入接口
    input wire [511:0] s_axis_tdata,
    input wire [7:0] s_axis_tdest,
    input wire [7:0] s_axis_tuser,
    input wire [63:0] s_axis_tkeep,
    input wire s_axis_tlast,
    input wire s_axis_tvalid,
    output wire s_axis_tready,
    
    // 输出接口
    output wire [511:0] m_axis_tdata,
    output wire [7:0] m_axis_tdest,
    output wire [7:0] m_axis_tuser,
    output wire [63:0] m_axis_tkeep,
    output wire m_axis_tlast,
    output wire m_axis_tvalid,
    input wire m_axis_tready,
    
    // 外部ready信号输入
    input wire ready_in_0,
    input wire ready_in_1,
    input wire ready_in_2,
    input wire ready_in_3,
    input wire ready_in_4,
    input wire ready_in_5,
    input wire ready_in_special
);

    // 从tdest提取序列号
    wire [3:0] sequence_number = s_axis_tdest[7:4];
    wire [3:0] tdest_lower = s_axis_tdest[3:0];
    
    // 特殊情况的检测：当sequence_number为0，但tdest_lower不为0时
    wire special_case = (sequence_number == 4'd0) && (tdest_lower != 4'd0);
    
    // 使用与或逻辑实现选择
    wire seq_is_0 = (sequence_number == 4'd0);
    wire seq_is_1 = (sequence_number == 4'd1);
    wire seq_is_2 = (sequence_number == 4'd2);
    wire seq_is_3 = (sequence_number == 4'd3);
    wire seq_is_4 = (sequence_number == 4'd4);
    wire seq_is_5 = (sequence_number == 4'd5);
    
    wire ready_selected = 
        (special_case & ready_in_special) | 
        (seq_is_0 & ~special_case & ready_in_0) |
        (seq_is_1 & ready_in_1) |
        (seq_is_2 & ready_in_2) |
        (seq_is_3 & ready_in_3) |
        (seq_is_4 & ready_in_4) |
        (seq_is_5 & ready_in_5);
    
    // 输入接口的tready等于序列号对应的ready_in_{序列号}
    assign s_axis_tready = ready_selected;
    
    // 输出接口的tvalid等于输入接口的tvalid与上输入接口的tready
    assign m_axis_tvalid = s_axis_tvalid & s_axis_tready;
    
    // 透传所有其他信号
    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tdest = s_axis_tdest;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tlast = s_axis_tlast;

endmodule
