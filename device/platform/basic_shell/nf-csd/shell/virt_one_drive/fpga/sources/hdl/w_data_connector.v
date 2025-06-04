module w_data_connector (
    input           aclk,
    input           aresetn,

    // From W data
	input [511:0]    s_axis_tdata,
	input           s_axis_tlast,
	input [ 7:0]    s_axis_tdest,
	input [ 7:0]   	s_axis_tid,
	input [63:0]    s_axis_tuser,
	input [63:0]    s_axis_tkeep,
	input           s_axis_tvalid,
	output          s_axis_tready,

    // To C2H data AXIS IC
	output [511:0]  m_axis_tdata,
	output          m_axis_tlast,
	output [ 7:0]   m_axis_tdest,
	output [ 7:0]   m_axis_tid,
	output [63:0]   m_axis_tuser,
	output [63:0]   m_axis_tkeep,
	output          m_axis_tvalid,
	input           m_axis_tready,

	output reg [31:0] w_pkt_cnt
);

    wire [1:0] drive_id;

    assign drive_id = s_axis_tdata[80:79];

    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tdest = s_axis_tdest;
    assign m_axis_tid = s_axis_tid;
    assign m_axis_tuser = s_axis_tuser;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

    always @(posedge aclk) begin
        if (~aresetn) begin
            w_pkt_cnt <= 32'b0;
        end else if (m_axis_tready & m_axis_tvalid & m_axis_tlast) begin
            w_pkt_cnt <= w_pkt_cnt + 32'b1;
        end
    end

endmodule