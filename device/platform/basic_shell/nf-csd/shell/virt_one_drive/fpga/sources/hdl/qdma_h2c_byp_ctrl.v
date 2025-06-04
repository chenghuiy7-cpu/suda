module qdma_h2c_byp_ctrl (
    input axi_aclk,
    input axi_aresetn,

    output [63:0]h2c_byp_in_st_0_addr,
    output [15:0]h2c_byp_in_st_0_len,
    output h2c_byp_in_st_0_sop,
    output h2c_byp_in_st_0_eop,
    output h2c_byp_in_st_0_sdi,
    output h2c_byp_in_st_0_mrkr_req,
    output h2c_byp_in_st_0_no_dma,
    output [10:0]h2c_byp_in_st_0_qid,
    output h2c_byp_in_st_0_error,
    output [7:0]h2c_byp_in_st_0_func,
    output [15:0]h2c_byp_in_st_0_cidx,
    output [2:0]h2c_byp_in_st_0_port_id,
    output h2c_byp_in_st_0_valid,
    input h2c_byp_in_st_0_ready,

    input [127:0] s_axis_h2c_byp_in_tdata,
    input         s_axis_h2c_byp_in_tlast,
    output        s_axis_h2c_byp_in_tready,
    input         s_axis_h2c_byp_in_tvalid,
    input [15:0]  s_axis_h2c_byp_in_tkeep,

    input [255:0]h2c_byp_out_0_dsc,
    input h2c_byp_out_0_st_mm,
    input [1:0]h2c_byp_out_0_dsc_sz,
    input [10:0]h2c_byp_out_0_qid,
    input h2c_byp_out_0_error,
    input [7:0]h2c_byp_out_0_func,
    input [15:0]h2c_byp_out_0_cidx,
    input [2:0]h2c_byp_out_0_port_id,
    input [2:0]h2c_byp_out_0_fmt,
    input h2c_byp_out_0_valid,
    output h2c_byp_out_0_ready,

    output [31:0] cidx_regs
);
    wire has_avilable_bd;
    reg [6:0] cidx[7:0];
    reg [6:0] used_cidx[7:0];
    wire [6:0] next_used_cidx;

    wire [3:0] out_data_qid;
    wire [3:0] in_data_qid;

    wire is_prp_fetch_qid;

    assign is_prp_fetch_qid = s_axis_h2c_byp_in_tdata[72:64] == 9'd17;

    assign out_data_qid = h2c_byp_out_0_qid - 11'd9;
    assign in_data_qid = h2c_byp_in_st_0_qid - 11'd9;

    always @ (posedge axi_aclk)
    begin
        if (~axi_aresetn)
        begin
            cidx[0] <= 7'b0; cidx[1] <= 7'b0; cidx[2] <= 7'b0; cidx[3] <= 7'b0;
            cidx[4] <= 7'b0; cidx[5] <= 7'b0; cidx[6] <= 7'b0; cidx[7] <= 7'b0;
        end
        else if (h2c_byp_out_0_ready & h2c_byp_out_0_valid)
        begin
            cidx[out_data_qid] <= h2c_byp_out_0_cidx[6:0];
        end
    end

    always @ (posedge axi_aclk)
    begin
        if (~axi_aresetn)
        begin
            used_cidx[0] <= 7'b0; used_cidx[1] <= 7'b0; used_cidx[2] <= 7'b0; used_cidx[3] <= 7'b0;
            used_cidx[4] <= 7'b0; used_cidx[5] <= 7'b0; used_cidx[6] <= 7'b0; used_cidx[7] <= 7'b0;
        end
        else if (h2c_byp_in_st_0_ready & h2c_byp_in_st_0_valid)
        begin
            used_cidx[in_data_qid] <= next_used_cidx;
        end
    end

    /* encapsulation of AR signals to QDMA bypass BD format 
     * 96-bit length single-beat packet
     * 95:32  64-bit bypass BD address
     * 31:30  2-bit reserved zero
     * 29:14  16-bit bypass BD length
     * 13:11  3-bit bypass port ID (i.e., ARID)
     * 10: 0  11-bit bypass BD qid 
     */
    assign next_used_cidx = used_cidx[in_data_qid] + 1;
    assign has_avilable_bd = used_cidx[in_data_qid] != cidx[in_data_qid];
    assign h2c_byp_in_st_0_addr = {16'b0, s_axis_h2c_byp_in_tdata[47:0]};
    assign h2c_byp_in_st_0_qid[10:7] = 4'b0;
    assign h2c_byp_in_st_0_qid[6:5] = s_axis_h2c_byp_in_tdata[74:73]; // Function ID
    assign h2c_byp_in_st_0_qid[4:0] = is_prp_fetch_qid ? 5'd17 : ({2'b0, s_axis_h2c_byp_in_tdata[77:75]} + 5'd9); // ARID
    assign h2c_byp_in_st_0_port_id = s_axis_h2c_byp_in_tdata[95:94];  // Drive ID
    assign h2c_byp_in_st_0_len = s_axis_h2c_byp_in_tdata[93:78];
    assign h2c_byp_in_st_0_cidx = {9'b0, next_used_cidx};
    assign h2c_byp_in_st_0_func = {6'b0, s_axis_h2c_byp_in_tdata[74:73]};

    assign h2c_byp_in_st_0_valid = s_axis_h2c_byp_in_tvalid;
    assign s_axis_h2c_byp_in_tready = h2c_byp_in_st_0_ready;

    assign h2c_byp_in_st_0_eop = 1'b1;
    assign h2c_byp_in_st_0_sop = 1'b1;

    assign h2c_byp_in_st_0_mrkr_req = 1'b0;
    assign h2c_byp_in_st_0_no_dma = 1'b0;
    assign h2c_byp_in_st_0_error = 1'b0;
    assign h2c_byp_in_st_0_sdi = |(used_cidx[in_data_qid] & 7'd63);

    assign h2c_byp_out_0_ready = 1'b1;

    assign cidx_regs[15:0] = {6'b0, used_cidx[0]};
    assign cidx_regs[31:16] = {6'b0, cidx[0]};

endmodule
