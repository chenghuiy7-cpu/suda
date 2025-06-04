module axis_debug_reg (
    input  aclk,
    input  aresetn,

    input user_reset,

    input [511:0] s_axis_data_tdata,
    input [63:0] s_axis_data_tkeep,
    input [63:0] s_axis_data_tuser,
    input s_axis_data_tlast,
    input s_axis_data_tvalid,
    output s_axis_data_tready,

    output reg [1023:0] data_out
);
    reg [19:0] offset;
    wire resetn;
    assign resetn = aresetn & ~user_reset;

    // Record first 1024 bits of data into data_out
    always @(posedge aclk) begin
        if (resetn == 1'b0) begin
            data_out <= 1024'b0;
            offset <= 0;
        end else if (s_axis_data_tvalid && s_axis_data_tready) begin
            if (offset < 1024) begin
                data_out[offset +: 512] <= s_axis_data_tdata;
            end
            if (s_axis_data_tlast) begin
                offset <= 0;
            end else begin
                offset <= offset + 512;
            end
        end
    end

    assign s_axis_data_tready = resetn;

endmodule
