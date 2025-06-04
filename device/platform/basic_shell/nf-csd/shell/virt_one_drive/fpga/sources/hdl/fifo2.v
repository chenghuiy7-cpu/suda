`timescale 1ns/1ps
module fifo2
#(  // Parameters
	parameter   DATA_WIDTH          = 25,
	parameter   ADDR_WIDTH          = 6,
	parameter   RAM_DEPTH           = (1 << ADDR_WIDTH),
	parameter   TYPE                = "MLAB"
)(  // Ports
	input  wire                         clk,
	input  wire                         resetn,
	input  wire                         wr_en,
	input  wire                         rd_en,
	input  wire [ DATA_WIDTH - 1 : 0 ]  din,
	output      [ DATA_WIDTH - 1 : 0 ]  dout,
	output reg                          empty,
	output reg                          full,
	output reg  [             31 : 0 ]  fifo_count
);

// Port Declarations
// ******************************************************************
// Internal variables
// ******************************************************************
reg     [ADDR_WIDTH-1:0]        wr_pointer;             //Write Pointer
reg     [ADDR_WIDTH-1:0]        rd_pointer;             //Read Pointer
//(* ram_style = TYPE *)
reg     [DATA_WIDTH-1:0]        mem[0:RAM_DEPTH-1]      /*synthesis ramstyle = "MLAB" */;     //Memory
// ******************************************************************
// INSTANTIATIONS
// ******************************************************************

always @ (fifo_count)
begin : FIFO_STATUS
	empty   = (fifo_count == 0);
	full    = (fifo_count == RAM_DEPTH);
end

always @ (posedge clk)
begin : FIFO_COUNTER
	if (~resetn)
		fifo_count <= 0;
	
	else if (wr_en && (!rd_en||rd_en&&empty) && !full)
		fifo_count <= fifo_count + 1;
	
	else if (rd_en && (!wr_en||wr_en&&full) && !empty)
		fifo_count <= fifo_count - 1;
end

always @ (posedge clk)
begin : WRITE_PTR
	if (~resetn) begin
		wr_pointer <= 0;
	end
	
	else if (wr_en && !full) begin
		wr_pointer <= wr_pointer + 1;
	end
end

always @ (posedge clk)
begin : READ_PTR
	if (~resetn) begin
		rd_pointer <= 0;
	end
	
	else if (rd_en && !empty) begin
		rd_pointer <= rd_pointer + 1;
	end
end

always @ (posedge clk)
begin : WRITE
	if (wr_en & !full) begin
		mem[wr_pointer] <= din;
	end
end

assign dout = mem[rd_pointer];

endmodule
