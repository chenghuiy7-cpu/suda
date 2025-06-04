module axis_aw_w_splitter (
    input           aclk,
    input           aresetn,

    // From W switch
	input [511:0]   s_axis_tdata,
	input           s_axis_tlast,
	input [ 7:0]    s_axis_tdest,
	input [ 7:0]   	s_axis_tid,
	input [63:0]    s_axis_tuser,
	input [63:0]    s_axis_tkeep,
	input           s_axis_tvalid,
	output          s_axis_tready,

    // To ar_to_bd_pktizer
    output  [95:0]   m_axis_ar_req_tdata,
    output           m_axis_ar_req_tlast,
    output  [11:0]   m_axis_ar_req_tkeep,
    output           m_axis_ar_req_tvalid,
    input            m_axis_ar_req_tready,

    // To C2H data AXIS IC
	output [511:0]  m_axis_tdata,
	output          m_axis_tlast,
	output [ 7:0]   m_axis_tdest,
	output [ 7:0]   m_axis_tid,
	output [63:0]   m_axis_tuser,
	output [63:0]   m_axis_tkeep,
	output          m_axis_tvalid,
	input           m_axis_tready
);

    reg send_to_sw;

    always @(posedge aclk) begin
        if (~aresetn) begin
            send_to_sw <= 1'b1;
        end else if (s_axis_tready & s_axis_tvalid) begin
            send_to_sw <= s_axis_tlast;
        end
    end

    assign m_axis_ar_req_tdata = s_axis_tdata[95:0];
    assign m_axis_ar_req_tlast = m_axis_ar_req_tvalid;
    assign m_axis_ar_req_tkeep = s_axis_tkeep[11:0];
    assign m_axis_ar_req_tvalid = s_axis_tvalid & send_to_sw;

    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tdest = s_axis_tdest;
    assign m_axis_tid = s_axis_tid;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tvalid = s_axis_tvalid & ~send_to_sw;

    assign s_axis_tready = send_to_sw ? m_axis_ar_req_tready : m_axis_tready;
    
endmodule
