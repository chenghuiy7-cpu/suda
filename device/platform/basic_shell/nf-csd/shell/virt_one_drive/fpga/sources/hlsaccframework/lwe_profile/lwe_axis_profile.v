`timescale 1ns / 1ps
// Nonintrusive wall-clock profiler for both LWE operators (250 MHz).
// Boundary: accepted ap_start to first assertion of the task-finish TVALID.
// Includes key setup, input starvation and output backpressure. It is NOT a
// count of HLS loop iterations and NOT an estimate of compute-only time.
// No buffering or handshake changes. A finish snapshot remains stable under
// backpressure; final marker-transfer wait is outside this measured interval.
// The original first 16 finish bytes (including error codes) are preserved.
module lwe_axis_profile (
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ap_clk, ASSOCIATED_BUSIF s_axis:m_axis" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 ap_clk CLK" *)
    input wire ap_clk,
    // Wired to the slot's ap_rst_n: hold counters clear while the operator
    // is held in reset. This is a synchronous monitor enable, not a new
    // clock/reset interface or a clock gate.
    input wire slot_enable,
    input wire op_start, input wire op_done,
    input wire input_valid, input wire input_ready,
    input wire [7:0] input_user,
    input wire [511:0] s_axis_TDATA, input wire s_axis_TVALID,
    output wire s_axis_TREADY,
    input wire [63:0] s_axis_TKEEP, input wire [63:0] s_axis_TSTRB,
    input wire [7:0] s_axis_TUSER, input wire s_axis_TLAST,
    input wire [7:0] s_axis_TID, input wire [7:0] s_axis_TDEST,
    output wire [511:0] m_axis_TDATA, output wire m_axis_TVALID,
    input wire m_axis_TREADY,
    output wire [63:0] m_axis_TKEEP, output wire [63:0] m_axis_TSTRB,
    output wire [7:0] m_axis_TUSER, output wire m_axis_TLAST,
    output wire [7:0] m_axis_TID, output wire [7:0] m_axis_TDEST
);
    localparam [63:0] PROFILE_MAGIC = 64'h313030465045574c; // "LWEPF001"
    reg running, start_seen, input_finished, finish_seen;
    reg [63:0] cycles, input_wait, output_wait, input_beats, output_beats;
    reg [319:0] finish_snapshot;
    wire start_event = op_start && !start_seen && !running;
    wire in_data = input_valid && input_ready && (input_user[7:4] == 0);
    wire in_finish = input_valid && input_ready && (input_user[7:4] != 0);
    wire in_wait = input_ready && !input_valid && !input_finished;
    wire out_wait = s_axis_TVALID && !s_axis_TREADY;
    wire out_data = s_axis_TVALID && s_axis_TREADY && (s_axis_TUSER[7:4] == 0);
    wire finish_valid = s_axis_TVALID && (s_axis_TUSER[7:4] != 0);
    wire [319:0] live_snapshot = {output_beats,
        input_beats + {63'b0, (running && in_data)}, output_wait,
        input_wait + {63'b0, (running && in_wait)}, cycles + 64'd1};
    assign s_axis_TREADY = m_axis_TREADY;
    assign m_axis_TVALID = s_axis_TVALID;
    assign m_axis_TDATA = finish_valid
        ? {finish_seen ? finish_snapshot : live_snapshot, PROFILE_MAGIC, s_axis_TDATA[127:0]}
        : s_axis_TDATA;
    assign m_axis_TKEEP = finish_valid ? 64'hffffffffffffffff : s_axis_TKEEP;
    assign m_axis_TSTRB = finish_valid ? 64'hffffffffffffffff : s_axis_TSTRB;
    assign m_axis_TUSER = s_axis_TUSER;
    assign m_axis_TLAST = s_axis_TLAST;
    assign m_axis_TID = s_axis_TID;
    assign m_axis_TDEST = s_axis_TDEST;
    always @(posedge ap_clk) begin
        if (!slot_enable) begin
            running <= 0; start_seen <= 0; input_finished <= 0; finish_seen <= 0;
            cycles <= 0; input_wait <= 0; output_wait <= 0;
            input_beats <= 0; output_beats <= 0; finish_snapshot <= 0;
        end else begin
            if (!op_start) start_seen <= 0;
            if (start_event) begin
                running <= 1; start_seen <= 1; input_finished <= in_finish;
                finish_seen <= 0;
                cycles <= 0; input_wait <= 0; output_wait <= 0;
                input_beats <= in_data ? 1 : 0;
                output_beats <= out_data ? 1 : 0;
            end else if (running && !finish_seen) begin
                cycles <= cycles + 1;
                if (in_finish) input_finished <= 1;
                if (in_wait) input_wait <= input_wait + 1;
                if (out_wait) output_wait <= output_wait + 1;
                if (in_data) input_beats <= input_beats + 1;
                if (out_data) output_beats <= output_beats + 1;
            end
            if (finish_valid && !finish_seen) begin
                finish_snapshot <= live_snapshot;
                finish_seen <= 1;
            end
            if (op_done) begin
                running <= 0;
                start_seen <= 0; // ap_start may remain asserted for the next transaction
            end
        end
    end
endmodule
