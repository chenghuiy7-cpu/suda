module axis_ic_connector (
    input clk,
    input rstn,

    // From AXI-Stream input IC
	input [95:0]    s_axis_ic_tdata,
	input           s_axis_ic_tlast,
	input [ 2:0]    s_axis_ic_tdest,
	input [11:0]    s_axis_ic_tkeep,
	input           s_axis_ic_tvalid,
	output          s_axis_ic_tready,

    // To AXI-Stream output IC
	output [95:0]   m_axis_ic_tdata,
	output          m_axis_ic_tlast,
	output [ 2:0]   m_axis_ic_tdest,
	output [11:0]   m_axis_ic_tkeep,
	output          m_axis_ic_tvalid,
	input           m_axis_ic_tready,

    output          tvalid_in,
    input           ar_req_gating,
    output [1:0]    drive_id,
    output          tready,
    output          tlast,

    output reg [31:0] ar_pkt_cnt,
    output reg [31:0] ar_cycle_cnt
);

    assign m_axis_ic_tdata = s_axis_ic_tdata;
    assign m_axis_ic_tlast = s_axis_ic_tlast;
    assign m_axis_ic_tdest = s_axis_ic_tdest;
    assign m_axis_ic_tkeep = s_axis_ic_tkeep;
    assign m_axis_ic_tvalid = s_axis_ic_tvalid & ~ar_req_gating;
    assign s_axis_ic_tready = m_axis_ic_tready & ~ar_req_gating;

    assign tvalid_in = s_axis_ic_tvalid;
    assign tready = s_axis_ic_tready;
    assign tlast = s_axis_ic_tlast;
    assign drive_id = s_axis_ic_tdata[80:79];

    always @ (posedge clk)
    begin
        if (~rstn)
            ar_pkt_cnt <= 32'b0;
        else if (s_axis_ic_tvalid & s_axis_ic_tready & s_axis_ic_tlast)
            ar_pkt_cnt <= ar_pkt_cnt + 32'b1;
    end

    always @ (posedge clk) begin
        if (~rstn)
            ar_cycle_cnt <= 32'b0;
        else if (ar_pkt_cnt == 32'b0)
            ar_cycle_cnt <= ar_cycle_cnt + 32'b1;
    end

endmodule