module axis_route_drive_handler(
  input          aclk         ,
  input          aresetn      ,
  
  input  [  7:0] s_axis_tid   ,
  input  [  7:0] s_axis_tdest ,
  input  [511:0] s_axis_tdata ,
  input  [ 63:0] s_axis_tkeep ,
  input          s_axis_tlast ,
  input  [ 63:0] s_axis_tuser ,
  input          s_axis_tvalid,
  output         s_axis_tready,

  output [  7:0] m_axis_tid   ,
  output [  7:0] m_axis_tdest ,
  output [511:0] m_axis_tdata ,
  output [ 63:0] m_axis_tkeep ,
  output         m_axis_tlast ,
  output [ 63:0] m_axis_tuser ,
  output         m_axis_tvalid,
  input          m_axis_tready,

  output reg [31:0] r_pkt_cnt,
  output reg [31:0] r_cycle_cnt
);

  assign m_axis_tid    = s_axis_tid   ;
  assign m_axis_tdest  = s_axis_tid   ;
  assign m_axis_tdata  = s_axis_tdata ;
  assign m_axis_tkeep  = s_axis_tkeep ;
  assign m_axis_tlast  = s_axis_tlast ;
  assign m_axis_tuser  = s_axis_tuser ;
  assign m_axis_tvalid = s_axis_tvalid;
  assign s_axis_tready = m_axis_tready;

endmodule