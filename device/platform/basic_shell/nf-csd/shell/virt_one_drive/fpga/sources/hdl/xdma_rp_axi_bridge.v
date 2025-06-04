module xdma_rp_axi_bridge (
    s_axib_aclk,
    s_axib_aresetn,

	drive_id,
        
    // AXI slave I/F attached to PCIe RP port
	s_axib_awid,
	s_axib_awaddr,
	s_axib_awlen,
	s_axib_awsize,
	s_axib_awburst,
	s_axib_awprot,
	s_axib_awvalid,
	s_axib_awready,
	s_axib_awlock,
	s_axib_awcache,
	s_axib_wdata,
	s_axib_wstrb,
	s_axib_wlast,
	s_axib_wvalid,
	s_axib_wready,
	s_axib_bid,
	s_axib_bresp,
	s_axib_bvalid,
	s_axib_bready,
	s_axib_arid,
	s_axib_araddr,
	s_axib_arlen,
	s_axib_arsize,
	s_axib_arburst,
	s_axib_arprot,
	s_axib_arvalid,
	s_axib_arready,
	s_axib_arlock,
	s_axib_arcache,
	s_axib_rid,
	s_axib_rdata,
	s_axib_rresp,
	s_axib_rlast,
	s_axib_rvalid,
	s_axib_rready,

    //AXI-Stream slave I/F attached to PCIe EP H2C port
    s_axis_h2c_tdata,
	s_axis_h2c_tlast,
	s_axis_h2c_tuser,
	s_axis_h2c_tkeep,
	s_axis_h2c_tvalid,
	s_axis_h2c_tready,

	//AXI-Stream master I/F for AR-to-stream request output
	m_axis_ar_req_tdata,
	m_axis_ar_req_tlast,
	m_axis_ar_req_tdest,
	m_axis_ar_req_tkeep,
	m_axis_ar_req_tvalid,
	m_axis_ar_req_tready,

	m_axis_aw_req_tdata,
	m_axis_aw_req_tlast,
	m_axis_aw_req_tdest,
	m_axis_aw_req_tid,
	m_axis_aw_req_tkeep,
	m_axis_aw_req_tvalid,
	m_axis_aw_req_tready,

	m_axis_w_tdata,
	m_axis_w_tlast,
	m_axis_w_tdest,
	m_axis_w_tid,
	m_axis_w_tuser,
	m_axis_w_tkeep,
	m_axis_w_tvalid,
	m_axis_w_tready,

	len_fifo_rd_en,
	len_fifo_rd_data,
	len_fifo_wr_en,
	len_fifo_wr_data,
	len_fifo_full,
	len_fifo_empty,

	wid_fifo_rd_en,
	wid_fifo_rd_data,
	wid_fifo_wr_en,
	wid_fifo_wr_data,
	wid_fifo_full,
	wid_fifo_empty,

	aw_pkt_cnt,
	w_pkt_cnt,
	ar_pkt_cnt,
	r_pkt_cnt,
	b_pkt_cnt
);

input s_axib_aclk;
input s_axib_aresetn;
input [1:0] drive_id;

// AXI slave interface to PCIe RP port
input  [ 3 : 0]                     s_axib_awid;
input  [63 : 0]                     s_axib_awaddr;
input  [ 7 : 0]                     s_axib_awlen;
input  [ 2 : 0]                     s_axib_awsize;
input  [ 1 : 0]                     s_axib_awburst;
input  [ 2 : 0]                     s_axib_awprot;
input                               s_axib_awvalid;
output                              s_axib_awready;
input                               s_axib_awlock;
input  [  3 : 0]                    s_axib_awcache;
input  [127 : 0]                    s_axib_wdata ;
input  [ 15 : 0]                    s_axib_wstrb ;
input                               s_axib_wlast ;
input                               s_axib_wvalid ;
output                              s_axib_wready ;
output  [3 : 0]                     s_axib_bid ;
output  [1 : 0]                     s_axib_bresp ;
output                              s_axib_bvalid ;
input                               s_axib_bready ;
input  [3 : 0]                      s_axib_arid ;
input  [63 : 0]                     s_axib_araddr ;
input  [7 : 0]                      s_axib_arlen ;
input  [2 : 0]                      s_axib_arsize ;
input  [1 : 0]                      s_axib_arburst ;
input  [2 : 0]                      s_axib_arprot ;
input                               s_axib_arvalid ;
output                              s_axib_arready ;
input                               s_axib_arlock;
input  [  3 : 0]                    s_axib_arcache;
output [  3 : 0]                    s_axib_rid ;
output [127 : 0]                    s_axib_rdata ;
output [  1 : 0]                    s_axib_rresp ;
output                              s_axib_rlast ;
output                              s_axib_rvalid ;
input                               s_axib_rready ;

