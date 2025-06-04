module credit_manager (
    input           axi_aclk,
    input           axi_aresetn,

    input           tm_dsc_sts_0_vld,
    output          tm_dsc_sts_0_rdy,
    input           tm_dsc_sts_0_byp,
    input           tm_dsc_sts_0_dir,
    input           tm_dsc_sts_0_mm,
    input [10:0]    tm_dsc_sts_0_qid,
    input [15:0]    tm_dsc_sts_0_avl,
    input           tm_dsc_sts_0_qinv,
    input           tm_dsc_sts_0_qen,
    input           tm_dsc_sts_0_irq_arm,
    input           tm_dsc_sts_0_error,
    input [13:0]    tm_dsc_sts_0_pidx,
    input [2:0]     tm_dsc_sts_0_port_id,
    
    output          dsc_crdt_in_0_vld,
    input           dsc_crdt_in_0_rdy,
    output          dsc_crdt_in_0_dir,
    output          dsc_crdt_in_0_fence,
    output [10:0]   dsc_crdt_in_0_qid,
    output [13:0]   dsc_crdt_in_0_crdt,

    output          crdt_fifo_wr_en,
    output [16:0]   crdt_fifo_din,
    output          crdt_fifo_rd_en,
    input [16:0]    crdt_fifo_dout,
    input           crdt_fifo_empty,
    input           crdt_fifo_full,

    output [63:0]  m_axis_crdt_tdata,
    output          m_axis_crdt_tlast,
    input           m_axis_crdt_tready,
    output          m_axis_crdt_tvalid,
    output [7:0]    m_axis_crdt_tid,
    output [3:0]    m_axis_crdt_tdest,
    output [5:0]   m_axis_crdt_tkeep,

    output reg [31:0]  total_crdt
);
    wire [2:0]      tm_qid;
    wire [2:0]      crdt_qid;
    reg [13:0]      prev_pidx[7:0];
    wire            qid_is_c2h_data;
    wire            should_record;

    always @ (posedge axi_aclk) begin
        if (~axi_aresetn) begin
            prev_pidx[0] <= 14'd0; prev_pidx[1] <= 14'd0; prev_pidx[2] <= 14'd0; prev_pidx[3] <= 14'd0;
            prev_pidx[4] <= 14'd0; prev_pidx[5] <= 14'd0; prev_pidx[6] <= 14'd0; prev_pidx[7] <= 14'd0;
        end else if (dsc_crdt_in_0_vld & dsc_crdt_in_0_rdy) begin
            prev_pidx[crdt_qid] <= crdt_fifo_dout[13:0];
        end
    end

    always @ (posedge axi_aclk) begin
        if (~axi_aresetn) begin
            total_crdt <= 0;
        end else if (dsc_crdt_in_0_vld & dsc_crdt_in_0_rdy & (dsc_crdt_in_0_qid == 11'd9)) begin
            total_crdt <= total_crdt + dsc_crdt_in_0_crdt;
        end
    end

    assign tm_qid = tm_dsc_sts_0_qid - 11'd9;
    assign crdt_qid = crdt_fifo_dout[16:14];
    assign qid_is_c2h_data = (tm_dsc_sts_0_qid[4:0] >= 5'd9) && (tm_dsc_sts_0_qid[4:0] <= 5'd16);
    assign should_record = qid_is_c2h_data & tm_dsc_sts_0_dir & tm_dsc_sts_0_byp & tm_dsc_sts_0_qen;

    assign tm_dsc_sts_0_rdy = should_record ? (~crdt_fifo_full & m_axis_crdt_tready) : 1'b1;

    assign dsc_crdt_in_0_vld = ~crdt_fifo_empty;
    assign dsc_crdt_in_0_dir = 1'b1; // C2H
    assign dsc_crdt_in_0_fence = 1'b1;
    assign dsc_crdt_in_0_qid = {8'b0, crdt_qid} + 11'd9;
    assign dsc_crdt_in_0_crdt = crdt_fifo_dout[13:0] - prev_pidx[crdt_qid];

    assign crdt_fifo_wr_en = should_record & tm_dsc_sts_0_vld & tm_dsc_sts_0_rdy;
    assign crdt_fifo_din[13:0] = tm_dsc_sts_0_pidx;
    assign crdt_fifo_din[16:14] = tm_qid;
    assign crdt_fifo_rd_en = dsc_crdt_in_0_vld & dsc_crdt_in_0_rdy;

    assign m_axis_crdt_tvalid = should_record & ~crdt_fifo_full & tm_dsc_sts_0_vld;
    assign m_axis_crdt_tlast = m_axis_crdt_tvalid;
    assign m_axis_crdt_tkeep = 6'b111111;
    assign m_axis_crdt_tid = 8'h32;
    assign m_axis_crdt_tdest = 4'd2;

    assign m_axis_crdt_tdata[10:0]     = tm_dsc_sts_0_qid;
    assign m_axis_crdt_tdata[26:11]    = tm_dsc_sts_0_avl;
    assign m_axis_crdt_tdata[40:27]    = tm_dsc_sts_0_pidx;
    assign m_axis_crdt_tdata[43:41]    = tm_dsc_sts_0_port_id;
    assign m_axis_crdt_tdata[44]       = tm_dsc_sts_0_byp;
    assign m_axis_crdt_tdata[45]       = tm_dsc_sts_0_dir;
    assign m_axis_crdt_tdata[46]       = tm_dsc_sts_0_mm;
    assign m_axis_crdt_tdata[47]       = tm_dsc_sts_0_qinv;
    assign m_axis_crdt_tdata[48]       = tm_dsc_sts_0_qen;
    assign m_axis_crdt_tdata[49]       = tm_dsc_sts_0_irq_arm;
    assign m_axis_crdt_tdata[50]       = tm_dsc_sts_0_error;
endmodule