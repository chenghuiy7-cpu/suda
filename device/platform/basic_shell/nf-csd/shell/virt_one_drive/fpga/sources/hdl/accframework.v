//Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2020.2_AR75986 (lin64) Build 3064766 Wed Nov 18 09:12:47 MST 2020
//Date        : Sun Mar  9 21:00:28 2025
//Host        : ubuntu-2288H-V5 running 64-bit Ubuntu 22.04.3 LTS
//Command     : generate_target accframework_wrapper.bd
//Design      : accframework_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module accframework
   (axi_mem_access_araddr,
    axi_mem_access_arburst,
    axi_mem_access_arcache,
    axi_mem_access_arlen,
    axi_mem_access_arlock,
    axi_mem_access_arprot,
    axi_mem_access_arqos,
    axi_mem_access_arready,
    axi_mem_access_arregion,
    axi_mem_access_arsize,
    axi_mem_access_arvalid,
    axi_mem_access_awaddr,
    axi_mem_access_awburst,
    axi_mem_access_awcache,
    axi_mem_access_awlen,
    axi_mem_access_awlock,
    axi_mem_access_awprot,
    axi_mem_access_awqos,
    axi_mem_access_awready,
    axi_mem_access_awregion,
    axi_mem_access_awsize,
    axi_mem_access_awvalid,
    axi_mem_access_bready,
    axi_mem_access_bresp,
    axi_mem_access_bvalid,
    axi_mem_access_rdata,
    axi_mem_access_rlast,
    axi_mem_access_rready,
    axi_mem_access_rresp,
    axi_mem_access_rvalid,
    axi_mem_access_wdata,
    axi_mem_access_wlast,
    axi_mem_access_wready,
    axi_mem_access_wstrb,
    axi_mem_access_wvalid,
    c2h_data_tdata,
    c2h_data_tid,
    c2h_data_tkeep,
    c2h_data_tlast,
    c2h_data_tready,
    c2h_data_tstrb,
    c2h_data_tuser,
    c2h_data_tvalid,
    clk,
    m_ext_axis_tdata,
    m_ext_axis_tdest,
    m_ext_axis_tid,
    m_ext_axis_tkeep,
    m_ext_axis_tlast,
    m_ext_axis_tready,
    m_ext_axis_tstrb,
    m_ext_axis_tuser,
    m_ext_axis_tvalid,
    prp_out_tdata,
    prp_out_tready,
    prp_out_tuser,
    prp_out_tvalid,
    read_mem_cmd_tdata,
    read_mem_cmd_tready,
    read_mem_cmd_tvalid,
    read_mem_data_tdata,
    read_mem_data_tdest,
    read_mem_data_tid,
    read_mem_data_tkeep,
    read_mem_data_tlast,
    read_mem_data_tready,
    read_mem_data_tstrb,
    read_mem_data_tvalid,
    resetn,
    s_axi_Manager_araddr,
    s_axi_Manager_arprot,
    s_axi_Manager_arready,
    s_axi_Manager_arvalid,
    s_axi_Manager_awaddr,
    s_axi_Manager_awprot,
    s_axi_Manager_awready,
    s_axi_Manager_awvalid,
    s_axi_Manager_bready,
    s_axi_Manager_bresp,
    s_axi_Manager_bvalid,
    s_axi_Manager_rdata,
    s_axi_Manager_rready,
    s_axi_Manager_rresp,
    s_axi_Manager_rvalid,
    s_axi_Manager_wdata,
    s_axi_Manager_wready,
    s_axi_Manager_wstrb,
    s_axi_Manager_wvalid,
    s_ext_axis_tdata,
    s_ext_axis_tdest,
    s_ext_axis_tid,
    s_ext_axis_tkeep,
    s_ext_axis_tlast,
    s_ext_axis_tready,
    s_ext_axis_tstrb,
    s_ext_axis_tuser,
    s_ext_axis_tvalid,
    write_mem_cmd_tdata,
    write_mem_cmd_tready,
    write_mem_cmd_tvalid,
    write_mem_data_tdata,
    write_mem_data_tdest,
    write_mem_data_tid,
    write_mem_data_tkeep,
    write_mem_data_tlast,
    write_mem_data_tready,
    write_mem_data_tstrb,
    write_mem_data_tvalid);
  output [63:0]axi_mem_access_araddr;
  output [1:0]axi_mem_access_arburst;
  output [3:0]axi_mem_access_arcache;
  output [7:0]axi_mem_access_arlen;
  output [0:0]axi_mem_access_arlock;
  output [2:0]axi_mem_access_arprot;
  output [3:0]axi_mem_access_arqos;
  input axi_mem_access_arready;
  output [3:0]axi_mem_access_arregion;
  output [2:0]axi_mem_access_arsize;
  output axi_mem_access_arvalid;
  output [63:0]axi_mem_access_awaddr;
  output [1:0]axi_mem_access_awburst;
  output [3:0]axi_mem_access_awcache;
  output [7:0]axi_mem_access_awlen;
  output [0:0]axi_mem_access_awlock;
  output [2:0]axi_mem_access_awprot;
  output [3:0]axi_mem_access_awqos;
  input axi_mem_access_awready;
  output [3:0]axi_mem_access_awregion;
  output [2:0]axi_mem_access_awsize;
  output axi_mem_access_awvalid;
  output axi_mem_access_bready;
  input [1:0]axi_mem_access_bresp;
  input axi_mem_access_bvalid;
  input [31:0]axi_mem_access_rdata;
  input axi_mem_access_rlast;
  output axi_mem_access_rready;
  input [1:0]axi_mem_access_rresp;
  input axi_mem_access_rvalid;
  output [31:0]axi_mem_access_wdata;
  output axi_mem_access_wlast;
  input axi_mem_access_wready;
  output [3:0]axi_mem_access_wstrb;
  output axi_mem_access_wvalid;
  output [511:0]c2h_data_tdata;
  output [0:0]c2h_data_tid;
  output [63:0]c2h_data_tkeep;
  output c2h_data_tlast;
  input c2h_data_tready;
  output [63:0]c2h_data_tstrb;
  output [0:0]c2h_data_tuser;
  output c2h_data_tvalid;
  input clk;
  output [511:0]m_ext_axis_tdata;
  output [7:0]m_ext_axis_tdest;
  output [3:0]m_ext_axis_tid;
  output [63:0]m_ext_axis_tkeep;
  output [0:0]m_ext_axis_tlast;
  input [0:0]m_ext_axis_tready;
  output [63:0]m_ext_axis_tstrb;
  output [7:0]m_ext_axis_tuser;
  output [0:0]m_ext_axis_tvalid;
  output [160:0]prp_out_tdata;
  input prp_out_tready;
  output [31:0]prp_out_tuser;
  output prp_out_tvalid;
  output [79:0]read_mem_cmd_tdata;
  input read_mem_cmd_tready;
  output read_mem_cmd_tvalid;
  input [511:0]read_mem_data_tdata;
  input [7:0]read_mem_data_tdest;
  input [7:0]read_mem_data_tid;
  input [63:0]read_mem_data_tkeep;
  input [0:0]read_mem_data_tlast;
  output read_mem_data_tready;
  input [63:0]read_mem_data_tstrb;
  input read_mem_data_tvalid;
  input resetn;
  input [15:0]s_axi_Manager_araddr;
  input [2:0]s_axi_Manager_arprot;
  output s_axi_Manager_arready;
  input s_axi_Manager_arvalid;
  input [15:0]s_axi_Manager_awaddr;
  input [2:0]s_axi_Manager_awprot;
  output s_axi_Manager_awready;
  input s_axi_Manager_awvalid;
  input s_axi_Manager_bready;
  output [1:0]s_axi_Manager_bresp;
  output s_axi_Manager_bvalid;
  output [31:0]s_axi_Manager_rdata;
  input s_axi_Manager_rready;
  output [1:0]s_axi_Manager_rresp;
  output s_axi_Manager_rvalid;
  input [31:0]s_axi_Manager_wdata;
  output s_axi_Manager_wready;
  input [3:0]s_axi_Manager_wstrb;
  input s_axi_Manager_wvalid;
  input [511:0]s_ext_axis_tdata;
  input [7:0]s_ext_axis_tdest;
  input [3:0]s_ext_axis_tid;
  input [63:0]s_ext_axis_tkeep;
  input [0:0]s_ext_axis_tlast;
  output [0:0]s_ext_axis_tready;
  input [63:0]s_ext_axis_tstrb;
  input [7:0]s_ext_axis_tuser;
  input [0:0]s_ext_axis_tvalid;
  output [79:0]write_mem_cmd_tdata;
  input write_mem_cmd_tready;
  output write_mem_cmd_tvalid;
  output [511:0]write_mem_data_tdata;
  output [7:0]write_mem_data_tdest;
  output [7:0]write_mem_data_tid;
  output [63:0]write_mem_data_tkeep;
  output [0:0]write_mem_data_tlast;
  input write_mem_data_tready;
  output [63:0]write_mem_data_tstrb;
  output write_mem_data_tvalid;

  wire [63:0]axi_mem_access_araddr;
  wire [1:0]axi_mem_access_arburst;
  wire [3:0]axi_mem_access_arcache;
  wire [7:0]axi_mem_access_arlen;
  wire [0:0]axi_mem_access_arlock;
  wire [2:0]axi_mem_access_arprot;
  wire [3:0]axi_mem_access_arqos;
  wire axi_mem_access_arready;
  wire [3:0]axi_mem_access_arregion;
  wire [2:0]axi_mem_access_arsize;
  wire axi_mem_access_arvalid;
  wire [63:0]axi_mem_access_awaddr;
  wire [1:0]axi_mem_access_awburst;
  wire [3:0]axi_mem_access_awcache;
  wire [7:0]axi_mem_access_awlen;
  wire [0:0]axi_mem_access_awlock;
  wire [2:0]axi_mem_access_awprot;
  wire [3:0]axi_mem_access_awqos;
  wire axi_mem_access_awready;
  wire [3:0]axi_mem_access_awregion;
  wire [2:0]axi_mem_access_awsize;
  wire axi_mem_access_awvalid;
  wire axi_mem_access_bready;
  wire [1:0]axi_mem_access_bresp;
  wire axi_mem_access_bvalid;
  wire [31:0]axi_mem_access_rdata;
  wire axi_mem_access_rlast;
  wire axi_mem_access_rready;
  wire [1:0]axi_mem_access_rresp;
  wire axi_mem_access_rvalid;
  wire [31:0]axi_mem_access_wdata;
  wire axi_mem_access_wlast;
  wire axi_mem_access_wready;
  wire [3:0]axi_mem_access_wstrb;
  wire axi_mem_access_wvalid;
  wire [511:0]c2h_data_tdata;
  wire [0:0]c2h_data_tid;
  wire [63:0]c2h_data_tkeep;
  wire c2h_data_tlast;
  wire c2h_data_tready;
  wire [63:0]c2h_data_tstrb;
  wire [0:0]c2h_data_tuser;
  wire c2h_data_tvalid;
  wire clk;
  wire [511:0]m_ext_axis_tdata;
  wire [7:0]m_ext_axis_tdest;
  wire [3:0]m_ext_axis_tid;
  wire [63:0]m_ext_axis_tkeep;
  wire [0:0]m_ext_axis_tlast;
  wire [0:0]m_ext_axis_tready;
  wire [63:0]m_ext_axis_tstrb;
  wire [7:0]m_ext_axis_tuser;
  wire [0:0]m_ext_axis_tvalid;
  wire [160:0]prp_out_tdata;
  wire prp_out_tready;
  wire [31:0]prp_out_tuser;
  wire prp_out_tvalid;
  wire [79:0]read_mem_cmd_tdata;
  wire read_mem_cmd_tready;
  wire read_mem_cmd_tvalid;
  wire [511:0]read_mem_data_tdata;
  wire [7:0]read_mem_data_tdest;
  wire [7:0]read_mem_data_tid;
  wire [63:0]read_mem_data_tkeep;
  wire [0:0]read_mem_data_tlast;
  wire read_mem_data_tready;
  wire [63:0]read_mem_data_tstrb;
  wire read_mem_data_tvalid;
  wire resetn;
  wire [15:0]s_axi_Manager_araddr;
  wire [2:0]s_axi_Manager_arprot;
  wire s_axi_Manager_arready;
  wire s_axi_Manager_arvalid;
  wire [15:0]s_axi_Manager_awaddr;
  wire [2:0]s_axi_Manager_awprot;
  wire s_axi_Manager_awready;
  wire s_axi_Manager_awvalid;
  wire s_axi_Manager_bready;
  wire [1:0]s_axi_Manager_bresp;
  wire s_axi_Manager_bvalid;
  wire [31:0]s_axi_Manager_rdata;
  wire s_axi_Manager_rready;
  wire [1:0]s_axi_Manager_rresp;
  wire s_axi_Manager_rvalid;
  wire [31:0]s_axi_Manager_wdata;
  wire s_axi_Manager_wready;
  wire [3:0]s_axi_Manager_wstrb;
  wire s_axi_Manager_wvalid;
  wire [511:0]s_ext_axis_tdata;
  wire [7:0]s_ext_axis_tdest;
  wire [3:0]s_ext_axis_tid;
  wire [63:0]s_ext_axis_tkeep;
  wire [0:0]s_ext_axis_tlast;
  wire [0:0]s_ext_axis_tready;
  wire [63:0]s_ext_axis_tstrb;
  wire [7:0]s_ext_axis_tuser;
  wire [0:0]s_ext_axis_tvalid;
  wire [79:0]write_mem_cmd_tdata;
  wire write_mem_cmd_tready;
  wire write_mem_cmd_tvalid;
  wire [511:0]write_mem_data_tdata;
  wire [7:0]write_mem_data_tdest;
  wire [7:0]write_mem_data_tid;
  wire [63:0]write_mem_data_tkeep;
  wire [0:0]write_mem_data_tlast;
  wire write_mem_data_tready;
  wire [63:0]write_mem_data_tstrb;
  wire write_mem_data_tvalid;

endmodule
