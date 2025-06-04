module tdest_cmp (
    input tlast,
    input tvalid,
    input tready,
    input [15:0] tdest,
    input [15:0] target,
    output out
);
    wire dst_match;
    assign dst_match = (tdest == target) ? 1'b1 : 1'b0;
    assign out = tlast & tvalid & tready & dst_match;
endmodule