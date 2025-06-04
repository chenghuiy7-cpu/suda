module op_aggregate (
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

    reg [511:0] sum;
    reg [63:0] tuser_reg;

    localparam WAIT_S = 0, SEND_M = 1;

    reg state;

    reg [5:0] beat_count;

    always @(posedge aclk) begin
        if (~aresetn) begin
            state <= WAIT_S;
            sum <= 0;
            beat_count <= 0;
            tuser_reg <= 0;
        end else begin
            case (state)
                WAIT_S: begin
                    if (s_axis_tvalid && s_axis_tready) begin
                        $display("Aggr received data: %h", s_axis_tdata);
                        sum <= sum + s_axis_tdata;
                        if (s_axis_tlast) begin
                            state <= SEND_M;
                            tuser_reg <= s_axis_tuser;
                            beat_count <= 0;
                        end
                    end
                end
                SEND_M: begin
                    if (m_axis_tready) begin
                        beat_count <= beat_count + 1;
                        $display("Aggr send data: %h beat_count %d", m_axis_tdata, beat_count);
                        if (m_axis_tlast) begin
                            sum <= 0;
                            state <= WAIT_S;
                        end
                    end
                end
            endcase
        end
    end

    assign s_axis_tready = (state == WAIT_S);

    assign m_axis_tdata = (beat_count == 0) ? sum : 0;
    assign m_axis_tkeep = 64'hFFFF_FFFF_FFFF_FFFF;
    assign m_axis_tuser = tuser_reg;
    assign m_axis_tlast = (state == SEND_M) && (beat_count == 63);
    assign m_axis_tvalid = state == SEND_M;

endmodule