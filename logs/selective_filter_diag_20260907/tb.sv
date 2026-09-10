`timescale 1ns/1ps
module tb;
reg clk=0;
always #2 clk=~clk;
reg rst=0, start=0, iv=0, olready=0;
reg [511:0] idata=0, ctx=0;
reg [7:0] iuser=0;
reg ilast=0;
wire ir, ov, olast, done, cen;
wire [511:0] odata;
wire [63:0] okeep;
wire [7:0] ouser;
wire [31:0] addr;
reg [511:0] mem[0:127];
reg [7:0] expected[0:5];
integer cycle=0, count=0, idx, stalls=0;
integer pause_cycles=0;
selective_filter dut(
 .ap_clk(clk), .ap_rst_n(rst), .ap_start(start), .ap_done(done),
 .data_in_TDATA(idata), .data_in_TVALID(iv), .data_in_TREADY(ir),
 .data_in_TKEEP(64'hffffffffffffffff), .data_in_TSTRB(64'hffffffffffffffff),
 .data_in_TUSER(iuser), .data_in_TLAST(ilast), .data_in_TID(8'd0), .data_in_TDEST(8'd0),
 .data_out_TDATA(odata), .data_out_TVALID(ov), .data_out_TREADY(olready),
 .data_out_TKEEP(okeep), .data_out_TUSER(ouser), .data_out_TLAST(olast),
 .context_Addr_A(addr), .context_EN_A(cen), .context_Dout_A(ctx));
always @(posedge clk) begin
 if (cen) begin
  ctx <= 0;
  if(addr==192) ctx <= {320'd0,32'd1,32'd32,32'd0,32'd4,32'd512,32'd16};
 end
 if(rst) begin
  cycle <= cycle+1;
  if(cycle>1000000) $fatal(1,"Timeout: outputs=%0d",count);
  if(ov && !olready) stalls <= stalls+1;
  if(ov && olready) begin
   if(ouser==0) begin
    if(count>=6) $fatal(1,"extra payload");
    if(odata[7:0] !== expected[count] || odata[511:8] !== 0 || olast !== 0)
     $fatal(1,"payload %0d: value=%0d last=%b",count,odata[7:0],olast);
    $display("quantity[%0d]=%0d keep=%h",count,odata[7:0],okeep);
    count <= count+1;
   end else begin
    if(ouser!==8'hff || olast!==1 || count!=6 ||
       odata[63:0]!==64'h53464c5453544154 || odata[95:64]!==32'd16 ||
       odata[127:96]!==32'd6 || odata[159:128]!==32'd0)
      $fatal(1,"bad status: %h count=%0d",odata,count);
    $display("PASS RTL total=16 selected=6 error=0 cycles=%0d stalled_cycles=%0d",cycle,stalls);
    $finish;
   end
  end
 end
end
always @(negedge clk) begin
 if(pause_cycles==0) olready=1;
 else olready=(cycle % (pause_cycles+1))==0;
end
initial begin
 if($value$plusargs("PAUSE=%d",pause_cycles)) begin end
 $readmemh("input.hex",mem);
 expected[0]=58; expected[1]=44; expected[2]=45; expected[3]=41; expected[4]=54; expected[5]=39;
 repeat(5) @(negedge clk);
 rst=1; start=1;
 for(idx=0;idx<129;idx=idx+1) begin
  @(negedge clk);
  iv=1;
  idata=(idx==128)?512'd0:mem[idx];
  iuser=(idx==128)?8'hff:8'd0;
  ilast=(idx==128);
  @(posedge clk);
  while(!ir) @(posedge clk);
 end
 @(negedge clk); iv=0;
end
endmodule
