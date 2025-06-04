module axis_tdest_width_converter (
    // From AXI-Stream input IC
	input [95:0]    s_axis_tdata,
	input           s_axis_tlast,
	input [ 7:0]    s_axis_tdest,
	input [ 7:0]    s_axis_tid,
	input [11:0]    s_axis_tkeep,
	input           s_axis_tvalid,
	output          s_axis_tready,

    // To AXI-Stream output IC
	output [95:0]   m_axis_tdata,
	output          m_axis_tlast,
	output [ 7:0]   m_axis_tdest,
	output [ 7:0]   m_axis_tid,
	output [11:0]   m_axis_tkeep,
	output          m_axis_tvalid,
	input           m_axis_tready
);

    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tdest = s_axis_tdest;
    assign m_axis_tid = 8'h1F;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

endmodule