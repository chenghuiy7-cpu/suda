module compute_engine #(
    // Parameter definitions for flexibility and reusability
    parameter integer ADDR_WIDTH_MASTER = 64,
    parameter integer DATA_WIDTH_MASTER = 128,
    parameter integer ADDR_WIDTH_SLAVE = 16,
    parameter integer DATA_WIDTH_SLAVE = 64,
    parameter integer NUM_RINGS = 4,
    parameter integer NUM_RING_LOG2 = 2,
    parameter integer ENTRIES_PER_RING = 16,
    parameter integer RING_ENTRIES_LOG2 = 4,
    parameter integer FIFO_DATA_WIDTH = NUM_RING_LOG2 + RING_ENTRIES_LOG2,
    parameter integer RING_ENTRY_SIZE_BITS = 1024
)(
    // Global Signals
    input wire aclk,
    input wire aresetn,

    // AXI Master Interface Signals
    // Write address channel (Master)
    output wire [ADDR_WIDTH_MASTER-1:0] m_axi_awaddr,
    output wire [7:0] m_axi_awlen,
    output wire [2:0] m_axi_awsize,
    output wire [1:0] m_axi_awburst,
    output wire m_axi_awvalid,
    input wire m_axi_awready,
    // Write data channel (Master)
    output wire [DATA_WIDTH_MASTER-1:0] m_axi_wdata,
    output wire [(DATA_WIDTH_MASTER/8)-1:0] m_axi_wstrb,
    output wire m_axi_wlast,
    output wire m_axi_wvalid,
    input wire m_axi_wready,
    // Write response channel (Master)
    input wire [1:0] m_axi_bresp,
    input wire m_axi_bvalid,
    output wire m_axi_bready,
    // Read address channel (Master)
    output wire [ADDR_WIDTH_MASTER-1:0] m_axi_araddr,
    output wire [7:0] m_axi_arlen,
    output wire [2:0] m_axi_arsize,
    output wire [1:0] m_axi_arburst,
    output wire m_axi_arvalid,
    input wire m_axi_arready,
    // Read data channel (Master)
    input wire [DATA_WIDTH_MASTER-1:0] m_axi_rdata,
    input wire [1:0] m_axi_rresp,
    input wire m_axi_rlast,
    input wire m_axi_rvalid,
    output wire m_axi_rready,

    // AXI Slave Interface Signals
    // Write address channel (Slave)
    input wire [ADDR_WIDTH_SLAVE-1:0] s_axi_awaddr,
    input wire [2:0] s_axi_awprot,
    input wire s_axi_awvalid,
    output s_axi_awready,
    // Write data channel (Slave)
    input wire [DATA_WIDTH_SLAVE-1:0] s_axi_wdata,
    input wire [(DATA_WIDTH_SLAVE/8)-1:0] s_axi_wstrb,
    input wire s_axi_wvalid,
    output s_axi_wready,
    // Write response channel (Slave)
    output [1:0] s_axi_bresp,
    output s_axi_bvalid,
    input wire s_axi_bready,
    // Read address channel (Slave)
    input wire [ADDR_WIDTH_SLAVE-1:0] s_axi_araddr,
    input wire [2:0] s_axi_arprot,
    input wire s_axi_arvalid,
    output s_axi_arready,
    // Read data channel (Slave)
    output reg [DATA_WIDTH_SLAVE-1:0] s_axi_rdata,
    output [1:0] s_axi_rresp,
    output s_axi_rvalid,
    input s_axi_rready,

    output [RING_ENTRY_SIZE_BITS-1:0] m_axis_sqe_tdata,
    output m_axis_sqe_tvalid,
    input m_axis_sqe_tready,

    input [DATA_WIDTH_MASTER-1:0] s_axis_cqe_tdata,
    input s_axis_cqe_tvalid,
    output s_axis_cqe_tready,

    input [1023:0] debug_in,

    input [63:0] exec_state,

    input [127:0] dram_reader_state,
    input [63:0] dram_writer_state,

    input [63:0] merger_state,

    input [127:0] fetcher_state,

    input [127:0] op_in_count,
    input [127:0] op_out_count,

    output reg user_reset
);

    reg [ADDR_WIDTH_MASTER-1:0]ring_sq_phys_addr[NUM_RINGS-1:0];
    reg [ADDR_WIDTH_MASTER-1:0]ring_cq_phys_addr[NUM_RINGS-1:0];
    reg [RING_ENTRIES_LOG2-1:0]ring_tail_db[NUM_RINGS-1:0];

    // FSM State declaration
    localparam AWAIT_ADDR = 2'b00, AWAIT_DATA = 2'b01, WRITE_FIFO = 2'b10, SEND_BRESP = 2'b11;
    reg [1:0] current_state, next_state;

    localparam READ_AWAIT_ARADDR = 2'b00, READ_SEND_RDATA = 2'b01;
    reg [1:0] read_current_state, read_next_state;

    // Temporary storage for the address to determine which register to update after receiving data
    reg [ADDR_WIDTH_SLAVE-1:0] temp_addr, read_temp_addr;
    wire [1:0] index, read_index;
    wire wen;

    reg [NUM_RING_LOG2-1:0] ring_idx_iter;
    reg [RING_ENTRIES_LOG2-1:0] ring_cur_processed[NUM_RINGS-1:0];
    reg [RING_ENTRIES_LOG2-1:0] ring_cq_tail[NUM_RINGS-1:0];

    localparam WAIT_DB = 3'b000, FETCH_SQE_SEND_AR = 3'b001, FETCH_SQE_WAIT_R = 3'b010, COMPUTE = 3'b011;
    localparam WAIT_CQE = 3'b000, SEND_AW = 3'b001, SEND_W = 3'b010, WAIT_B = 3'b011;
    localparam RING_ENTRY_SIZE_LOG2 = 7;
    localparam CQE_SIZE_LOG2 = 4;
    localparam RING_ENTRY_SIZE = 1 << RING_ENTRY_SIZE_LOG2;

    reg [2:0] current_db_state;
    reg [2:0] current_cq_state;
    reg [RING_ENTRY_SIZE_BITS-1:0] csqe_buffer;
    reg [9:0] csqe_buffer_offset;
    wire [9:0] csqe_buffer_read_offset;

    reg [NUM_RINGS-1:0] phase;

    integer i;

    wire resetn;
    assign resetn = aresetn & ~user_reset;

    assign csqe_buffer_read_offset = {read_temp_addr[6:0], 3'b0};

    assign index = temp_addr[4:3];
    assign read_index = read_temp_addr[4:3];

    // FSM for handling AXI write transactions
    always @(posedge aclk) begin
        if (!aresetn) current_state <= AWAIT_ADDR;
        else current_state <= next_state;
    end

    always @(posedge aclk) begin
        if (!resetn) read_current_state <= READ_AWAIT_ARADDR;
        else read_current_state <= read_next_state;
    end

    assign s_axi_awready = aresetn & (current_state == AWAIT_ADDR);
    assign s_axi_wready = aresetn & (current_state == AWAIT_DATA);
    assign s_axi_bvalid = aresetn & (current_state == SEND_BRESP);
    assign s_axi_bresp = 2'b00; // OKAY response by default
    assign wen = resetn & (current_state == AWAIT_DATA) & s_axi_wvalid & s_axi_wready;

    // Next state logic and data/address recording
    always @(*) begin
        case (current_state)
            AWAIT_ADDR: begin
                if (s_axi_awvalid) begin
                    // Temporarily store the address to determine which register to update after receiving data
                    next_state = AWAIT_DATA;
                end else begin
                    next_state = AWAIT_ADDR;
                end
            end
            AWAIT_DATA: begin
                if (s_axi_wvalid) begin
                    next_state = SEND_BRESP; // Return to IDLE to await the next transaction
                end else begin
                    next_state = AWAIT_DATA;
                end
            end
            SEND_BRESP: begin
                if (s_axi_bready && s_axi_bvalid) begin
                    next_state = AWAIT_ADDR; // Return to IDLE to await the next transaction
                end else begin
                    next_state = SEND_BRESP;
                end
            end
            default: next_state = AWAIT_ADDR;
        endcase
    end

    always @(posedge aclk) begin
        if (current_state == AWAIT_ADDR) begin
            temp_addr <= s_axi_awaddr;
        end
    end

    reg [DATA_WIDTH_SLAVE-1:0] write_count;
    reg [DATA_WIDTH_SLAVE-1:0] write_data;
    reg [DATA_WIDTH_SLAVE-1:0] write_addr;
    reg [DATA_WIDTH_SLAVE-1:0] write_wstrb;

    // Update logic for ring_sq_phys_addr and ring_tail_db based on temp_addr
    // Assuming addresses are aligned with the array indexes for simplicity
    always @ (posedge aclk) begin
        if (~resetn) begin
            write_count <= 0;
            write_data <= 0;
            write_addr <= 0;
            write_wstrb <= 0;
            user_reset <= 0;
            for (i = 0; i < NUM_RINGS; i = i + 1) begin
                ring_sq_phys_addr[i] <= 0;
                ring_cq_phys_addr[i] <= 0;
                ring_tail_db[i] <= 0;
            end
        end else if (wen) begin
            // Update the ring_sq_phys_addr and ring_tail_db registers
            if (temp_addr < 16'd32) begin
                write_data <= s_axi_wdata;
                write_addr <= temp_addr;
                write_wstrb <= s_axi_wstrb;
                write_count <= write_count + 1;
                ring_sq_phys_addr[index] <= s_axi_wdata;
            end else if (temp_addr >= 16'd32 && temp_addr < 16'd64) begin
                ring_cq_phys_addr[index] <= s_axi_wdata;
            end else if (temp_addr >= 16'd64 && temp_addr < 16'd96) begin
                ring_tail_db[index] <= s_axi_wdata[RING_ENTRIES_LOG2-1:0];
            end else if (temp_addr == 16'd160) begin
                user_reset <= s_axi_wdata[0];
            end
        end
    end

    always @(*) begin
        if (read_temp_addr < 16'd32) begin
            s_axi_rdata = ring_sq_phys_addr[read_index];
        end else if (read_temp_addr >= 16'd32 && read_temp_addr < 16'd64) begin
            s_axi_rdata = ring_cq_phys_addr[read_index];
        end else if (read_temp_addr >= 16'd64 && read_temp_addr < 16'd96) begin
            s_axi_rdata = ring_tail_db[read_index];
        end else if (read_temp_addr >= 96 && read_temp_addr < 128) begin
            s_axi_rdata = ring_cur_processed[read_index];
        end else if (read_temp_addr >= 128 && read_temp_addr < 160) begin
            s_axi_rdata = ring_cq_tail[read_index];
        end else if (read_temp_addr >= 1024 && read_temp_addr < 2048) begin
            s_axi_rdata = csqe_buffer[csqe_buffer_read_offset +: DATA_WIDTH_SLAVE];
        end else if (read_temp_addr >= 2048 && read_temp_addr < 3072) begin
            s_axi_rdata = debug_in[csqe_buffer_read_offset +: DATA_WIDTH_SLAVE];
        end else if (read_temp_addr == 160) begin
            s_axi_rdata = {63'b0, user_reset};
        end else if (read_temp_addr == 168) begin
            s_axi_rdata = exec_state;
        end else if (read_temp_addr == 176) begin
            s_axi_rdata = dram_reader_state[63:0];
        end else if (read_temp_addr == 184) begin
            s_axi_rdata = dram_reader_state[127:64];
        end else if (read_temp_addr == 192) begin
            s_axi_rdata = merger_state;
        end else if (read_temp_addr == 200) begin
            s_axi_rdata = fetcher_state[63:0];
        end else if (read_temp_addr == 208) begin
            s_axi_rdata = fetcher_state[127:64];
        end else if (read_temp_addr == 216) begin
            s_axi_rdata = op_in_count[63:0];
        end else if (read_temp_addr == 224) begin
            s_axi_rdata = op_in_count[127:64];
        end else if (read_temp_addr == 232) begin
            s_axi_rdata = op_out_count[63:0];
        end else if (read_temp_addr == 240) begin
            s_axi_rdata = op_out_count[127:64];
        end else if (read_temp_addr == 248) begin
            s_axi_rdata = dram_writer_state;
        end else if (read_temp_addr == 256) begin
            s_axi_rdata = {32'b0, 32'hdeadbeef};
        end else begin
            s_axi_rdata = 64'd0;
        end
    end

    assign s_axi_arready = resetn & (read_current_state == READ_AWAIT_ARADDR);
    assign s_axi_rvalid = resetn & (read_current_state == READ_SEND_RDATA);
    assign s_axi_rresp = 2'b00; // OKAY response by default

    // Read FSM Next State Logic
    always @(*) begin
        case (read_current_state)
            READ_AWAIT_ARADDR: begin
                if (s_axi_arready && s_axi_arvalid) begin
                    read_next_state = READ_SEND_RDATA;
                end else begin
                    read_next_state = READ_AWAIT_ARADDR;
                end
            end
            READ_SEND_RDATA: begin
                if (s_axi_rready && s_axi_rvalid) begin
                    read_next_state = READ_AWAIT_ARADDR; // Data sent, return to IDLE
                end else begin
                    read_next_state = READ_SEND_RDATA;
                end
            end
            default: read_next_state = READ_AWAIT_ARADDR;
        endcase
    end

    always @(posedge aclk) begin
        if (read_current_state == READ_AWAIT_ARADDR) begin
            read_temp_addr <= s_axi_araddr;
        end
    end

    always @(posedge aclk) begin
        if (!resetn) begin
            current_db_state <= WAIT_DB;
            for (i = 0; i < NUM_RINGS; i = i + 1) begin
                ring_cur_processed[i] <= 0;
            end
            ring_idx_iter <= 0;
        end else begin
            case (current_db_state)
                WAIT_DB: begin
                    if (ring_tail_db[ring_idx_iter] == ring_cur_processed[ring_idx_iter]) begin
                        current_db_state <= WAIT_DB;
                        ring_idx_iter <= ring_idx_iter + 1;
                    end else begin
                        current_db_state <= FETCH_SQE_SEND_AR;
                    end
                end
                FETCH_SQE_SEND_AR: begin
                    if (m_axi_arready && m_axi_arvalid) begin
                        current_db_state <= FETCH_SQE_WAIT_R;
                        csqe_buffer_offset <= 0;
                    end
                end
                FETCH_SQE_WAIT_R: begin
                    if (m_axi_rvalid && m_axi_rready) begin
                        csqe_buffer[csqe_buffer_offset +: DATA_WIDTH_MASTER] <= m_axi_rdata;
                        csqe_buffer_offset <= csqe_buffer_offset + DATA_WIDTH_MASTER;

                        if (m_axi_rlast) begin
                            current_db_state <= COMPUTE;
                        end
                    end
                end
                COMPUTE: begin
                    ring_cur_processed[ring_idx_iter] <= ring_cur_processed[ring_idx_iter] + 1;
                    if (m_axis_sqe_tready) begin
                        current_db_state <= WAIT_DB;
                    end else begin
                        current_db_state <= COMPUTE;
                    end
                end
                default: current_db_state <= WAIT_DB;
            endcase
        end
    end

    reg [DATA_WIDTH_MASTER-1:0] cqe_data;
    wire [1:0] cqid;

    assign cqid = cqe_data[25:24];

    always @(posedge aclk) begin
        if (!resetn) begin
            current_cq_state <= WAIT_CQE;
            cqe_data <= 0;
            for (i = 0; i < NUM_RINGS; i = i + 1) begin
                ring_cq_tail[i] <= 0;
                phase[i] <= 1;
            end
        end else begin
            case (current_cq_state)
                WAIT_CQE: begin
                    if (s_axis_cqe_tvalid) begin
                        cqe_data <= s_axis_cqe_tdata;
                        current_cq_state <= SEND_AW;
                    end
                end
                SEND_AW: begin
                    if (m_axi_awready && m_axi_awvalid) begin
                        current_cq_state <= SEND_W;
                    end
                end
                SEND_W: begin
                    if (m_axi_wready && m_axi_wvalid) begin
                        current_cq_state <= WAIT_B;
                    end
                end
                WAIT_B: begin
                    if (m_axi_bvalid && m_axi_bready) begin
                        if (&ring_cq_tail[cqid]) begin
                            phase[cqid] <= ~phase[cqid];
                        end
                        ring_cq_tail[cqid] <= ring_cq_tail[cqid] + 1;
                        current_cq_state <= WAIT_CQE;
                    end
                end
                default: current_cq_state <= WAIT_CQE;
            endcase
        end
    end

    assign m_axis_sqe_tdata = csqe_buffer;
    assign m_axis_sqe_tvalid = resetn & current_db_state == COMPUTE;

    assign s_axis_cqe_tready = resetn & current_cq_state == WAIT_CQE;

    assign m_axi_araddr = ring_sq_phys_addr[ring_idx_iter] + (ring_cur_processed[ring_idx_iter] << RING_ENTRY_SIZE_LOG2);
    assign m_axi_arsize = 3'b100; // 16B
    assign m_axi_arlen = 8'd7; // 128B SQEs
    assign m_axi_arburst = 2'b01; // Incrementing
    assign m_axi_arvalid = resetn & current_db_state == FETCH_SQE_SEND_AR;
    assign m_axi_rready = resetn & current_db_state == FETCH_SQE_WAIT_R;

    assign m_axi_awaddr = ring_cq_phys_addr[cqid] + (ring_cq_tail[cqid] << CQE_SIZE_LOG2);
    assign m_axi_awvalid = resetn & current_cq_state == SEND_AW;
    assign m_axi_awlen = 8'd0; // 16B CQEs
    assign m_axi_awsize = 3'b100; // 16B
    assign m_axi_awburst = 2'b01; // Incrementing

    assign m_axi_wvalid = resetn & current_cq_state == SEND_W;
    assign m_axi_wdata[15:0] = csqe_buffer[15:0];
    assign m_axi_wdata[63:16] = 0;
    assign m_axi_wdata[64] = phase[cqid];
    assign m_axi_wdata[DATA_WIDTH_MASTER-1:65] = 0;
    assign m_axi_wstrb = {DATA_WIDTH_MASTER/8{1'b1}};
    assign m_axi_wlast = 1'b1;

    assign m_axi_bready = resetn & current_cq_state == WAIT_B;
endmodule
