module exec_engine(
    input aclk,
    input aresetn,

    input user_reset,

    output [127:0] m_axis_mem_rd_req_tdata,
    output m_axis_mem_rd_req_tvalid,
    input m_axis_mem_rd_req_tready,

    output [127:0] m_axis_mem_wr_req_tdata,
    output m_axis_mem_wr_req_tvalid,
    input m_axis_mem_wr_req_tready,

    output [160:0] m_axis_prp_fetch_tdata,
    output [31:0] m_axis_prp_fetch_tuser,
    output m_axis_prp_fetch_tvalid,
    input m_axis_prp_fetch_tready,

    input [1023:0] s_axis_sqe_tdata,
    input s_axis_sqe_tvalid,
    output s_axis_sqe_tready,

    output [127:0] m_axis_cqe_tdata,
    output m_axis_cqe_tvalid,
    input m_axis_cqe_tready,

    output [64:0] exec_state,
    input [3:0] sqe_finished,

    output reg [1023:0] current_sqe

);

    wire [1023:0] fifo_din;
    wire [1023:0] fifo_dout;
    wire fifo_empty;
    wire fifo_full;
    wire fifo_wr_en;
    wire fifo_rd_en;
    wire [7:0] pfid;
    wire [7:0] csqe_id;
    wire [7:0] ring_id;
    wire [7:0] num_streams;

    localparam WAIT_CMD = 4'b0000, ANALYSIS = 4'b0001, READ_DRAM = 4'b0010, FETCH_PRP = 4'b0011, 
               PREPARE = 4'b0100, EXECUTE = 4'b0101, SEND_CQE = 4'b0111, WRITE_DRAM = 4'b1000;
               
    reg [3:0] state;

    reg [2:0] stream_idx;
    reg [9:0] stream_offset;
    wire [191:0] stream;
    wire [31:0] stream_op;
    wire [15:0] stream_len_blocks;
    wire [7:0] stream_input1;
    wire [7:0] stream_input2;
    wire [63:0] stream_prp1;
    wire [63:0] stream_prp2;

    wire stream_input1_empty;
    wire stream_input2_empty;
    wire stream_prp1_empty;
    wire stream_prp2_empty;
    wire stream_prp1_from_host;

    reg [3:0] compute_state;
    always @ (posedge aclk) begin
        if (~aresetn)
            compute_state <= 4'b0;
        else
            compute_state <= compute_state | sqe_finished; 
    end

    reg check_reader_valid;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_reader_valid <= 1'b0;
        else if (m_axis_mem_rd_req_tvalid)
            check_reader_valid <= 1'b1; 
    end

    reg check_reader_ready;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_reader_ready <= 1'b0;
        else if (m_axis_mem_rd_req_tready)
            check_reader_ready <= 1'b1; 
    end

    reg check_reader_complete;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_reader_complete <= 1'b0;
        else if (m_axis_mem_rd_req_tvalid & m_axis_mem_rd_req_tready)
            check_reader_complete <= 1'b1; 
    end
    
    reg check_writer_valid;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_writer_valid <= 1'b0;
        else if (m_axis_mem_rd_req_tvalid)
            check_writer_valid <= 1'b1; 
    end

    reg check_writer_ready;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_writer_ready <= 1'b0;
        else if (m_axis_mem_rd_req_tready)
            check_writer_ready <= 1'b1; 
    end

    reg check_writer_complete;
    always @ (posedge aclk) begin
        if (~aresetn)
            check_writer_complete <= 1'b0;
        else if (m_axis_mem_rd_req_tvalid & m_axis_mem_rd_req_tready)
            check_writer_complete <= 1'b1; 
    end

    wire resetn;
    assign resetn = aresetn & ~user_reset;

    assign stream = current_sqe[stream_offset +: 192];

    assign stream_op = stream[31:0];
    assign stream_len_blocks = stream[47:32];
    assign stream_input1 = stream[55:48];
    assign stream_input2 = stream[63:56];
    assign stream_prp1 = stream[127:64];
    assign stream_prp2 = stream[191:128];

    assign stream_input1_empty = (stream_input1 == 8'hFF);
    assign stream_input2_empty = (stream_input2 == 8'hFF);
    assign stream_prp1_empty = (stream_prp1 == 0);
    assign stream_prp2_empty = (stream_prp2 == 0);
    assign stream_prp1_from_host = stream_prp1[63];

    assign fifo_din = s_axis_sqe_tdata;
    assign fifo_wr_en = s_axis_sqe_tvalid & s_axis_sqe_tready;
    assign fifo_rd_en = state == WAIT_CMD && ~fifo_empty;

    assign s_axis_sqe_tready = ~fifo_full;
    assign m_axis_cqe_tvalid = state == SEND_CQE;
    assign m_axis_cqe_tdata = {96'b0, current_sqe[31:0]};

    assign pfid = current_sqe[7:0];
    assign csqe_id = current_sqe[15:8];
    assign num_streams = current_sqe[23:16];
    assign ring_id = current_sqe[31:24];

    assign m_axis_mem_rd_req_tvalid = resetn & state == READ_DRAM;
    assign m_axis_mem_rd_req_tdata[39:0] = stream_prp1[39:0];
    assign m_axis_mem_rd_req_tdata[71:40] = {4'b0, stream_len_blocks, 12'b0}; // 4KB per block
    assign m_axis_mem_rd_req_tdata[79:72] = pfid;
    assign m_axis_mem_rd_req_tdata[87:80] = csqe_id;
    assign m_axis_mem_rd_req_tdata[95:88] = ring_id;
    assign m_axis_mem_rd_req_tdata[127:96] = 32'b0;

    assign m_axis_mem_wr_req_tvalid = resetn & state == WRITE_DRAM;
    assign m_axis_mem_wr_req_tdata[39:0] = stream_prp1[39:0];
    assign m_axis_mem_wr_req_tdata[71:40] = {4'b0, stream_len_blocks, 12'b0}; // 4KB per block
    assign m_axis_mem_wr_req_tdata[79:72] = pfid;
    assign m_axis_mem_wr_req_tdata[87:80] = csqe_id;
    assign m_axis_mem_wr_req_tdata[95:88] = ring_id;
    assign m_axis_mem_wr_req_tdata[127:96] = 32'b0;

    assign m_axis_prp_fetch_tvalid = resetn && state == FETCH_PRP;
    assign m_axis_prp_fetch_tdata[127:0] = {stream_prp2, stream_prp1};
    assign m_axis_prp_fetch_tdata[135:128] = ring_id;
    assign m_axis_prp_fetch_tdata[143:136] = csqe_id;
    assign m_axis_prp_fetch_tdata[151:144] = pfid;
    assign m_axis_prp_fetch_tdata[159:152] = stream_len_blocks;
    assign m_axis_prp_fetch_tdata[160] = stream_input1_empty && !stream_input2_empty;
    assign m_axis_prp_fetch_tuser = {8'b0, pfid, csqe_id, ring_id};

    fifo2 #(.DATA_WIDTH(1024), .ADDR_WIDTH(3)) cmd_fifo(
       .clk(aclk),
       .resetn(resetn),
       .din(fifo_din),
       .wr_en(fifo_wr_en),
       .dout(fifo_dout),
       .rd_en(fifo_rd_en),
       .empty(fifo_empty),
       .full(fifo_full)
    );

    always @(posedge aclk) begin
        if (!resetn) begin
            state <= WAIT_CMD;
            current_sqe <= 1024'b0;
            stream_idx <= 3'b000;
            stream_offset <= 10'd0;
        end else begin
            case (state)
                WAIT_CMD: begin
                    if (~fifo_empty) begin
                        state <= ANALYSIS;//DECODE_CMD;
                        current_sqe <= fifo_dout;
                        stream_idx <= 3'b000;
                        stream_offset <= 10'd64;
                    end
                end
                ANALYSIS: begin
                    if (stream_idx >= num_streams) begin
                        state <= PREPARE;
                    end else if (~stream_prp1_empty) begin
                        if (stream_prp1_from_host) begin
                            state <= FETCH_PRP;
                        end else begin
                            if (stream_input1_empty && stream_input2_empty)
                                state <= READ_DRAM;
                            else
                                state <= WRITE_DRAM;
                        end
                    end else begin
                        stream_idx <= stream_idx + 1;
                        stream_offset <= stream_offset + 192;
                        state <= ANALYSIS;
                    end
                end
                READ_DRAM: begin
                    if (m_axis_mem_rd_req_tready) begin
                        stream_idx <= stream_idx + 1;
                        stream_offset <= stream_offset + 192;
                        state <= ANALYSIS;
                    end
                end
                WRITE_DRAM: begin
                    if (m_axis_mem_wr_req_tready) begin
                        stream_idx <= stream_idx + 1;
                        stream_offset <= stream_offset + 192;
                        state <= ANALYSIS;
                    end
                end
                FETCH_PRP: begin
                    if (m_axis_prp_fetch_tready) begin
                        stream_idx <= stream_idx + 1;
                        stream_offset <= stream_offset + 192;
                        state <= ANALYSIS;
                    end
                end
                PREPARE: begin
                    state <= EXECUTE;
                end
                EXECUTE: begin
                    if (|sqe_finished) begin
                        state <= SEND_CQE;
                    end
                end
                SEND_CQE: begin
                    if (m_axis_cqe_tready) begin 
                        state <= WAIT_CMD;
                    end
                end
            endcase
        end
    end
    
    assign exec_state[11:0] = {2'b0, stream_offset};
    assign exec_state[15:12] = {1'b0, stream_idx};
    assign exec_state[19:16] = {1'b0, state};
    assign exec_state[23:20] = compute_state;
    assign exec_state[31:24] = num_streams;
    assign exec_state[63:32] = {26'b0, check_reader_valid, check_reader_ready, check_reader_complete,
                                       check_writer_valid, check_writer_ready, check_writer_complete};

endmodule