//AXI-Stream slave I/F attached to PCIe EP H2C port
input   [127 : 0]                   s_axis_h2c_tdata;
input                               s_axis_h2c_tlast;
input   [  2 : 0]                   s_axis_h2c_tuser;
input   [ 15 : 0]                   s_axis_h2c_tkeep;
input                               s_axis_h2c_tvalid;
output                              s_axis_h2c_tready;

//AXI-Stream master I/F for AR-to-AXIS request output
output  [ 95 : 0]                   m_axis_ar_req_tdata;
output                              m_axis_ar_req_tlast;
output  [  7 : 0]                   m_axis_ar_req_tdest;
output  [ 11 : 0]                   m_axis_ar_req_tkeep;
output                              m_axis_ar_req_tvalid;
input                               m_axis_ar_req_tready;

output [95:0] 						m_axis_aw_req_tdata;
output 								m_axis_aw_req_tlast;
output [7:0] 						m_axis_aw_req_tdest;
output [7:0] 						m_axis_aw_req_tid;
output [11:0] 						m_axis_aw_req_tkeep;
output 								m_axis_aw_req_tvalid;
input 								m_axis_aw_req_tready;

output [127:0] 						m_axis_w_tdata;
output 								m_axis_w_tlast;
output [7:0] 						m_axis_w_tdest;
output [7:0] 						m_axis_w_tid;
output [15:0] 						m_axis_w_tkeep;
output [15:0] 						m_axis_w_tuser;
output 								m_axis_w_tvalid;
input 								m_axis_w_tready;

output								len_fifo_rd_en;
input [24:0]						len_fifo_rd_data;
output								len_fifo_wr_en;
output  [24:0]						len_fifo_wr_data;
input								len_fifo_full;
input								len_fifo_empty;

output								wid_fifo_rd_en;
input [3:0]							wid_fifo_rd_data;
output								wid_fifo_wr_en;
output  [3:0]						wid_fifo_wr_data;
input								wid_fifo_full;
input								wid_fifo_empty;

output reg [31:0]					aw_pkt_cnt;
output reg [31:0]					w_pkt_cnt;
output reg [31:0]					ar_pkt_cnt;
output reg [31:0]					r_pkt_cnt;
output reg [31:0]					b_pkt_cnt;


// AXI-Stream slave port to R channel of AXI slave port 
// H2C-to-R de-packetizer
wire    [127 : 0]                   s_axis_h2c_tdata_mask;
wire    [  8 : 0]   				c2h_burst_len;

reg									next_w_pkt_is_new;
wire								new_w_pkt;

wire								aw_pkt_should_send;
wire								w_pkt_should_send;

reg	 [24:0]							cur_w_len;
wire [24:0]							len_and_awid;

