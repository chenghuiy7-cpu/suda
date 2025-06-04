module op_filter (
    input aclk,
    input aresetn,
    // AXI Stream input interface
    input [511:0] s_axis_tdata,
    input [63:0] s_axis_tkeep,
    input [63:0] s_axis_tuser,
    input s_axis_tlast,
    input s_axis_tvalid,
    output s_axis_tready,
    // AXI Stream output interface
    output [511:0] m_axis_tdata,
    output [63:0] m_axis_tkeep,
    output [63:0] m_axis_tuser,
    output m_axis_tlast,
    output m_axis_tvalid,
    input m_axis_tready
);

    reg [511:0] s_axis_tdata_reg[1:0];
    reg [63:0] s_axis_tkeep_reg[1:0];
    reg [63:0] s_axis_tuser_reg[1:0];
    reg s_axis_tlast_reg[1:0];

    reg [1:0] reg_depth;

    wire should_keep;
    wire [63:0] a, b, c, d;

    assign a = s_axis_tdata[63:0];
    assign b = s_axis_tdata[127:64];
    assign c = s_axis_tdata[191:128];
    assign d = s_axis_tdata[255:192];

    assign should_keep = (a < 10 && b < 10) || (c < d);

    localparam WAIT_S = 2'b00, SEND_ONE = 2'b01, SEND_TILL_LAST = 2'b10;

    reg [1:0] state;

    always @(posedge aclk) begin
        if (!aresetn) begin
            state <= WAIT_S;
            s_axis_tdata_reg[0] <= 0;
            s_axis_tkeep_reg[0] <= 0;
            s_axis_tuser_reg[0] <= 0;
            s_axis_tlast_reg[0] <= 0;
            s_axis_tdata_reg[1] <= 0;
            s_axis_tkeep_reg[1] <= 0;
            s_axis_tuser_reg[1] <= 0;
            s_axis_tlast_reg[1] <= 0;
            reg_depth <= 0;
        end else begin
            case (state)
                WAIT_S: begin
                    if (s_axis_tvalid) begin
                        // $display("Received data: %h should_keep: %d", s_axis_tdata, should_keep);
                        if (should_keep) begin
                            s_axis_tdata_reg[reg_depth] <= s_axis_tdata;
                            s_axis_tkeep_reg[reg_depth] <= s_axis_tkeep;
                            s_axis_tuser_reg[reg_depth] <= s_axis_tuser;
                            s_axis_tlast_reg[reg_depth] <= s_axis_tlast;
                            reg_depth <= reg_depth + 1;
                            if (reg_depth == 1) begin
                                state <= s_axis_tlast ? SEND_TILL_LAST : SEND_ONE;
                            end else begin
                                state <= s_axis_tlast ? SEND_TILL_LAST : WAIT_S;
                            end
                        end else if (s_axis_tlast) begin
                            state <= SEND_TILL_LAST;
                        end
                    end
                end
                SEND_ONE: begin
                    if (m_axis_tready) begin
                        // $display("Sending data: %h", s_axis_tdata_reg[0]);
                        s_axis_tdata_reg[0] <= s_axis_tdata_reg[1];
                        s_axis_tkeep_reg[0] <= s_axis_tkeep_reg[1];
                        s_axis_tuser_reg[0] <= s_axis_tuser_reg[1];
                        s_axis_tlast_reg[0] <= s_axis_tlast_reg[1];
                        reg_depth <= reg_depth - 1;
                        state <= WAIT_S;
                    end
                end
                SEND_TILL_LAST: begin
                    if (m_axis_tready) begin
                        // $display("Sending data: %h", s_axis_tdata_reg[0]);
                        if (reg_depth > 1) begin
                            s_axis_tdata_reg[0] <= s_axis_tdata_reg[1];
                            s_axis_tkeep_reg[0] <= s_axis_tkeep_reg[1];
                            s_axis_tuser_reg[0] <= s_axis_tuser_reg[1];
                            s_axis_tlast_reg[0] <= s_axis_tlast_reg[1];
                            reg_depth <= reg_depth - 1;
                        end else begin
                            // $display("Jump back to WAIT_S");
                            reg_depth <= 0;
                            state <= WAIT_S;
                        end
                    end
                end
                default: begin
                    state <= WAIT_S;
                end
            endcase
        end
    end

    assign s_axis_tready = (state == WAIT_S) & aresetn;

    assign m_axis_tvalid = (state == SEND_ONE || ((state == SEND_TILL_LAST) && (reg_depth != 0))) & aresetn;
    assign m_axis_tdata = s_axis_tdata_reg[0];
    assign m_axis_tkeep = s_axis_tkeep_reg[0];
    assign m_axis_tuser = s_axis_tuser_reg[0];
    assign m_axis_tlast = (state == SEND_TILL_LAST) & (reg_depth == 1) & aresetn;

endmodule