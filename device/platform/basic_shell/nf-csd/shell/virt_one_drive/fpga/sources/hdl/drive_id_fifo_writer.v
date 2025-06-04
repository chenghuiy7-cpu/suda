module drive_id_fifo_writer (
    input                               aclk,
    input                               aresetn,

    input [95:0] 						s_axis_aw_req_tdata,
    input 								s_axis_aw_req_tlast,
    input [7:0] 						s_axis_aw_req_tdest,
    input [7:0] 						s_axis_aw_req_tid,
    input [11:0] 						s_axis_aw_req_tkeep,
    input 								s_axis_aw_req_tvalid,
    output 								s_axis_aw_req_tready,

    output [95:0] 						m_axis_aw_req_tdata,
    output 								m_axis_aw_req_tlast,
    output [7:0] 						m_axis_aw_req_tdest,
    output [7:0] 						m_axis_aw_req_tid,
    output [11:0] 						m_axis_aw_req_tkeep,
    output 								m_axis_aw_req_tvalid,
    input 								m_axis_aw_req_tready,

    output reg [31:0]                   aw_pkt_cnt,

    output                              fifo_wr_en,
    output [1:0]                        fifo_wr_data,
    input                               fifo_full
);
    wire [1:0] drive_id;

    assign drive_id = m_axis_aw_req_tdata[80:79];

    assign fifo_wr_en = m_axis_aw_req_tready & m_axis_aw_req_tvalid;
    assign fifo_wr_data = drive_id;

    assign m_axis_aw_req_tdata = s_axis_aw_req_tdata;
    assign m_axis_aw_req_tlast = s_axis_aw_req_tlast;
    assign m_axis_aw_req_tdest = s_axis_aw_req_tdest;
    assign m_axis_aw_req_tid = s_axis_aw_req_tid;
    assign m_axis_aw_req_tkeep = s_axis_aw_req_tkeep;
    assign m_axis_aw_req_tvalid = s_axis_aw_req_tvalid & ~fifo_full;
    assign s_axis_aw_req_tready = m_axis_aw_req_tready & ~fifo_full;

    always @(posedge aclk) begin
        if (~aresetn) begin
            aw_pkt_cnt <= 32'b0;
        end else if (m_axis_aw_req_tready & m_axis_aw_req_tvalid & m_axis_aw_req_tlast) begin
            aw_pkt_cnt <= aw_pkt_cnt + 32'b1;
        end
    end
    
endmodule