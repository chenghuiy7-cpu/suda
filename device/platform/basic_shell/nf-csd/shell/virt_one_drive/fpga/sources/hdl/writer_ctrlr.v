module writer_ctrlr(
    input aclk,
    input aresetn,

    output [127:0] m_axis_mem_wr_req_tdata,
    output m_axis_mem_wr_req_tvalid,
    input m_axis_mem_wr_req_tready
);

    reg [2:0] current_state, next_state;

    wire [39:0] cur_req_addr;
    wire [31:0] cur_req_len;
    wire [7:0] cur_req_pfid;
    wire [7:0] cur_req_csqe_id;
    wire [7:0] cur_req_stream_id;

    always @ (posedge aclk) begin
        if (~aresetn)
            current_state <= 3'b001;
        else
            current_state <= next_state;
    end

    always @ (*) begin
        case (current_state)
            3'b001:
                next_state = 3'b010;
            3'b010:
                if (m_axis_mem_wr_req_tvalid & m_axis_mem_wr_req_tready)
                    next_state = 3'b100;
                else
                    next_state = 3'b010;
            3'b100:
                next_state = 3'b000;
            default:
                next_state = 3'b100;
        endcase
    end

    assign m_axis_mem_wr_req_tvalid = current_state == 3'b010;

    assign m_axis_mem_wr_req_tdata[39:0] = cur_req_addr;
    assign m_axis_mem_wr_req_tdata[71:40] = cur_req_len;
    assign m_axis_mem_wr_req_tdata[79:72] = cur_req_pfid;
    assign m_axis_mem_wr_req_tdata[87:80] = cur_req_csqe_id;
    assign m_axis_mem_wr_req_tdata[95:88] = cur_req_stream_id;
    assign m_axis_mem_wr_req_tdata[127:96] = 32'b0;

    assign cur_req_addr = 40'h0020000000;
    assign cur_req_len = 32'h00020000;
    assign cur_req_pfid = 8'h1;
    assign cur_req_csqe_id = 8'h1f;
    assign cur_req_stream_id = 8'h0;

endmodule