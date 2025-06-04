module op_incr_4 (
    input aclk,
    input aresetn,
    // AXI Stream input interface
    input [511:0] s_axis_tdata,
    input [63:0] s_axis_tkeep,
    input [63:0] s_axis_tuser,
    input s_axis_tlast,
    input s_axis_tvalid,
    output s_axis_tready,
    // AXI Stream output interface
    output [511:0] m_axis_tdata,
    output [63:0] m_axis_tkeep,
    output [63:0] m_axis_tuser,
    output m_axis_tlast,
    output m_axis_tvalid,
    input m_axis_tready
);

    // Passthrough the slave signal to master, but increment every data byte by 1
    genvar i;
    generate
        for (i = 0; i < 512; i = i + 8) begin : gen_increment_byte
            assign m_axis_tdata[i+7:i] = s_axis_tdata[i+7:i] + 8'd4;
        end
    endgenerate
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

endmodule