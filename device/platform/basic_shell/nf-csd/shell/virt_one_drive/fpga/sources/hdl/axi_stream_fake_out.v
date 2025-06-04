module axi_stream_fake_out (
    
    // AXI Stream输出接口
    output wire [511:0] m_axis_tdata,
    output wire [7:0] m_axis_tdest,
    output wire [3:0] m_axis_tid,
    output wire [63:0] m_axis_tkeep,
    output wire m_axis_tlast,
    output wire m_axis_tvalid,
    output wire [7:0] m_axis_tuser,
    input wire m_axis_tready
);

    // 所有输出信号持续拉低
    assign m_axis_tdata = 512'd0;
    assign m_axis_tdest = 8'd0;
    assign m_axis_tid = 4'd0;
    assign m_axis_tkeep = 64'd0;
    assign m_axis_tlast = 1'b0;
    assign m_axis_tvalid = 1'b0;
    assign m_axis_tuser = 8'd0;

endmodule
