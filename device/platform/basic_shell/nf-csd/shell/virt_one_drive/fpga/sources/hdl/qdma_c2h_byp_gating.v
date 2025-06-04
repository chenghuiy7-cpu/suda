module qdma_c2h_byp_gating (
    input axi_aclk,
    input axi_aresetn,

    input aw_req_sent,
    output aw_req_should_send,

    input w_sent,
    output w_should_send,

    output [31:0] idxs
);

    reg [9:0] aw_idx;
    reg [9:0] w_idx;

    always @ (posedge axi_aclk) begin
        if (~axi_aresetn) begin
            aw_idx <= 10'b0;
            w_idx <= 10'b0;
        end else if (aw_req_sent) begin
            aw_idx <= aw_idx + 1;
        end else if (w_sent) begin
            w_idx <= w_idx + 1;
        end
    end

    assign aw_req_should_send = (aw_idx + 1) != w_idx;
    assign w_should_send = aw_idx != w_idx;

    assign idxs[9:0] = w_idx;
    assign idxs[25:16] = aw_idx;

endmodule