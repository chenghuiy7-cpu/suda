module ar_drive_monitor(
  input clk,
  input resetn,
  input [7:0] ar_to_drive,
  output reg [3:0] drive_exist
);

  always @ (posedge clk)
  begin
    if (resetn)
      drive_exist[0] <= 1'd0;
    else if (ar_to_drive == 8'd0)
      drive_exist[0] <= 1'd1;
  end

  always @ (posedge clk)
  begin
    if (resetn)
      drive_exist[1] <= 1'd0;
    else if (ar_to_drive == 8'd1)
      drive_exist[1] <= 1'd1;
  end

  always @ (posedge clk)
  begin
    if (resetn)
      drive_exist[2] <= 1'd0;
    else if (ar_to_drive == 8'd2)
      drive_exist[2] <= 1'd1;
  end

  always @ (posedge clk)
  begin
    if (resetn)
      drive_exist[3] <= 1'd0;
    else if (ar_to_drive == 8'd3)
      drive_exist[3] <= 1'd1;
  end

endmodule