//Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2020.2_AR75986 (lin64) Build 3064766 Wed Nov 18 09:12:47 MST 2020
//Date        : Wed Mar 26 22:49:44 2025
//Host        : ubuntu-2288H-V5 running 64-bit Ubuntu 22.04.3 LTS
//Command     : generate_target pcie_ep_wrapper.bd
//Design      : pcie_ep_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module qdma_ep
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
    S_AXI_BRIDGE_0_araddr,
    S_AXI_BRIDGE_0_arburst,
    S_AXI_BRIDGE_0_arid,
    S_AXI_BRIDGE_0_arlen,
    S_AXI_BRIDGE_0_arready,
    S_AXI_BRIDGE_0_arregion,
    S_AXI_BRIDGE_0_arsize,
    S_AXI_BRIDGE_0_aruser,
    S_AXI_BRIDGE_0_arvalid,
    S_AXI_BRIDGE_0_awaddr,
    S_AXI_BRIDGE_0_awburst,
    S_AXI_BRIDGE_0_awid,
    S_AXI_BRIDGE_0_awlen,
    S_AXI_BRIDGE_0_awready,
    S_AXI_BRIDGE_0_awregion,
    S_AXI_BRIDGE_0_awsize,
    S_AXI_BRIDGE_0_awuser,
    S_AXI_BRIDGE_0_awvalid,
    S_AXI_BRIDGE_0_bid,
    S_AXI_BRIDGE_0_bready,
    S_AXI_BRIDGE_0_bresp,
    S_AXI_BRIDGE_0_bvalid,
    S_AXI_BRIDGE_0_rdata,
    S_AXI_BRIDGE_0_rid,
    S_AXI_BRIDGE_0_rlast,
    S_AXI_BRIDGE_0_rready,
    S_AXI_BRIDGE_0_rresp,
    S_AXI_BRIDGE_0_ruser,
    S_AXI_BRIDGE_0_rvalid,
    S_AXI_BRIDGE_0_wdata,
    S_AXI_BRIDGE_0_wlast,
    S_AXI_BRIDGE_0_wready,
    S_AXI_BRIDGE_0_wstrb,
    S_AXI_BRIDGE_0_wuser,
    S_AXI_BRIDGE_0_wvalid,
    axi_aclk,
    axi_aresetn,
    c2h_byp_in_st_0_addr,
    c2h_byp_in_st_0_error,
    c2h_byp_in_st_0_func,
    c2h_byp_in_st_0_pfch_tag,
    c2h_byp_in_st_0_port_id,
    c2h_byp_in_st_0_qid,
    c2h_byp_in_st_0_ready,
    c2h_byp_in_st_0_valid,
    c2h_byp_out_0_cidx,
    c2h_byp_out_0_dsc,
    c2h_byp_out_0_dsc_sz,
    c2h_byp_out_0_error,
    c2h_byp_out_0_fmt,
    c2h_byp_out_0_func,
    c2h_byp_out_0_pfch_tag,
    c2h_byp_out_0_port_id,
    c2h_byp_out_0_qid,
    c2h_byp_out_0_ready,
    c2h_byp_out_0_st_mm,
    c2h_byp_out_0_valid,
    dsc_crdt_in_0_crdt,
    dsc_crdt_in_0_dir,
    dsc_crdt_in_0_fence,
    dsc_crdt_in_0_qid,
    dsc_crdt_in_0_rdy,
    dsc_crdt_in_0_valid,
    h2c_byp_in_st_0_addr,
    h2c_byp_in_st_0_cidx,
    h2c_byp_in_st_0_eop,
    h2c_byp_in_st_0_error,
    h2c_byp_in_st_0_func,
    h2c_byp_in_st_0_len,
    h2c_byp_in_st_0_mrkr_req,
    h2c_byp_in_st_0_no_dma,
    h2c_byp_in_st_0_port_id,
    h2c_byp_in_st_0_qid,
    h2c_byp_in_st_0_ready,
    h2c_byp_in_st_0_sdi,
    h2c_byp_in_st_0_sop,
    h2c_byp_in_st_0_valid,
    h2c_byp_out_0_cidx,
    h2c_byp_out_0_dsc,
    h2c_byp_out_0_dsc_sz,
    h2c_byp_out_0_error,
    h2c_byp_out_0_fmt,
    h2c_byp_out_0_func,
    h2c_byp_out_0_port_id,
    h2c_byp_out_0_qid,
    h2c_byp_out_0_ready,
    h2c_byp_out_0_st_mm,
    h2c_byp_out_0_valid,
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
    sys_clk,
    sys_clk_gt,
    sys_rst_n,
    tm_dsc_sts_0_avl,
    tm_dsc_sts_0_byp,
    tm_dsc_sts_0_dir,
    tm_dsc_sts_0_error,
    tm_dsc_sts_0_irq_arm,
    tm_dsc_sts_0_mm,
    tm_dsc_sts_0_pidx,
    tm_dsc_sts_0_port_id,
    tm_dsc_sts_0_qen,
    tm_dsc_sts_0_qid,
    tm_dsc_sts_0_qinv,
    tm_dsc_sts_0_rdy,
    tm_dsc_sts_0_valid);
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
  input [63:0]S_AXI_BRIDGE_0_araddr;
  input [1:0]S_AXI_BRIDGE_0_arburst;
  input [3:0]S_AXI_BRIDGE_0_arid;
  input [7:0]S_AXI_BRIDGE_0_arlen;
  output S_AXI_BRIDGE_0_arready;
  input [3:0]S_AXI_BRIDGE_0_arregion;
  input [2:0]S_AXI_BRIDGE_0_arsize;
  input [11:0]S_AXI_BRIDGE_0_aruser;
  input S_AXI_BRIDGE_0_arvalid;
  input [63:0]S_AXI_BRIDGE_0_awaddr;
  input [1:0]S_AXI_BRIDGE_0_awburst;
  input [3:0]S_AXI_BRIDGE_0_awid;
  input [7:0]S_AXI_BRIDGE_0_awlen;
  output S_AXI_BRIDGE_0_awready;
  input [3:0]S_AXI_BRIDGE_0_awregion;
  input [2:0]S_AXI_BRIDGE_0_awsize;
  input [11:0]S_AXI_BRIDGE_0_awuser;
  input S_AXI_BRIDGE_0_awvalid;
  output [3:0]S_AXI_BRIDGE_0_bid;
  input S_AXI_BRIDGE_0_bready;
  output [1:0]S_AXI_BRIDGE_0_bresp;
  output S_AXI_BRIDGE_0_bvalid;
  output [511:0]S_AXI_BRIDGE_0_rdata;
  output [3:0]S_AXI_BRIDGE_0_rid;
  output S_AXI_BRIDGE_0_rlast;
  input S_AXI_BRIDGE_0_rready;
  output [1:0]S_AXI_BRIDGE_0_rresp;
  output [63:0]S_AXI_BRIDGE_0_ruser;
  output S_AXI_BRIDGE_0_rvalid;
  input [511:0]S_AXI_BRIDGE_0_wdata;
  input S_AXI_BRIDGE_0_wlast;
  output S_AXI_BRIDGE_0_wready;
  input [63:0]S_AXI_BRIDGE_0_wstrb;
  input [63:0]S_AXI_BRIDGE_0_wuser;
  input S_AXI_BRIDGE_0_wvalid;
  output axi_aclk;
  output axi_aresetn;
  input [63:0]c2h_byp_in_st_0_addr;
  input c2h_byp_in_st_0_error;
  input [7:0]c2h_byp_in_st_0_func;
  input [6:0]c2h_byp_in_st_0_pfch_tag;
  input [2:0]c2h_byp_in_st_0_port_id;
  input [10:0]c2h_byp_in_st_0_qid;
  output c2h_byp_in_st_0_ready;
  input c2h_byp_in_st_0_valid;
  output [15:0]c2h_byp_out_0_cidx;
  output [255:0]c2h_byp_out_0_dsc;
  output [1:0]c2h_byp_out_0_dsc_sz;
  output c2h_byp_out_0_error;
  output [3:0]c2h_byp_out_0_fmt;
  output [7:0]c2h_byp_out_0_func;
  output [6:0]c2h_byp_out_0_pfch_tag;
  output [2:0]c2h_byp_out_0_port_id;
  output [10:0]c2h_byp_out_0_qid;
  input c2h_byp_out_0_ready;
  output c2h_byp_out_0_st_mm;
  output c2h_byp_out_0_valid;
  input [15:0]dsc_crdt_in_0_crdt;
  input dsc_crdt_in_0_dir;
  input dsc_crdt_in_0_fence;
  input [10:0]dsc_crdt_in_0_qid;
  output dsc_crdt_in_0_rdy;
  input dsc_crdt_in_0_valid;
  input [63:0]h2c_byp_in_st_0_addr;
  input [15:0]h2c_byp_in_st_0_cidx;
  input h2c_byp_in_st_0_eop;
  input h2c_byp_in_st_0_error;
  input [7:0]h2c_byp_in_st_0_func;
  input [15:0]h2c_byp_in_st_0_len;
  input h2c_byp_in_st_0_mrkr_req;
  input h2c_byp_in_st_0_no_dma;
  input [2:0]h2c_byp_in_st_0_port_id;
  input [10:0]h2c_byp_in_st_0_qid;
  output h2c_byp_in_st_0_ready;
  input h2c_byp_in_st_0_sdi;
  input h2c_byp_in_st_0_sop;
  input h2c_byp_in_st_0_valid;
  output [15:0]h2c_byp_out_0_cidx;
  output [255:0]h2c_byp_out_0_dsc;
  output [1:0]h2c_byp_out_0_dsc_sz;
  output h2c_byp_out_0_error;
  output [3:0]h2c_byp_out_0_fmt;
  output [7:0]h2c_byp_out_0_func;
  output [2:0]h2c_byp_out_0_port_id;
  output [10:0]h2c_byp_out_0_qid;
  input h2c_byp_out_0_ready;
  output h2c_byp_out_0_st_mm;
  output h2c_byp_out_0_valid;
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
  output [15:0]tm_dsc_sts_0_avl;
  output tm_dsc_sts_0_byp;
  output tm_dsc_sts_0_dir;
  output tm_dsc_sts_0_error;
  output tm_dsc_sts_0_irq_arm;
  output tm_dsc_sts_0_mm;
  output [15:0]tm_dsc_sts_0_pidx;
  output [2:0]tm_dsc_sts_0_port_id;
  output tm_dsc_sts_0_qen;
  output [10:0]tm_dsc_sts_0_qid;
  output tm_dsc_sts_0_qinv;
  input tm_dsc_sts_0_rdy;
  output tm_dsc_sts_0_valid;

  wire [63:0]M_AXI_BRIDGE_0_araddr;
  wire [1:0]M_AXI_BRIDGE_0_arburst;
  wire [3:0]M_AXI_BRIDGE_0_arcache;
  wire [3:0]M_AXI_BRIDGE_0_arid;
  wire [7:0]M_AXI_BRIDGE_0_arlen;
  wire M_AXI_BRIDGE_0_arlock;
  wire [2:0]M_AXI_BRIDGE_0_arprot;
  wire M_AXI_BRIDGE_0_arready;
  wire [2:0]M_AXI_BRIDGE_0_arsize;
  wire [54:0]M_AXI_BRIDGE_0_aruser;
  wire M_AXI_BRIDGE_0_arvalid;
  wire [63:0]M_AXI_BRIDGE_0_awaddr;
  wire [1:0]M_AXI_BRIDGE_0_awburst;
  wire [3:0]M_AXI_BRIDGE_0_awcache;
  wire [3:0]M_AXI_BRIDGE_0_awid;
  wire [7:0]M_AXI_BRIDGE_0_awlen;
  wire M_AXI_BRIDGE_0_awlock;
  wire [2:0]M_AXI_BRIDGE_0_awprot;
  wire M_AXI_BRIDGE_0_awready;
  wire [2:0]M_AXI_BRIDGE_0_awsize;
  wire [54:0]M_AXI_BRIDGE_0_awuser;
  wire M_AXI_BRIDGE_0_awvalid;
  wire [3:0]M_AXI_BRIDGE_0_bid;
  wire M_AXI_BRIDGE_0_bready;
  wire [1:0]M_AXI_BRIDGE_0_bresp;
  wire M_AXI_BRIDGE_0_bvalid;
  wire [511:0]M_AXI_BRIDGE_0_rdata;
  wire [3:0]M_AXI_BRIDGE_0_rid;
  wire M_AXI_BRIDGE_0_rlast;
  wire M_AXI_BRIDGE_0_rready;
  wire [1:0]M_AXI_BRIDGE_0_rresp;
  wire M_AXI_BRIDGE_0_rvalid;
  wire [511:0]M_AXI_BRIDGE_0_wdata;
  wire M_AXI_BRIDGE_0_wlast;
  wire M_AXI_BRIDGE_0_wready;
  wire [63:0]M_AXI_BRIDGE_0_wstrb;
  wire M_AXI_BRIDGE_0_wvalid;
  wire [63:0]S_AXI_BRIDGE_0_araddr;
  wire [1:0]S_AXI_BRIDGE_0_arburst;
  wire [3:0]S_AXI_BRIDGE_0_arid;
  wire [7:0]S_AXI_BRIDGE_0_arlen;
  wire S_AXI_BRIDGE_0_arready;
  wire [3:0]S_AXI_BRIDGE_0_arregion;
  wire [2:0]S_AXI_BRIDGE_0_arsize;
  wire [11:0]S_AXI_BRIDGE_0_aruser;
  wire S_AXI_BRIDGE_0_arvalid;
  wire [63:0]S_AXI_BRIDGE_0_awaddr;
  wire [1:0]S_AXI_BRIDGE_0_awburst;
  wire [3:0]S_AXI_BRIDGE_0_awid;
  wire [7:0]S_AXI_BRIDGE_0_awlen;
  wire S_AXI_BRIDGE_0_awready;
  wire [3:0]S_AXI_BRIDGE_0_awregion;
  wire [2:0]S_AXI_BRIDGE_0_awsize;
  wire [11:0]S_AXI_BRIDGE_0_awuser;
  wire S_AXI_BRIDGE_0_awvalid;
  wire [3:0]S_AXI_BRIDGE_0_bid;
  wire S_AXI_BRIDGE_0_bready;
  wire [1:0]S_AXI_BRIDGE_0_bresp;
  wire S_AXI_BRIDGE_0_bvalid;
  wire [511:0]S_AXI_BRIDGE_0_rdata;
  wire [3:0]S_AXI_BRIDGE_0_rid;
  wire S_AXI_BRIDGE_0_rlast;
  wire S_AXI_BRIDGE_0_rready;
  wire [1:0]S_AXI_BRIDGE_0_rresp;
  wire [63:0]S_AXI_BRIDGE_0_ruser;
  wire S_AXI_BRIDGE_0_rvalid;
  wire [511:0]S_AXI_BRIDGE_0_wdata;
  wire S_AXI_BRIDGE_0_wlast;
  wire S_AXI_BRIDGE_0_wready;
  wire [63:0]S_AXI_BRIDGE_0_wstrb;
  wire [63:0]S_AXI_BRIDGE_0_wuser;
  wire S_AXI_BRIDGE_0_wvalid;
  wire axi_aclk;
  wire axi_aresetn;
  wire [63:0]c2h_byp_in_st_0_addr;
  wire c2h_byp_in_st_0_error;
  wire [7:0]c2h_byp_in_st_0_func;
  wire [6:0]c2h_byp_in_st_0_pfch_tag;
  wire [2:0]c2h_byp_in_st_0_port_id;
  wire [10:0]c2h_byp_in_st_0_qid;
  wire c2h_byp_in_st_0_ready;
  wire c2h_byp_in_st_0_valid;
  wire [15:0]c2h_byp_out_0_cidx;
  wire [255:0]c2h_byp_out_0_dsc;
  wire [1:0]c2h_byp_out_0_dsc_sz;
  wire c2h_byp_out_0_error;
  wire [3:0]c2h_byp_out_0_fmt;
  wire [7:0]c2h_byp_out_0_func;
  wire [6:0]c2h_byp_out_0_pfch_tag;
  wire [2:0]c2h_byp_out_0_port_id;
  wire [10:0]c2h_byp_out_0_qid;
  wire c2h_byp_out_0_ready;
  wire c2h_byp_out_0_st_mm;
  wire c2h_byp_out_0_valid;
  wire [15:0]dsc_crdt_in_0_crdt;
  wire dsc_crdt_in_0_dir;
  wire dsc_crdt_in_0_fence;
  wire [10:0]dsc_crdt_in_0_qid;
  wire dsc_crdt_in_0_rdy;
  wire dsc_crdt_in_0_valid;
  wire [63:0]h2c_byp_in_st_0_addr;
  wire [15:0]h2c_byp_in_st_0_cidx;
  wire h2c_byp_in_st_0_eop;
  wire h2c_byp_in_st_0_error;
  wire [7:0]h2c_byp_in_st_0_func;
  wire [15:0]h2c_byp_in_st_0_len;
  wire h2c_byp_in_st_0_mrkr_req;
  wire h2c_byp_in_st_0_no_dma;
  wire [2:0]h2c_byp_in_st_0_port_id;
  wire [10:0]h2c_byp_in_st_0_qid;
  wire h2c_byp_in_st_0_ready;
  wire h2c_byp_in_st_0_sdi;
  wire h2c_byp_in_st_0_sop;
  wire h2c_byp_in_st_0_valid;
  wire [15:0]h2c_byp_out_0_cidx;
  wire [255:0]h2c_byp_out_0_dsc;
  wire [1:0]h2c_byp_out_0_dsc_sz;
  wire h2c_byp_out_0_error;
  wire [3:0]h2c_byp_out_0_fmt;
  wire [7:0]h2c_byp_out_0_func;
  wire [2:0]h2c_byp_out_0_port_id;
  wire [10:0]h2c_byp_out_0_qid;
  wire h2c_byp_out_0_ready;
  wire h2c_byp_out_0_st_mm;
  wire h2c_byp_out_0_valid;
  wire m_axis_h2c_0_err;
  wire [31:0]m_axis_h2c_0_mdata;
  wire [5:0]m_axis_h2c_0_mty;
  wire [2:0]m_axis_h2c_0_port_id;
  wire [10:0]m_axis_h2c_0_qid;
  wire [31:0]m_axis_h2c_0_tcrc;
  wire [511:0]m_axis_h2c_0_tdata;
  wire m_axis_h2c_0_tlast;
  wire m_axis_h2c_0_tready;
  wire m_axis_h2c_0_tvalid;
  wire m_axis_h2c_0_zero_byte;
  wire [15:0]pcie_ep_rxn;
  wire [15:0]pcie_ep_rxp;
  wire [15:0]pcie_ep_txn;
  wire [15:0]pcie_ep_txp;
  wire s_axis_c2h_0_ctrl_has_cmpt;
  wire [15:0]s_axis_c2h_0_ctrl_len;
  wire s_axis_c2h_0_ctrl_marker;
  wire [2:0]s_axis_c2h_0_ctrl_port_id;
  wire [10:0]s_axis_c2h_0_ctrl_qid;
  wire [6:0]s_axis_c2h_0_ecc;
  wire [5:0]s_axis_c2h_0_mty;
  wire [31:0]s_axis_c2h_0_tcrc;
  wire [511:0]s_axis_c2h_0_tdata;
  wire s_axis_c2h_0_tlast;
  wire s_axis_c2h_0_tready;
  wire s_axis_c2h_0_tvalid;
  wire [1:0]s_axis_c2h_cmpt_0_cmpt_type;
  wire [2:0]s_axis_c2h_cmpt_0_col_idx;
  wire [511:0]s_axis_c2h_cmpt_0_data;
  wire [15:0]s_axis_c2h_cmpt_0_dpar;
  wire [2:0]s_axis_c2h_cmpt_0_err_idx;
  wire s_axis_c2h_cmpt_0_marker;
  wire s_axis_c2h_cmpt_0_no_wrb_marker;
  wire [2:0]s_axis_c2h_cmpt_0_port_id;
  wire [10:0]s_axis_c2h_cmpt_0_qid;
  wire [1:0]s_axis_c2h_cmpt_0_size;
  wire s_axis_c2h_cmpt_0_tready;
  wire s_axis_c2h_cmpt_0_tvalid;
  wire s_axis_c2h_cmpt_0_user_trig;
  wire [15:0]s_axis_c2h_cmpt_0_wait_pld_pkt_id;
  wire sys_clk;
  wire sys_clk_gt;
  wire sys_rst_n;
  wire [15:0]tm_dsc_sts_0_avl;
  wire tm_dsc_sts_0_byp;
  wire tm_dsc_sts_0_dir;
  wire tm_dsc_sts_0_error;
  wire tm_dsc_sts_0_irq_arm;
  wire tm_dsc_sts_0_mm;
  wire [15:0]tm_dsc_sts_0_pidx;
  wire [2:0]tm_dsc_sts_0_port_id;
  wire tm_dsc_sts_0_qen;
  wire [10:0]tm_dsc_sts_0_qid;
  wire tm_dsc_sts_0_qinv;
  wire tm_dsc_sts_0_rdy;
  wire tm_dsc_sts_0_valid;

endmodule