assign c2h_burst_len = {1'b0, s_axib_awlen} + 9'd1;

assign s_axis_h2c_tdata_mask = {{8{s_axis_h2c_tkeep[15]}}, 
                                {8{s_axis_h2c_tkeep[14]}},
                                {8{s_axis_h2c_tkeep[13]}},
                                {8{s_axis_h2c_tkeep[12]}},
                                {8{s_axis_h2c_tkeep[11]}},
                                {8{s_axis_h2c_tkeep[10]}},
                                {8{s_axis_h2c_tkeep[9]}},
                                {8{s_axis_h2c_tkeep[8]}},
                                {8{s_axis_h2c_tkeep[7]}},
                                {8{s_axis_h2c_tkeep[6]}},
                                {8{s_axis_h2c_tkeep[5]}},
                                {8{s_axis_h2c_tkeep[4]}},
                                {8{s_axis_h2c_tkeep[3]}},
                                {8{s_axis_h2c_tkeep[2]}},
                                {8{s_axis_h2c_tkeep[1]}},
                                {8{s_axis_h2c_tkeep[0]}}};

assign s_axis_h2c_tready = s_axib_rready;
assign s_axib_rlast      = s_axis_h2c_tlast;
assign s_axib_rresp      = 2'b00;
assign s_axib_rvalid     = s_axis_h2c_tvalid;
assign s_axib_rdata      = s_axis_h2c_tdata & s_axis_h2c_tdata_mask;
assign s_axib_rid        = 4'b0;

/* Encapsulation of an AR request as a AXI-Stream packet
 * 96-bit length single-beat packet
 * 95:78 Reserved
 * 77:75 3-bit ARID
 * 74:67 8-bit AR burst length
 * 66:64 3-bit AR burst size (4, indicating 128-bit)
 * 63: 0 64-bit AR address
 * 62:61 2-bit FN id
 * 60:58 3-bit QID
 * 57:48 10-bit CID (1024 commands)
 * 47:0 48-bit physical addr
 */
assign m_axis_ar_req_tdata[63:0]  = s_axib_araddr;
assign m_axis_ar_req_tdata[66:64]  = s_axib_arsize;
assign m_axis_ar_req_tdata[74:67]  = s_axib_arlen;
assign m_axis_ar_req_tdata[78:75]  = s_axib_arid;
assign m_axis_ar_req_tdata[80:79]  = drive_id;
assign m_axis_ar_req_tlast  = s_axib_arvalid;
assign m_axis_ar_req_tdest  = {5'b0, s_axib_arid[2:0]};
assign m_axis_ar_req_tkeep  = 12'hFFF;
assign m_axis_ar_req_tvalid = s_axib_arvalid;
assign s_axib_arready       = m_axis_ar_req_tready;

assign m_axis_aw_req_tdata[63:0]  = s_axib_awaddr;
assign m_axis_aw_req_tdata[66:64]  = s_axib_awsize;
assign m_axis_aw_req_tdata[74:67]  = s_axib_awlen;
assign m_axis_aw_req_tdata[78:75]  = s_axib_awid;
assign m_axis_aw_req_tdata[80:79]  = drive_id;
assign m_axis_aw_req_tlast  = s_axib_awvalid;
assign m_axis_aw_req_tdest  = 8'h1F;
assign m_axis_aw_req_tid  = 8'h21;
assign m_axis_aw_req_tkeep  = 12'hFFF;
assign m_axis_aw_req_tvalid = s_axib_awvalid & aw_pkt_should_send;
assign s_axib_awready       = m_axis_aw_req_tready & aw_pkt_should_send;

assign len_and_awid = new_w_pkt ? len_fifo_rd_data : cur_w_len;

assign m_axis_w_tdata  = s_axib_wdata;
assign m_axis_w_tlast  = s_axib_wlast;
assign m_axis_w_tdest  = 8'h1F;
assign m_axis_w_tid    = {1'b0, len_and_awid[24:23], 2'b0, len_and_awid[22:20]} + 8'd8;
assign m_axis_w_tuser  = len_and_awid[15:0];
assign m_axis_w_tkeep  = s_axib_wstrb;
assign m_axis_w_tvalid = s_axib_wvalid & w_pkt_should_send;
assign s_axib_wready   = m_axis_w_tready & w_pkt_should_send;

assign aw_pkt_should_send = ~len_fifo_full;
assign w_pkt_should_send = ~(next_w_pkt_is_new & (len_fifo_empty | wid_fifo_full));

assign new_w_pkt = next_w_pkt_is_new & s_axib_wready & m_axis_w_tvalid;

assign len_fifo_rd_en = new_w_pkt;
assign len_fifo_wr_en = m_axis_aw_req_tvalid & m_axis_aw_req_tready;
assign len_fifo_wr_data[15:0] = {3'b0, c2h_burst_len, 4'b0};
assign len_fifo_wr_data[19:16] = s_axib_awid;
assign len_fifo_wr_data[24:20] = s_axib_awaddr[62:58];

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		next_w_pkt_is_new <= 1'b1;
	end else if (s_axib_wready & m_axis_w_tvalid) begin
		next_w_pkt_is_new <= m_axis_w_tlast;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		cur_w_len <= 16'b0;
	end else if (new_w_pkt) begin
		cur_w_len <= len_fifo_rd_data;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		aw_pkt_cnt <= 0;
	end else if (s_axib_awvalid & s_axib_awready) begin
		aw_pkt_cnt <= aw_pkt_cnt + 1;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		w_pkt_cnt <= 0;
	end else if (s_axib_wvalid & s_axib_wready & s_axib_wlast) begin
		w_pkt_cnt <= w_pkt_cnt + 1;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		ar_pkt_cnt <= 0;
	end else if (s_axib_arvalid & s_axib_arready) begin
		ar_pkt_cnt <= ar_pkt_cnt + 1;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		r_pkt_cnt <= 0;
	end else if (s_axib_rvalid & s_axib_rready & s_axib_rlast) begin
		r_pkt_cnt <= r_pkt_cnt + 1;
	end
end

always @ (posedge s_axib_aclk) begin
	if (~s_axib_aresetn) begin
		b_pkt_cnt <= 0;
	end else if (s_axib_bvalid & s_axib_bready) begin
		b_pkt_cnt <= b_pkt_cnt + 1;
	end
end

assign s_axib_bid = wid_fifo_rd_data;
assign s_axib_bvalid = ~wid_fifo_empty;
assign s_axib_bresp = 2'b0;
assign wid_fifo_wr_en = m_axis_w_tready & m_axis_w_tvalid & m_axis_w_tlast & ~wid_fifo_full;
assign wid_fifo_wr_data = len_and_awid[19:16];
assign wid_fifo_rd_en = s_axib_bready & s_axib_bvalid;

endmodule
