module axis_tid_as_tdest_512bits (
    // From AXI-Stream input IC
	input [511:0]   s_axis_tdata,
	input           s_axis_tlast,
	input [ 7:0]    s_axis_tdest,
	input [ 7:0]    s_axis_tid,
	input [63:0]    s_axis_tkeep,
	input [63:0]    s_axis_tuser,
	input           s_axis_tvalid,
	output          s_axis_tready,

    // To AXI-Stream output IC
	output [511:0]  m_axis_tdata,
	output          m_axis_tlast,
	output [ 7:0]   m_axis_tdest,
	output [ 7:0]   m_axis_tid,
	output [63:0]   m_axis_tkeep,
	output [63:0]   m_axis_tuser,
	output          m_axis_tvalid,
	input           m_axis_tready
);

    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tdest = s_axis_tid;
    assign m_axis_tid = s_axis_tid;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

endmodule