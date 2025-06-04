module counter (
    input sig,
    input clk,
    input resetn,
    output reg [7:0] out
);
  always @ (posedge clk) begin
      if (~resetn) begin
        out <= 8'h0;
      end else if (sig) begin
        out <= out + 8'h1;
      end
  end
endmodule