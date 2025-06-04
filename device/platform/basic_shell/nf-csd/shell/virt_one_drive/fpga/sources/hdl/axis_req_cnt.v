module axis_req_cnt(
  input aclk,
  input aresetn,
  // From W data
  input [95:0]    s_axis_tdata,
  input           s_axis_tlast,
  input           s_axis_tvalid,
  input [11:0]    s_axis_tkeep,
  input [ 7:0]    s_axis_tdest,
  output          s_axis_tready,

  // To C2H data AXIS IC
  output [95:0]    m_axis_tdata,
  output           m_axis_tlast,
  output           m_axis_tvalid,
  output [11:0]    m_axis_tkeep,
  output [ 2:0]    m_axis_tdest,
  input            m_axis_tready,

  output reg [31:0] req_pkt_cnt
);

  always @ (posedge aclk)
  begin
    if (~aresetn)
      req_pkt_cnt <= 32'b0;
    else if (s_axis_tlast & s_axis_tvalid & s_axis_tready)
      req_pkt_cnt <= req_pkt_cnt + 32'b1;
  end

  assign m_axis_tdata = {s_axis_tdata[95:78], req_pkt_cnt[2:0], s_axis_tdata[74:0]};
  assign m_axis_tlast = s_axis_tlast;
  assign m_axis_tvalid = s_axis_tvalid;
  assign s_axis_tready = m_axis_tready;
  assign m_axis_tkeep = s_axis_tkeep;
  assign m_axis_tdest = req_pkt_cnt[2:0];

endmodule