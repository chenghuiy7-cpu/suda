module compute_op_input (
    input aclk,
    input aresetn,

    // AXI Stream input interface
    input [511:0] s_axis_tdata,
    input [63:0] s_axis_tkeep,
    input [63:0] s_axis_tuser,
    input [3:0] s_axis_tdest,
    input s_axis_tlast,
    input s_axis_tvalid,
    output s_axis_tready,
    // AXI Stream output interface
    output [511:0] m_axis_tdata,
    output [63:0] m_axis_tkeep,
    output [63:0] m_axis_tuser,
    output [3:0] m_axis_tdest,
    output m_axis_tlast,
    output m_axis_tvalid,
    input m_axis_tready,

    output reg [31:0] count
);

    always @(posedge aclk) begin
        if (~aresetn) begin
            count <= 0;
        end else if (s_axis_tvalid & s_axis_tready) begin
            count <= count + 1;
        end
    end

    assign m_axis_tdest = s_axis_tdest;
    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

endmodule