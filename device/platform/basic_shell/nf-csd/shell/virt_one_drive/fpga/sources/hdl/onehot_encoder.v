module onehot_encoder (
    input  [2:0] in,
    output [7:0] out
);
    parameter ZERO  = 8'b00000001,
              ONE   = 8'b00000010,
              TWO   = 8'b00000100,
              THREE = 8'b00001000,
              FOUR  = 8'b00010000,
              FIVE  = 8'b00100000,
              SIX   = 8'b01000000,
              SEVEN = 8'b10000000;
    
    assign out[0] = in == 3'd0;
    assign out[1] = in == 3'd1;
    assign out[2] = in == 3'd2;
    assign out[3] = in == 3'd3;
    assign out[4] = in == 3'd4;
    assign out[5] = in == 3'd5;
    assign out[6] = in == 3'd6;
    assign out[7] = in == 3'd7;
endmodule
