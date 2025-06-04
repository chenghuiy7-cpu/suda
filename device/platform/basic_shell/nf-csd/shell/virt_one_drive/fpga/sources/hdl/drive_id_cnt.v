module drive_id_cnt(
  input clk,
  input resetn,
  input  [1:0] in_drive_id_0,
  input  [1:0] in_drive_id_1,
  input  [1:0] in_drive_id_2,
  input  [1:0] in_drive_id_3,
  input  [1:0] in_drive_id_4,
  input  [1:0] in_drive_id_5,
  input  [1:0] in_drive_id_6,
  input  [1:0] in_drive_id_7,
  output [1:0] out_drive_id_0,
  output [1:0] out_drive_id_1,
  output [1:0] out_drive_id_2,
  output [1:0] out_drive_id_3,
  output [1:0] out_drive_id_4,
  output [1:0] out_drive_id_5,
  output [1:0] out_drive_id_6,
  output [1:0] out_drive_id_7,
  output [31:0] drive_cnt
);

  reg [31:0] drive_reg;
  wire [15:0] bunch_drive;

  assign out_drive_id_0 = in_drive_id_0;
  assign out_drive_id_1 = in_drive_id_1;
  assign out_drive_id_2 = in_drive_id_2;
  assign out_drive_id_3 = in_drive_id_3;
  assign out_drive_id_4 = in_drive_id_4;
  assign out_drive_id_5 = in_drive_id_5;
  assign out_drive_id_6 = in_drive_id_6;
  assign out_drive_id_7 = in_drive_id_7;

  assign bunch_drive = {in_drive_id_7,
                        in_drive_id_6,
                        in_drive_id_5,
                        in_drive_id_4,
                        in_drive_id_3,
                        in_drive_id_2,
                        in_drive_id_1,
                        in_drive_id_0};

  always @ (posedge clk)
  begin
    if (~resetn)
      drive_reg <= 32'b0;
    else
      drive_reg <= drive_reg | {16'b0, bunch_drive};
  end

  assign drive_cnt = drive_reg;

endmodule