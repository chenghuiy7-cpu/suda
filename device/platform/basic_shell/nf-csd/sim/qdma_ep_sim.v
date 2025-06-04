//Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2020.2_AR75986 (lin64) Build 3064766 Wed Nov 18 09:12:47 MST 2020
//Date        : Tue Nov  8 01:55:12 2022
//Host        : 888dd5ba7f59 running 64-bit Ubuntu 22.04.1 LTS
//Command     : generate_target pcie_ep_wrapper.bd
//Design      : pcie_ep_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module qdma_ep_sim
   (M_AXI_BRIDGE_0_araddr,
    M_AXI_BRIDGE_0_arburst,
    M_AXI_BRIDGE_0_arcache,
    M_AXI_BRIDGE_0_arid,
    M_AXI_BRIDGE_0_arlen,
    M_AXI_BRIDGE_0_arlock,
    M_AXI_BRIDGE_0_arprot,
    M_AXI_BRIDGE_0_arready,
    M_AXI_BRIDGE_0_arsize,
    M_AXI_BRIDGE_0_aruser,
    M_AXI_BRIDGE_0_arvalid,
    M_AXI_BRIDGE_0_awaddr,
    M_AXI_BRIDGE_0_awburst,
    M_AXI_BRIDGE_0_awcache,
    M_AXI_BRIDGE_0_awid,
    M_AXI_BRIDGE_0_awlen,
    M_AXI_BRIDGE_0_awlock,
    M_AXI_BRIDGE_0_awprot,
    M_AXI_BRIDGE_0_awready,
    M_AXI_BRIDGE_0_awsize,
    M_AXI_BRIDGE_0_awuser,
    M_AXI_BRIDGE_0_awvalid,
    M_AXI_BRIDGE_0_bid,
    M_AXI_BRIDGE_0_bready,
    M_AXI_BRIDGE_0_bresp,
    M_AXI_BRIDGE_0_bvalid,
    M_AXI_BRIDGE_0_rdata,
    M_AXI_BRIDGE_0_rid,
    M_AXI_BRIDGE_0_rlast,
    M_AXI_BRIDGE_0_rready,
    M_AXI_BRIDGE_0_rresp,
    M_AXI_BRIDGE_0_rvalid,
    M_AXI_BRIDGE_0_wdata,
    M_AXI_BRIDGE_0_wlast,
    M_AXI_BRIDGE_0_wready,
    M_AXI_BRIDGE_0_wstrb,
    M_AXI_BRIDGE_0_wvalid,
    axi_aclk,
    axi_aresetn,
    m_axis_h2c_0_err,
    m_axis_h2c_0_mdata,
    m_axis_h2c_0_mty,
    m_axis_h2c_0_port_id,
    m_axis_h2c_0_qid,
    m_axis_h2c_0_tcrc,
    m_axis_h2c_0_tdata,
    m_axis_h2c_0_tlast,
    m_axis_h2c_0_tready,
    m_axis_h2c_0_tvalid,
    m_axis_h2c_0_zero_byte,
    pcie_ep_rxn,
    pcie_ep_rxp,
    pcie_ep_txn,
    pcie_ep_txp,
    s_axis_c2h_0_ctrl_has_cmpt,
    s_axis_c2h_0_ctrl_len,
    s_axis_c2h_0_ctrl_marker,
    s_axis_c2h_0_ctrl_port_id,
    s_axis_c2h_0_ctrl_qid,
    s_axis_c2h_0_ecc,
    s_axis_c2h_0_mty,
    s_axis_c2h_0_tcrc,
    s_axis_c2h_0_tdata,
    s_axis_c2h_0_tlast,
    s_axis_c2h_0_tready,
    s_axis_c2h_0_tvalid,
    s_axis_c2h_cmpt_0_cmpt_type,
    s_axis_c2h_cmpt_0_col_idx,
    s_axis_c2h_cmpt_0_data,
    s_axis_c2h_cmpt_0_dpar,
    s_axis_c2h_cmpt_0_err_idx,
    s_axis_c2h_cmpt_0_marker,
    s_axis_c2h_cmpt_0_no_wrb_marker,
    s_axis_c2h_cmpt_0_port_id,
    s_axis_c2h_cmpt_0_qid,
    s_axis_c2h_cmpt_0_size,
    s_axis_c2h_cmpt_0_tready,
    s_axis_c2h_cmpt_0_tvalid,
    s_axis_c2h_cmpt_0_user_trig,
    s_axis_c2h_cmpt_0_wait_pld_pkt_id,
    h2c_byp_in_st_0_addr,
    h2c_byp_in_st_0_len,
    h2c_byp_in_st_0_sop,
    h2c_byp_in_st_0_eop,
    h2c_byp_in_st_0_sdi,
    h2c_byp_in_st_0_mrkr_req,
    h2c_byp_in_st_0_no_dma,
    h2c_byp_in_st_0_qid,
    h2c_byp_in_st_0_error,
    h2c_byp_in_st_0_func,
    h2c_byp_in_st_0_cidx,
    h2c_byp_in_st_0_port_id,
    h2c_byp_in_st_0_valid,
    h2c_byp_in_st_0_ready,
    c2h_byp_in_st_0_addr,
    c2h_byp_in_st_0_qid,
    c2h_byp_in_st_0_error,
    c2h_byp_in_st_0_func,
    c2h_byp_in_st_0_port_id,
    c2h_byp_in_st_0_pfch_tag,
    c2h_byp_in_st_0_valid,
    c2h_byp_in_st_0_ready,
    h2c_byp_out_0_dsc,
    h2c_byp_out_0_st_mm,
    h2c_byp_out_0_dsc_sz,
    h2c_byp_out_0_qid,
    h2c_byp_out_0_error,
    h2c_byp_out_0_func,
    h2c_byp_out_0_cidx,
    h2c_byp_out_0_port_id,
    h2c_byp_out_0_fmt,
    h2c_byp_out_0_valid,
    h2c_byp_out_0_ready,
    c2h_byp_out_0_dsc,
    c2h_byp_out_0_st_mm,
    c2h_byp_out_0_dsc_sz,
    c2h_byp_out_0_qid,
    c2h_byp_out_0_error,
    c2h_byp_out_0_func,
    c2h_byp_out_0_cidx,
    c2h_byp_out_0_port_id,
    c2h_byp_out_0_pfch_tag,
    c2h_byp_out_0_fmt,
    c2h_byp_out_0_valid,
    c2h_byp_out_0_ready,
    tm_dsc_sts_0_valid,
    tm_dsc_sts_0_rdy,
    tm_dsc_sts_0_byp,
    tm_dsc_sts_0_dir,
    tm_dsc_sts_0_mm,
    tm_dsc_sts_0_qid,
    tm_dsc_sts_0_avl,
    tm_dsc_sts_0_qinv,
    tm_dsc_sts_0_qen,
    tm_dsc_sts_0_irq_arm,
    tm_dsc_sts_0_error,
    tm_dsc_sts_0_pidx,
    tm_dsc_sts_0_port_id,
    dsc_crdt_in_0_valid,
    dsc_crdt_in_0_rdy,
    dsc_crdt_in_0_dir,
    dsc_crdt_in_0_fence,
    dsc_crdt_in_0_qid,
    dsc_crdt_in_0_crdt,
    sys_clk,
    sys_clk_gt,
    sys_rst_n);
  output [63:0]M_AXI_BRIDGE_0_araddr;
  output [1:0]M_AXI_BRIDGE_0_arburst;
  output [3:0]M_AXI_BRIDGE_0_arcache;
  output [3:0]M_AXI_BRIDGE_0_arid;
  output [7:0]M_AXI_BRIDGE_0_arlen;
  output M_AXI_BRIDGE_0_arlock;
  output [2:0]M_AXI_BRIDGE_0_arprot;
  input M_AXI_BRIDGE_0_arready;
  output [2:0]M_AXI_BRIDGE_0_arsize;
  output [54:0]M_AXI_BRIDGE_0_aruser;
  output M_AXI_BRIDGE_0_arvalid;
  output [63:0]M_AXI_BRIDGE_0_awaddr;
  output [1:0]M_AXI_BRIDGE_0_awburst;
  output [3:0]M_AXI_BRIDGE_0_awcache;
  output [3:0]M_AXI_BRIDGE_0_awid;
  output [7:0]M_AXI_BRIDGE_0_awlen;
  output M_AXI_BRIDGE_0_awlock;
  output [2:0]M_AXI_BRIDGE_0_awprot;
  input M_AXI_BRIDGE_0_awready;
  output [2:0]M_AXI_BRIDGE_0_awsize;
  output [54:0]M_AXI_BRIDGE_0_awuser;
  output M_AXI_BRIDGE_0_awvalid;
  input [3:0]M_AXI_BRIDGE_0_bid;
  output M_AXI_BRIDGE_0_bready;
  input [1:0]M_AXI_BRIDGE_0_bresp;
  input M_AXI_BRIDGE_0_bvalid;
  input [511:0]M_AXI_BRIDGE_0_rdata;
  input [3:0]M_AXI_BRIDGE_0_rid;
  input M_AXI_BRIDGE_0_rlast;
  output M_AXI_BRIDGE_0_rready;
  input [1:0]M_AXI_BRIDGE_0_rresp;
  input M_AXI_BRIDGE_0_rvalid;
  output [511:0]M_AXI_BRIDGE_0_wdata;
  output M_AXI_BRIDGE_0_wlast;
  input M_AXI_BRIDGE_0_wready;
  output [63:0]M_AXI_BRIDGE_0_wstrb;
  output M_AXI_BRIDGE_0_wvalid;
  output axi_aclk;
  output axi_aresetn;
  output m_axis_h2c_0_err;
  output [31:0]m_axis_h2c_0_mdata;
  output [5:0]m_axis_h2c_0_mty;
  output [2:0]m_axis_h2c_0_port_id;
  output [10:0]m_axis_h2c_0_qid;
  output [31:0]m_axis_h2c_0_tcrc;
  output [511:0]m_axis_h2c_0_tdata;
  output m_axis_h2c_0_tlast;
  input m_axis_h2c_0_tready;
  output m_axis_h2c_0_tvalid;
  output m_axis_h2c_0_zero_byte;
  input [15:0]pcie_ep_rxn;
  input [15:0]pcie_ep_rxp;
  output [15:0]pcie_ep_txn;
  output [15:0]pcie_ep_txp;
  input s_axis_c2h_0_ctrl_has_cmpt;
  input [15:0]s_axis_c2h_0_ctrl_len;
  input s_axis_c2h_0_ctrl_marker;
  input [2:0]s_axis_c2h_0_ctrl_port_id;
  input [10:0]s_axis_c2h_0_ctrl_qid;
  input [6:0]s_axis_c2h_0_ecc;
  input [5:0]s_axis_c2h_0_mty;
  input [31:0]s_axis_c2h_0_tcrc;
  input [511:0]s_axis_c2h_0_tdata;
  input s_axis_c2h_0_tlast;
  output s_axis_c2h_0_tready;
  input s_axis_c2h_0_tvalid;
  input [1:0]s_axis_c2h_cmpt_0_cmpt_type;
  input [2:0]s_axis_c2h_cmpt_0_col_idx;
  input [511:0]s_axis_c2h_cmpt_0_data;
  input [15:0]s_axis_c2h_cmpt_0_dpar;
  input [2:0]s_axis_c2h_cmpt_0_err_idx;
  input s_axis_c2h_cmpt_0_marker;
  input s_axis_c2h_cmpt_0_no_wrb_marker;
  input [2:0]s_axis_c2h_cmpt_0_port_id;
  input [10:0]s_axis_c2h_cmpt_0_qid;
  input [1:0]s_axis_c2h_cmpt_0_size;
  output s_axis_c2h_cmpt_0_tready;
  input s_axis_c2h_cmpt_0_tvalid;
  input s_axis_c2h_cmpt_0_user_trig;
  input [15:0]s_axis_c2h_cmpt_0_wait_pld_pkt_id;
  input sys_clk;
  input sys_clk_gt;
  input sys_rst_n;
  input [63:0]h2c_byp_in_st_0_addr;
  input [15:0]h2c_byp_in_st_0_len;
  input h2c_byp_in_st_0_sop;
  input h2c_byp_in_st_0_eop;
  input h2c_byp_in_st_0_sdi;
  input h2c_byp_in_st_0_mrkr_req;
  input h2c_byp_in_st_0_no_dma;
  input [10:0]h2c_byp_in_st_0_qid;
  input h2c_byp_in_st_0_error;
  input [7:0]h2c_byp_in_st_0_func;
  input [15:0]h2c_byp_in_st_0_cidx;
  input [2:0]h2c_byp_in_st_0_port_id;
  input h2c_byp_in_st_0_valid;
  output h2c_byp_in_st_0_ready;
  input [63:0]c2h_byp_in_st_0_addr;
  input [10:0]c2h_byp_in_st_0_qid;
  input c2h_byp_in_st_0_error;
  input [7:0]c2h_byp_in_st_0_func;
  input [2:0]c2h_byp_in_st_0_port_id;
  input [6:0]c2h_byp_in_st_0_pfch_tag;
  input c2h_byp_in_st_0_valid;
  output c2h_byp_in_st_0_ready;
  output [255:0]h2c_byp_out_0_dsc;
  output h2c_byp_out_0_st_mm;
  output [1:0]h2c_byp_out_0_dsc_sz;
  output [10:0]h2c_byp_out_0_qid;
  output h2c_byp_out_0_error;
  output [7:0]h2c_byp_out_0_func;
  output [15:0]h2c_byp_out_0_cidx;
  output [2:0]h2c_byp_out_0_port_id;
  output [2:0]h2c_byp_out_0_fmt;
  output h2c_byp_out_0_valid;
  input h2c_byp_out_0_ready;
  output [255:0]c2h_byp_out_0_dsc;
  output c2h_byp_out_0_st_mm;
  output [1:0]c2h_byp_out_0_dsc_sz;
  output [10:0]c2h_byp_out_0_qid;
  output c2h_byp_out_0_error;
  output [7:0]c2h_byp_out_0_func;
  output [15:0]c2h_byp_out_0_cidx;
  output [2:0]c2h_byp_out_0_port_id;
  output [6:0]c2h_byp_out_0_pfch_tag;
  output [2:0]c2h_byp_out_0_fmt;
  output c2h_byp_out_0_valid;
  input c2h_byp_out_0_ready;
  output           tm_dsc_sts_0_valid;
  input          tm_dsc_sts_0_rdy;
  output           tm_dsc_sts_0_byp;
  output           tm_dsc_sts_0_dir;
  output           tm_dsc_sts_0_mm;
  output [10:0]    tm_dsc_sts_0_qid;
  output [15:0]    tm_dsc_sts_0_avl;
  output           tm_dsc_sts_0_qinv;
  output           tm_dsc_sts_0_qen;
  output           tm_dsc_sts_0_irq_arm;
  output           tm_dsc_sts_0_error;
  output [15:0]    tm_dsc_sts_0_pidx;
  output [2:0]     tm_dsc_sts_0_port_id;
  
  input          dsc_crdt_in_0_valid;
  output           dsc_crdt_in_0_rdy;
  input          dsc_crdt_in_0_dir;
  input          dsc_crdt_in_0_fence;
  input [10:0]   dsc_crdt_in_0_qid;
  input [15:0]   dsc_crdt_in_0_crdt;

  localparam H2C_WAIT_BD = 2'b00;
  localparam H2C_SENDING = 2'b01;
  localparam C2H_WAIT_BD = 2'b00;
  localparam C2H_RECVING = 2'b01;

  reg [1 : 0] h2c_state;
  reg [1 : 0] c2h_state;

  wire h2c_fifo_wr_en, h2c_fifo_rd_en;
  wire c2h_fifo_wr_en, c2h_fifo_rd_en;
  
  wire [31 : 0] h2c_fifo_din;
  wire [31 : 0] c2h_fifo_din;

  wire [31 : 0] h2c_fifo_dout;
  wire [31 : 0] c2h_fifo_dout;

  wire h2c_fifo_empty, h2c_fifo_full;
  wire c2h_fifo_empty, c2h_fifo_full;

  wire [15:0] h2c_cur_desc_len;
  wire [10:0] h2c_cur_desc_qid;
  wire [2:0]  h2c_cur_desc_port_id;
  reg [15:0] h2c_desc_len_sent;
  reg [15:0] c2h_desc_len_recv;
  wire h2c_is_eop;
  wire c2h_is_eop;

  wire [15:0] c2h_cur_desc_len;
  wire [10:0] c2h_cur_desc_qid;
  wire [2:0]  c2h_cur_desc_port_id;

  fifo #(.DATA_WIDTH(32)) h2c_fifo(
    .clk(axi_aclk),
    .resetn(axi_aresetn),
    .wr_en(h2c_fifo_wr_en),
    .rd_en(h2c_fifo_rd_en),
    .din(h2c_fifo_din),
    .dout(h2c_fifo_dout),
    .empty(h2c_fifo_empty),
    .full(h2c_fifo_full)
  );

  fifo #(.DATA_WIDTH(32)) c2h_fifo(
    .clk(axi_aclk),
    .resetn(axi_aresetn),
    .wr_en(c2h_fifo_wr_en),
    .rd_en(c2h_fifo_rd_en),
    .din(c2h_fifo_din),
    .dout(c2h_fifo_dout),
    .empty(c2h_fifo_empty),
    .full(c2h_fifo_full)
  );

  assign axi_aclk = sys_clk;
  assign axi_aresetn = sys_rst_n;

  assign h2c_fifo_din = {h2c_byp_in_st_0_len, h2c_byp_in_st_0_qid, h2c_byp_in_st_0_port_id};
  assign c2h_fifo_din = {c2h_byp_in_st_0_qid, c2h_byp_in_st_0_port_id};
  assign {h2c_cur_desc_len, h2c_cur_desc_qid, h2c_cur_desc_port_id} = h2c_fifo_dout;
  assign {c2h_cur_desc_len, c2h_cur_desc_qid, c2h_cur_desc_port_id} = c2h_fifo_dout;

  // assign h2c_byp_in_st_0_ready = h2c_byp_in_st_0_valid & ~h2c_fifo_full;
  assign h2c_byp_in_st_0_ready = ~h2c_fifo_full;
  // assign c2h_byp_in_st_0_ready = c2h_byp_in_st_0_valid & ~c2h_fifo_full;
  assign c2h_byp_in_st_0_ready = ~c2h_fifo_full;

  assign h2c_fifo_wr_en = h2c_byp_in_st_0_ready & h2c_byp_in_st_0_valid;
  assign c2h_fifo_wr_en = c2h_byp_in_st_0_ready & c2h_byp_in_st_0_valid;

  assign h2c_is_eop = h2c_desc_len_sent + 16'd64 >= h2c_cur_desc_len;
  assign c2h_is_eop = s_axis_c2h_0_tlast;

  always @ (posedge axi_aclk) begin
    if (~axi_aresetn) begin
      h2c_state <= H2C_WAIT_BD;
      h2c_desc_len_sent <= 16'd0;
    end else begin
      case (h2c_state)
        H2C_WAIT_BD: begin
          if (!h2c_fifo_empty) begin
            h2c_state <= H2C_SENDING;
            h2c_desc_len_sent <= 16'd0;
          end 
        end
        H2C_SENDING: begin
          if (m_axis_h2c_0_tvalid) begin
            h2c_state <= h2c_is_eop ? H2C_WAIT_BD : H2C_SENDING;
            h2c_desc_len_sent <= h2c_desc_len_sent + 16'd64;
          end
        end
        default: begin
          h2c_state <= H2C_WAIT_BD;
        end
      endcase
    end
  end

  //
  always @ (posedge axi_aclk) begin
    if (~axi_aresetn) begin
      c2h_state <= C2H_WAIT_BD;
      c2h_desc_len_recv <= 16'd0;
    end else begin
      case (c2h_state)
        C2H_WAIT_BD: begin
          if (!c2h_fifo_empty) begin
            c2h_state <= C2H_RECVING;
            c2h_desc_len_recv <= 16'd0;
          end 
        end
        C2H_RECVING: begin
          if (s_axis_c2h_0_tvalid) begin
            c2h_state <= c2h_is_eop ? C2H_WAIT_BD : C2H_RECVING;
            c2h_desc_len_recv <= c2h_desc_len_recv + 16'd64;
          end
        end
        default: begin
          c2h_state <= C2H_WAIT_BD;
        end
      endcase
    end
  end
  //

  assign m_axis_h2c_0_err = 1'b0;
  assign m_axis_h2c_0_mdata = 32'b0;
  assign m_axis_h2c_0_mty = 6'b0;
  assign m_axis_h2c_0_port_id = h2c_cur_desc_port_id;
  assign m_axis_h2c_0_qid = h2c_cur_desc_qid;
  assign m_axis_h2c_0_tcrc = 32'b0;
  assign m_axis_h2c_0_tlast = m_axis_h2c_0_tvalid & m_axis_h2c_0_tready & h2c_is_eop;
  assign m_axis_h2c_0_tvalid = m_axis_h2c_0_tready & (h2c_state == H2C_SENDING);
  assign m_axis_h2c_0_tdata[63:0] = {h2c_desc_len_sent, 48'd0};
  assign m_axis_h2c_0_tdata[127:64] = {h2c_desc_len_sent, 48'd1};
  assign m_axis_h2c_0_tdata[191:128] = {h2c_desc_len_sent, 48'd2};
  assign m_axis_h2c_0_tdata[255:192] = {h2c_desc_len_sent, 48'd3};
  assign m_axis_h2c_0_tdata[319:256] = {h2c_desc_len_sent, 48'd4};
  assign m_axis_h2c_0_tdata[383:320] = {h2c_desc_len_sent, 48'd5};
  assign m_axis_h2c_0_tdata[447:384] = {h2c_desc_len_sent, 48'd6};
  assign m_axis_h2c_0_tdata[511:448] = {h2c_desc_len_sent, 48'd7};
  
  assign h2c_fifo_rd_en = m_axis_h2c_0_tlast;

  //
  assign s_axis_c2h_0_mty = 6'b0;
  assign s_axis_c2h_0_ctrl_port_id = c2h_cur_desc_port_id;
  assign s_axis_c2h_0_ctrl_qid = c2h_cur_desc_qid;
  assign s_axis_c2h_0_tready = c2h_state == C2H_RECVING;

  assign c2h_fifo_rd_en = s_axis_c2h_0_tlast;

endmodule