module prp_fetcher(
    input aclk,
    input aresetn,
    input user_reset,

    input [160:0] s_axis_prp_fetch_tdata,
    input s_axis_prp_fetch_tvalid,
    input [31:0] s_axis_prp_fetch_tuser,
    output s_axis_prp_fetch_tready,

    output [63:0] m_axis_prp_out_tdata,
    output m_axis_prp_out_tvalid,
    input m_axis_prp_out_tready,

    input [31:0] prp_out_tag,

    output [127:0] m_axis_h2c_byp_in_tdata,
    output         m_axis_h2c_byp_in_tlast,
    input        m_axis_h2c_byp_in_tready,
    output         m_axis_h2c_byp_in_tvalid,
    output [15:0]  m_axis_h2c_byp_in_tkeep,

    input [7:0]  s_axis_h2c_tdest,
    input [511:0]s_axis_h2c_tdata,
    input        s_axis_h2c_tlast,
    output       s_axis_h2c_tready,
    input        s_axis_h2c_tvalid,
    input [63:0] s_axis_h2c_tkeep,
    input [ 7:0] s_axis_h2c_tid,
    input [ 7:0] s_axis_h2c_tuser,

    output reg [2111:0] prp_data,

    output reg [127:0] debug_out
);
    wire [63:0] prp1;
    wire [63:0] prp2;
    wire [23:0] meta;
    wire [7:0] req_len;
    wire prp_should_fetch;
    wire wr_cmd;

    reg [11:0] offset;
    reg [11:0] read_offset;

    reg [31:0] prev_prp_out_tag;

    reg [31:0] stored_prp_tag;
    reg stored_prp_tag_ready;

    wire prp_out_tag_changed = (prp_out_tag != prev_prp_out_tag);
    wire [11:0] prp_read_offset = (prp_out_tag_changed ? 0 : read_offset);

    localparam WAIT_PRP_REQ = 2'b00, WAIT_PRP_DATA = 2'b11;
    reg [1:0] state;

    wire resetn;
    assign resetn = aresetn & ~user_reset;

    assign m_axis_h2c_byp_in_tdata[63:0] = prp2; // Address
    assign m_axis_h2c_byp_in_tdata[72:64] = 9'd17; // QID
    assign m_axis_h2c_byp_in_tdata[74:73] = meta[17:16]; // PFID
    assign m_axis_h2c_byp_in_tdata[77:75] = 3'b0; // port ID
    assign m_axis_h2c_byp_in_tdata[93:78] = 16'd256; // Length
    assign m_axis_h2c_byp_in_tdata[127:94] = 0; // Padding

    assign m_axis_h2c_byp_in_tlast = s_axis_prp_fetch_tvalid;
    assign m_axis_h2c_byp_in_tvalid = s_axis_prp_fetch_tvalid & prp_should_fetch;
    assign m_axis_h2c_byp_in_tkeep = 16'hFFFF;

    assign prp1 = s_axis_prp_fetch_tdata[63:0];
    assign prp2 = s_axis_prp_fetch_tdata[127:64];
    assign meta = s_axis_prp_fetch_tdata[151:128];
    assign req_len = s_axis_prp_fetch_tdata[159:152];
    assign wr_cmd = s_axis_prp_fetch_tdata[160];
    assign prp_should_fetch = req_len > 2;

    always @ (posedge aclk) begin
        if (~resetn) begin
            state <= WAIT_PRP_REQ;
            prp_data <= 0;
            offset <= 0;
            stored_prp_tag <= 0;
            stored_prp_tag_ready <= 0;
            debug_out <= 0;
        end else begin
            case (state)
                WAIT_PRP_REQ: begin
                    if (s_axis_prp_fetch_tvalid) begin
                        if (!prp_should_fetch) begin
                            // Directly record PRP tag
                            state <= WAIT_PRP_REQ;
                            stored_prp_tag_ready <= 1;
                        end else if (m_axis_h2c_byp_in_tvalid & m_axis_h2c_byp_in_tready) begin
                            // Ask QDMA for PRP2 and later
                            state <= WAIT_PRP_DATA;
                            offset <= 64;
                            stored_prp_tag_ready <= 0;
                            debug_out[95:0] <= m_axis_h2c_byp_in_tdata[95:0];
                            debug_out[111:96] <= debug_out[111:96] + 1;
                        end
                        prp_data[63:0] <= prp1;
                        prp_data[127:64] <= prp2;
                        stored_prp_tag <= {8'b0, meta};
                    end
                end
                WAIT_PRP_DATA: begin
                    if (s_axis_h2c_tvalid & s_axis_h2c_tready) begin
                        prp_data[offset +: 512] <= s_axis_h2c_tdata;
                        offset <= offset + 512;
                        debug_out[127:112] <= debug_out[127:112] + 1;
                        if (s_axis_h2c_tlast) begin
                            stored_prp_tag_ready <= 1;
                            state <= WAIT_PRP_REQ;
                        end
                    end
                end
                default: begin
                    state <= WAIT_PRP_REQ;
                end
            endcase
        end
    end

    always @ (posedge aclk) begin
        if (~resetn | prp_out_tag_changed) begin
            read_offset <= 0;
        end else if (m_axis_prp_out_tready & m_axis_prp_out_tvalid) begin
            read_offset <= (prp_out_tag_changed ? 0 : read_offset) + 64;
        end
    end

    always @ (posedge aclk) begin
        if (~resetn) begin
            prev_prp_out_tag <= 0;
        end else begin
            prev_prp_out_tag <= prp_out_tag;
        end
    end

    assign s_axis_prp_fetch_tready = aresetn & (state == WAIT_PRP_REQ) & (prp_should_fetch ? m_axis_h2c_byp_in_tready : 1'b1);
    assign s_axis_h2c_tready = resetn & (state == WAIT_PRP_DATA);

    assign m_axis_prp_out_tdata = prp_data[read_offset +: 64];
    assign m_axis_prp_out_tvalid = resetn & (prp_out_tag == stored_prp_tag) & stored_prp_tag_ready;

endmodule