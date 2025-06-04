module compute_c2h_merger(
    input  aclk,
    input  aresetn,
    input user_reset,

    input [63:0] s_axis_prp_out_tdata,
    input s_axis_prp_out_tvalid,
    output s_axis_prp_out_tready,

    output [31:0] prp_out_tag,

    input [3:0]   s_axis_c2h_tdest,
    input [511:0] s_axis_c2h_tdata,
    input         s_axis_c2h_tlast,
    output        s_axis_c2h_tready,
    input         s_axis_c2h_tvalid,
    input [63:0]  s_axis_c2h_tkeep,
    input [7:0]   s_axis_c2h_tid,
    input [63:0]  s_axis_c2h_tuser,

    output [3:0]   m_axis_c2h_tdest,
    output [511:0] m_axis_c2h_tdata,
    output         m_axis_c2h_tlast,
    input          m_axis_c2h_tready,
    output         m_axis_c2h_tvalid,
    output [63:0]  m_axis_c2h_tkeep,
    output [7:0]   m_axis_c2h_tid,
    output [63:0]  m_axis_c2h_tuser,

    output [63:0] merger_state
);

    wire [511:0] aw_tdata;
    wire resetn;

    assign resetn = aresetn & ~user_reset;

    assign aw_tdata[63:0]  = s_axis_prp_out_tdata;
    assign aw_tdata[66:64]  = 3'd4; // XDMA uses 2^4 = 16 bytes = 128 bits data width
    assign aw_tdata[74:67]  = 8'd255; // 256 beats per packet
    assign aw_tdata[78:75]  = 4'b0; // AWID
    assign aw_tdata[80:79]  = 2'b0; // Driver ID
    assign aw_tdata[511:81] = 0; // Reserved

    // For each 4KB in s_axis_c2h_tdata, form a new packet as m_axis_c2h_tdata and insert aw_tdata in the first beat
    localparam IDLE = 2'b00, SEND_AW = 2'b01, SEND_W = 2'b10;
    reg [5:0] offset;
    reg [1:0] state;
    reg [4:0] cur_req_qid;

    reg [15:0] packets_sent;

    always @(posedge aclk) begin
        if (!resetn) begin
            offset <= 0;
            state <= IDLE;
            packets_sent <= 0;
            cur_req_qid <= 0;
        end else begin
            case (state)
                IDLE: begin
                    if (s_axis_c2h_tvalid) begin
                        state <= SEND_AW;
                    end else begin
                        state <= IDLE;
                    end
                end
                SEND_AW: begin
                    if (m_axis_c2h_tvalid & m_axis_c2h_tready) begin
                        state <= SEND_W;
                        cur_req_qid <= s_axis_prp_out_tdata[62:58];
                    end
                end
                SEND_W: begin
                    if (m_axis_c2h_tvalid & m_axis_c2h_tready) begin
                        if (s_axis_c2h_tlast) begin
                            state <= IDLE;
                            offset <= 0;
                            packets_sent <= packets_sent + 1;
                        end else if (&offset) begin
                            state <= SEND_AW;
                            offset <= 0;
                            packets_sent <= packets_sent + 1;
                        end else begin
                            offset <= offset + 1;
                        end
                    end
                end
            endcase
        end
    end
            
    assign prp_out_tag = {8'b0, s_axis_c2h_tuser[23:0]};

    assign m_axis_c2h_tdest = 4'b0;
    assign m_axis_c2h_tdata = state == SEND_W ? s_axis_c2h_tdata : aw_tdata;
    assign m_axis_c2h_tlast = s_axis_c2h_tlast | &offset;
    assign m_axis_c2h_tkeep = s_axis_c2h_tkeep;
    assign m_axis_c2h_tid = {3'b0, cur_req_qid} + 8'd8;
    assign m_axis_c2h_tuser = 64'd4096;

    assign s_axis_c2h_tready = resetn & (state == SEND_W) & m_axis_c2h_tready;
    assign m_axis_c2h_tvalid = resetn & (state == SEND_AW ? s_axis_prp_out_tvalid :
            state == SEND_W  ? s_axis_c2h_tvalid : 0);

    assign s_axis_prp_out_tready = resetn & state == SEND_AW & m_axis_c2h_tready;

    assign merger_state = {state, packets_sent};

endmodule
