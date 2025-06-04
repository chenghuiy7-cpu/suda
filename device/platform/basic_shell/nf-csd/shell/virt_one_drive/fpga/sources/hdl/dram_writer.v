module dram_writer(
    input aclk,
    input aresetn,

    input user_reset,

    input [127:0] s_axis_req_tdata,
    input s_axis_req_tvalid,
    output s_axis_req_tready,

    input [511:0] s_axis_data_tdata,
    input [63:0] s_axis_data_tkeep,
    input [63:0] s_axis_data_tuser,
    input s_axis_data_tlast,
    input s_axis_data_tvalid,
    output s_axis_data_tready,

    output [511:0] m_axis_s2mm_tdata,
    output [63:0] m_axis_s2mm_tkeep,
    output m_axis_s2mm_tlast,
    output m_axis_s2mm_tvalid,
    input m_axis_s2mm_tready,

    output [79:0] m_axis_s2mm_cmd_tdata,
    output m_axis_s2mm_cmd_tvalid,
    input m_axis_s2mm_cmd_tready,

    input [7:0] s_axis_s2mm_sts_tdata,
    input s_axis_s2mm_sts_tkeep,
    input s_axis_s2mm_sts_tlast,
    input s_axis_s2mm_sts_tvalid,
    output s_axis_s2mm_sts_tready,

    input s2mm_err,

    output reg [63:0] debug_out
);
    wire [39:0] cur_req_addr;
    wire [31:0] cur_req_len;
    wire [7:0] cur_req_pfid;
    wire [7:0] cur_req_csqe_id;
    wire [7:0] cur_req_stream_id;

    wire [23:0] fifo_din;
    wire [23:0] fifo_dout;
    wire fifo_empty;
    wire fifo_full;
    wire fifo_wr_en;
    wire fifo_rd_en;

    wire resetn;
    assign resetn = aresetn & ~user_reset;

    assign fifo_din = {cur_req_pfid, cur_req_csqe_id, cur_req_stream_id};
    assign fifo_wr_en = s_axis_req_tvalid & s_axis_req_tready;
    assign fifo_rd_en = s_axis_data_tready & s_axis_data_tvalid & s_axis_data_tlast;

    fifo #(.DATA_WIDTH(24)) req_fifo(
       .clk(aclk),
       .resetn(resetn),
       .din(fifo_din),
       .wr_en(fifo_wr_en),
       .dout(fifo_dout),
       .rd_en(fifo_rd_en),
       .empty(fifo_empty),
       .full(fifo_full)
    );

    assign cur_req_addr = s_axis_req_tdata[39:0];
    assign cur_req_len = s_axis_req_tdata[71:40];
    assign cur_req_pfid = s_axis_req_tdata[79:72];
    assign cur_req_csqe_id = s_axis_req_tdata[87:80];
    assign cur_req_stream_id = s_axis_req_tdata[95:88];

    assign s_axis_req_tready = ~fifo_full & m_axis_s2mm_cmd_tready;

    assign m_axis_s2mm_cmd_tvalid = s_axis_req_tvalid & s_axis_req_tready;
    assign m_axis_s2mm_cmd_tdata[22:0] = cur_req_len; // Bytes to transfer
    assign m_axis_s2mm_cmd_tdata[23] = 1'b1; // Type (INCR)
    assign m_axis_s2mm_cmd_tdata[29:24] = 0; // DRE Stream alignment, not used
    assign m_axis_s2mm_cmd_tdata[30] = 1'b1; // End of Frame
    assign m_axis_s2mm_cmd_tdata[31] = 1'b0; // DRE ReAlignment Request, not used
    assign m_axis_s2mm_cmd_tdata[71:32] = cur_req_addr; // Start address
    assign m_axis_s2mm_cmd_tdata[79:72] = 0; // Reserved 

    assign s_axis_s2mm_sts_tready = 1'b1;

    assign m_axis_s2mm_tdata = s_axis_data_tdata;
    assign m_axis_s2mm_tkeep = s_axis_data_tkeep;
    assign m_axis_s2mm_tlast = s_axis_data_tlast;
    assign m_axis_s2mm_tvalid = s_axis_data_tvalid;
    assign s_axis_data_tready = m_axis_s2mm_tready;

    always @(posedge aclk) begin
        if (~resetn)
            debug_out <= 64'b0;
        else if (s_axis_data_tvalid & s_axis_data_tready)
            debug_out <= debug_out + 64'b1;
    end

endmodule