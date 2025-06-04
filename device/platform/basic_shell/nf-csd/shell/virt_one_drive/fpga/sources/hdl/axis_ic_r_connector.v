module axis_ic_r_connector (
  input clk,
  input rstn,

  // From AXI-Stream input IC
  input [  7:0]    s_axis_ic_tid,
	input [511:0]    s_axis_ic_tdata,
	input            s_axis_ic_tlast,
	input [  7:0]    s_axis_ic_tdest,
	input            s_axis_ic_tvalid,
  input [ 63:0]    s_axis_ic_tkeep,
  input [ 63:0]    s_axis_ic_tuser,
	output           s_axis_ic_tready,

  // To AXI-Stream output IC
  output [  7:0]   m_aixs_ic_tid,
	output [511:0]   m_axis_ic_tdata,
	output           m_axis_ic_tlast,
	output [  7:0]   m_axis_ic_tdest,
	output           m_axis_ic_tvalid,
  output [ 63:0]   m_axis_ic_tkeep,
  output [ 63:0]   m_axis_ic_tuser,
	input            m_axis_ic_tready,

  output          tvalid,
  input           r_gating,
  output          tready,
  output          tlast
);

  assign m_aixs_ic_tid = s_axis_ic_tid;
  assign m_axis_ic_tdata = s_axis_ic_tdata;
  assign m_axis_ic_tlast = s_axis_ic_tlast;
  assign m_axis_ic_tdest = s_axis_ic_tid;
  assign m_axis_ic_tkeep = s_axis_ic_tkeep;
  assign m_axis_ic_tuser = s_axis_ic_tuser;
  assign m_axis_ic_tvalid = s_axis_ic_tvalid & ~r_gating;
  assign s_axis_ic_tready = m_axis_ic_tready & ~r_gating;

  assign tvalid = s_axis_ic_tvalid;
  assign tready = s_axis_ic_tready;
  assign tlast = s_axis_ic_tlast;

endmodule