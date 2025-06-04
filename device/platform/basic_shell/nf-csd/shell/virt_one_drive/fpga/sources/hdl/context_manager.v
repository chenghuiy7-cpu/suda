module context_manager (
    input aclk,
    input aresetn,

    // AXI Stream input interface
    input [511:0] s_axis_tdata,
    input [63:0] s_axis_tkeep,
    input [63:0] s_axis_tuser,
    input [3:0] s_axis_tdest,
    input s_axis_tlast,
    input s_axis_tvalid,
    output s_axis_tready,
    // AXI Stream output interface
    output [511:0] m_axis_tdata,
    output [63:0] m_axis_tkeep,
    output [63:0] m_axis_tuser,
    output [3:0] m_axis_tdest,
    output m_axis_tlast,
    output m_axis_tvalid,
    input m_axis_tready,

    input [1023:0] current_sqe,

    output sqe_finished,

    output reg [31:0] count
);

    localparam QDMA_TDEST = 4'd0, DRAM_TDEST = 4'd4;

    wire [2:0] current_stream_idx, current_op_idx;
    wire [2:0] next_stream_idx, next_op_idx;
    wire [9:0] current_stream_offset, current_op_offset;
    wire [3:0] current_op;
    wire is_last_op;
    wire is_last_stream;
    wire should_next_stream;
    wire should_output;

    wire [2:0] sqe_num_streams;
    wire [63:0] next_stream_prp1;

    assign sqe_num_streams = current_sqe[23:16];
    assign next_stream_prp1 = current_sqe[(current_stream_offset + 192 +64) +: 64];

    assign current_stream_idx = s_axis_tuser[30:28];
    assign current_op_idx = s_axis_tuser[26:24];

    assign current_stream_offset = 10'd64 + {1'b0, current_stream_idx, 6'b0} + {current_stream_idx, 7'b0}; // 64 + 192 * current_stream_idx
    assign current_op_offset = current_stream_offset + {current_op_idx, 2'b00};

    assign current_op = current_sqe[current_op_offset +: 4];
    assign is_last_op = (current_op_idx == 3'b111) | !current_op;
    assign is_last_stream = (current_stream_idx + 2 == sqe_num_streams);
    assign should_next_stream = is_last_op & ~is_last_stream;
    assign should_output = is_last_op & is_last_stream;

    assign sqe_finished = should_output & m_axis_tlast & m_axis_tvalid & m_axis_tready;

    assign next_stream_idx = should_next_stream ? (current_stream_idx + 1) : current_stream_idx;
    assign next_op_idx = should_next_stream ? 0 : (current_op_idx + 1);

    assign m_axis_tdest = should_output ? (next_stream_prp1[63] ? QDMA_TDEST : DRAM_TDEST) : current_op;
    assign m_axis_tdata = s_axis_tdata;
    assign m_axis_tkeep = s_axis_tkeep;
    assign m_axis_tuser[63:32] = 32'b0;
    assign m_axis_tuser[31:28] = {1'b0, next_stream_idx};
    assign m_axis_tuser[27:24] = {1'b0, next_op_idx};
    assign m_axis_tuser[23:0] = s_axis_tuser[23:0];
    assign m_axis_tlast = s_axis_tlast;
    assign m_axis_tvalid = s_axis_tvalid;
    assign s_axis_tready = m_axis_tready;

    always @(posedge aclk) begin
        if (!aresetn) begin
            count <= 0;
        end else if (s_axis_tready & s_axis_tvalid) begin
            count <= count + 1;
        end
    end

endmodule