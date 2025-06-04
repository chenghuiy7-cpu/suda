module qdma_ep_axis_wrapper(
    axi_aclk,
    axi_aresetn,
    
    /* From QDMA H2C port */
    s_axis_h2c_0_err,
    s_axis_h2c_0_mdata,
    s_axis_h2c_0_mty,
    s_axis_h2c_0_port_id,
    s_axis_h2c_0_qid,
    s_axis_h2c_0_tcrc,
    s_axis_h2c_0_tdata,
    s_axis_h2c_0_tlast,
    s_axis_h2c_0_tready,
    s_axis_h2c_0_tvalid,
    s_axis_h2c_0_zero_byte,

    /* To QDMA C2H port */
    m_axis_c2h_0_ctrl_has_cmpt,
    m_axis_c2h_0_ctrl_len,
    m_axis_c2h_0_ctrl_marker,
    m_axis_c2h_0_ctrl_port_id,
    m_axis_c2h_0_ctrl_qid,
    m_axis_c2h_0_ecc,
    m_axis_c2h_0_mty,
    m_axis_c2h_0_tcrc,
    m_axis_c2h_0_tdata,
    m_axis_c2h_0_tlast,
    m_axis_c2h_0_tready,
    m_axis_c2h_0_tvalid,

    /* To QDMA CMPT port */
    m_axis_c2h_cmpt_tdata,
    m_axis_c2h_cmpt_size,
    m_axis_c2h_cmpt_dpar,
    m_axis_c2h_cmpt_ctrl_qid,
    m_axis_c2h_cmpt_ctrl_marker,
    m_axis_c2h_cmpt_ctrl_user_trig,
    m_axis_c2h_cmpt_ctrl_cmpt_type,
    m_axis_c2h_cmpt_ctrl_wait_pld_pkt_id,
    m_axis_c2h_cmpt_ctrl_port_id,
    m_axis_c2h_cmpt_ctrl_col_idx,
    m_axis_c2h_cmpt_ctrl_err_idx,
    m_axis_c2h_cmpt_ctrl_no_wrb_marker,
    m_axis_c2h_cmpt_tvalid,
    m_axis_c2h_cmpt_tready,

    /* To AXIS-IC Slave Port */
    m_axis_h2c_tdest,
    m_axis_h2c_tdata,
    m_axis_h2c_tlast,
    m_axis_h2c_tready,
    m_axis_h2c_tvalid,
    m_axis_h2c_tkeep,
    m_axis_h2c_tid,
    m_axis_h2c_tuser,

    /* From AXIS-IC Master Port */
    s_axis_c2h_tid,
    s_axis_c2h_tdest,
    s_axis_c2h_tdata,
    s_axis_c2h_tlast,
    s_axis_c2h_tready,
    s_axis_c2h_tvalid,
    s_axis_c2h_tuser,
    s_axis_c2h_tkeep,

    cmpt_fifo_wr_en,
    cmpt_fifo_din,
    cmpt_fifo_rd_en,
    cmpt_fifo_dout,
    cmpt_fifo_empty,
    cmpt_fifo_full,

    h2c_is_data_queue,

    c2h_pkt_cnt_regs
);
  input         axi_aclk;
  input         axi_aresetn;

  input         s_axis_h2c_0_err;
  input [31:0]  s_axis_h2c_0_mdata;
  input [5:0]   s_axis_h2c_0_mty;
  input [2:0]   s_axis_h2c_0_port_id;
  input [10:0]  s_axis_h2c_0_qid;
  input [31:0]  s_axis_h2c_0_tcrc;
  input [511:0] s_axis_h2c_0_tdata;
  input         s_axis_h2c_0_tlast;
  output        s_axis_h2c_0_tready;
  input         s_axis_h2c_0_tvalid;
  input         s_axis_h2c_0_zero_byte;

  output        m_axis_c2h_0_ctrl_has_cmpt;
  output [15:0] m_axis_c2h_0_ctrl_len;
  output        m_axis_c2h_0_ctrl_marker;
  output [2:0]  m_axis_c2h_0_ctrl_port_id;
  output [10:0] m_axis_c2h_0_ctrl_qid;
  output [6:0]  m_axis_c2h_0_ecc;
  output [5:0]  m_axis_c2h_0_mty;
  output [31:0] m_axis_c2h_0_tcrc;
  output [511:0]m_axis_c2h_0_tdata;
  output        m_axis_c2h_0_tlast;
  input         m_axis_c2h_0_tready;
  output        m_axis_c2h_0_tvalid;

  output [511:0]m_axis_c2h_cmpt_tdata;
  output [1:0]  m_axis_c2h_cmpt_size;
  output [15:0] m_axis_c2h_cmpt_dpar;
  output [10:0] m_axis_c2h_cmpt_ctrl_qid;
  output        m_axis_c2h_cmpt_ctrl_marker;
  output        m_axis_c2h_cmpt_ctrl_user_trig;
  output [1:0]  m_axis_c2h_cmpt_ctrl_cmpt_type;
  output [15:0] m_axis_c2h_cmpt_ctrl_wait_pld_pkt_id;
  output [2:0]  m_axis_c2h_cmpt_ctrl_port_id;
  output [2:0]  m_axis_c2h_cmpt_ctrl_col_idx;
  output [2:0]  m_axis_c2h_cmpt_ctrl_err_idx;
  output        m_axis_c2h_cmpt_ctrl_no_wrb_marker;
  output        m_axis_c2h_cmpt_tvalid;
  input         m_axis_c2h_cmpt_tready;

  output [7:0]  m_axis_h2c_tdest;
  output [511:0]m_axis_h2c_tdata;
  output        m_axis_h2c_tlast;
  input         m_axis_h2c_tready;
  output        m_axis_h2c_tvalid;
  output [63:0] m_axis_h2c_tkeep;
  output [ 7:0] m_axis_h2c_tid;
  output [ 7:0] m_axis_h2c_tuser;

  input [3:0]   s_axis_c2h_tdest;
  input [511:0] s_axis_c2h_tdata;
  input         s_axis_c2h_tlast;
  output        s_axis_c2h_tready;
  input         s_axis_c2h_tvalid;
  input [63:0]  s_axis_c2h_tkeep;
  input [7:0]   s_axis_c2h_tid;
  input [63:0]  s_axis_c2h_tuser;

  output        h2c_is_data_queue;
  output [31:0] c2h_pkt_cnt_regs;

  output          cmpt_fifo_wr_en;
  output [22:0]   cmpt_fifo_din;
  output          cmpt_fifo_rd_en;
  input [22:0]    cmpt_fifo_dout;
  input           cmpt_fifo_empty;
  input           cmpt_fifo_full;

  reg [15:0]    pkt_pld_id;
  reg [15:0]    prev_pkt_pld_id;

  reg [11:0]     c2h_pkt_cnt[7:0];
  reg           next_c2h_pkt_is_new;

  wire [15:0]   cmpt_len;
  wire [6:0]    cmpt_qid;
  wire [2:0]    c2h_data_qid;

  wire end_of_packet;
  wire is_admin_queue;
  wire c2h_is_data_queue;
  wire c2h_data_should_send;
  wire cmpt_is_data_queue;
  wire cmpt_data_q_should_send_irq;

  always @ (posedge axi_aclk)
  begin
    if (~axi_aresetn)
      pkt_pld_id <= 1;
    else if (m_axis_c2h_cmpt_tready & m_axis_c2h_cmpt_tvalid)
      pkt_pld_id <= pkt_pld_id + 1;
  end

  always @ (posedge axi_aclk)
  begin
    if (~axi_aresetn) begin
      c2h_pkt_cnt[0] <= 0; c2h_pkt_cnt[1] <= 0; c2h_pkt_cnt[2] <= 0; c2h_pkt_cnt[3] <= 0;
      c2h_pkt_cnt[4] <= 0; c2h_pkt_cnt[5] <= 0; c2h_pkt_cnt[6] <= 0; c2h_pkt_cnt[7] <= 0;
    end else if (end_of_packet & c2h_is_data_queue) begin
      c2h_pkt_cnt[c2h_data_qid] <= c2h_pkt_cnt[c2h_data_qid] + 1;
    end
  end

  always @ (posedge axi_aclk)
  begin
    if (~axi_aresetn) begin
      next_c2h_pkt_is_new <= 1'b1;
    end else if (m_axis_c2h_0_tready & m_axis_c2h_0_tvalid) begin
      next_c2h_pkt_is_new <= m_axis_c2h_0_tlast;
    end
  end

  assign end_of_packet = m_axis_c2h_0_tvalid & m_axis_c2h_0_tready & m_axis_c2h_0_tlast;
  assign is_admin_queue = (m_axis_c2h_0_ctrl_qid[4:0] == 5'd0);
  assign h2c_is_data_queue = (s_axis_h2c_0_qid[4:0] >= 5'd9) && (s_axis_h2c_0_qid[4:0] <= 5'd16);
  assign c2h_is_data_queue = (s_axis_c2h_tid[4:0] >= 5'd9) && (s_axis_c2h_tid[4:0] <= 5'd16);
  assign c2h_data_qid = s_axis_c2h_tid[2:0] - 1;
  assign cmpt_data_q_should_send_irq = c2h_pkt_cnt[c2h_data_qid] == 0;

  assign m_axis_h2c_tdest  = {3'b0, s_axis_h2c_0_qid[4:0]};
  assign m_axis_h2c_tdata  = s_axis_h2c_0_tdata;
  assign m_axis_h2c_tlast  = s_axis_h2c_0_tlast;
  assign m_axis_h2c_tvalid = s_axis_h2c_0_tvalid;
  assign m_axis_h2c_tkeep  = (64'hFFFFFFFFFFFFFFFF >> s_axis_h2c_0_mty);
  assign m_axis_h2c_tid    = s_axis_h2c_0_qid[7:0];
  assign s_axis_h2c_0_tready = m_axis_h2c_tready;
  assign m_axis_h2c_tuser[7:5] = 3'b0;
  assign m_axis_h2c_tuser[4:3] = s_axis_h2c_0_port_id[1:0]; // Drive ID
  assign m_axis_h2c_tuser[2:0] = s_axis_h2c_0_qid[2:0] - 1; // ARID

  assign m_axis_c2h_0_ctrl_qid = {3'd0, s_axis_c2h_tid};
  assign m_axis_c2h_0_tdata  = s_axis_c2h_tdata;
  assign m_axis_c2h_0_tlast  = s_axis_c2h_tlast;
  assign m_axis_c2h_0_tvalid = s_axis_c2h_tvalid & c2h_data_should_send;
  assign s_axis_c2h_tready = m_axis_c2h_0_tready & c2h_data_should_send;
  assign m_axis_c2h_0_ecc           = 7'd0;
  assign m_axis_c2h_0_tcrc          = 32'd0;
  assign m_axis_c2h_0_ctrl_has_cmpt = 1'b1;
  assign m_axis_c2h_0_ctrl_len = (is_admin_queue ? 16'd4160 : (c2h_is_data_queue ? s_axis_c2h_tuser[15:0] : 16'd16));
  assign m_axis_c2h_0_mty = ~m_axis_c2h_0_ctrl_len[5:0] + 1;
  assign m_axis_c2h_0_ctrl_marker   = 1'b0;
  assign m_axis_c2h_0_ctrl_port_id  = 3'd0;

  assign c2h_data_should_send = ~(next_c2h_pkt_is_new & cmpt_fifo_full);

  assign cmpt_fifo_wr_en = end_of_packet;
  assign cmpt_fifo_din[15:0] = m_axis_c2h_0_ctrl_len;
  assign cmpt_fifo_din[22:16] = m_axis_c2h_0_ctrl_qid[6:0];
  assign cmpt_fifo_rd_en = m_axis_c2h_cmpt_tready & m_axis_c2h_cmpt_tvalid;
  assign cmpt_len = cmpt_fifo_dout[15:0];
  assign cmpt_qid = cmpt_fifo_dout[22:16];

  assign cmpt_is_data_queue = (cmpt_qid[4:0] >= 5'd9) && (cmpt_qid[4:0] <= 5'd16);

  assign m_axis_c2h_cmpt_tdata = {480'b0, cmpt_len, 16'b0};
  assign m_axis_c2h_cmpt_size = 2'b0;
  assign m_axis_c2h_cmpt_dpar[0] = ^m_axis_c2h_cmpt_tdata[31:0];
  assign m_axis_c2h_cmpt_dpar[15:1] = 15'h7FFF;
  assign m_axis_c2h_cmpt_ctrl_qid = {4'b0, cmpt_qid};
  assign m_axis_c2h_cmpt_ctrl_marker = 1'b0;
  assign m_axis_c2h_cmpt_ctrl_user_trig = cmpt_is_data_queue & cmpt_data_q_should_send_irq;
  assign m_axis_c2h_cmpt_ctrl_cmpt_type = 2'b11; // HAS_PLD
  assign m_axis_c2h_cmpt_ctrl_wait_pld_pkt_id = pkt_pld_id;
  assign m_axis_c2h_cmpt_tvalid = ~cmpt_fifo_empty;

  assign c2h_pkt_cnt_regs[6:0] = c2h_pkt_cnt[0];
  assign c2h_pkt_cnt_regs[14:8] = c2h_pkt_cnt[1];
  assign c2h_pkt_cnt_regs[22:16] = c2h_pkt_cnt[2];
  assign c2h_pkt_cnt_regs[30:24] = c2h_pkt_cnt[3];

endmodule
