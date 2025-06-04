module qdma_c2h_byp_ctrl (
    input axi_aclk,
    input axi_aresetn,

    output [63:0]c2h_byp_in_st_0_addr,
    output [10:0]c2h_byp_in_st_0_qid,
    output c2h_byp_in_st_0_error,
    output [7:0]c2h_byp_in_st_0_func,
    output [2:0]c2h_byp_in_st_0_port_id,
    output [6:0]c2h_byp_in_st_0_pfch_tag,
    output c2h_byp_in_st_0_valid,
    input c2h_byp_in_st_0_ready,

    input [511:0] s_axis_c2h_byp_in_tdata,
    input         s_axis_c2h_byp_in_tlast,
    output        s_axis_c2h_byp_in_tready,
    input         s_axis_c2h_byp_in_tvalid,
    input [63:0]  s_axis_c2h_byp_in_tkeep,

    input [255:0]c2h_byp_out_0_dsc,
    input c2h_byp_out_0_st_mm,
    input [1:0]c2h_byp_out_0_dsc_sz,
    input [10:0]c2h_byp_out_0_qid,
    input c2h_byp_out_0_error,
    input [7:0]c2h_byp_out_0_func,
    input [15:0]c2h_byp_out_0_cidx,
    input [2:0]c2h_byp_out_0_port_id,
    input [6:0]c2h_byp_out_0_pfch_tag,
    input [2:0]c2h_byp_out_0_fmt,
    input c2h_byp_out_0_valid,
    output c2h_byp_out_0_ready,

    input [31:0] pfch_tag_0_lsb,
    input [31:0] pfch_tag_0_msb,
    input [31:0] pfch_tag_1_lsb,
    input [31:0] pfch_tag_1_msb,
    input [31:0] pfch_tag_2_lsb,
    input [31:0] pfch_tag_2_msb,
    input [31:0] pfch_tag_3_lsb,
    input [31:0] pfch_tag_3_msb,

    output reg [31:0] aw_pkt_cnt
);
    wire has_avilable_bd;
    reg [6:0] cidx[31:0];
    reg [6:0] used_cidx[31:0];
    wire [6:0] pfch_tag[31:0];
    wire [6:0] next_used_cidx;

    wire [5:0] out_data_qid;
    wire [5:0] in_data_qid;

    assign out_data_qid = c2h_byp_out_0_qid - ({3'b0, c2h_byp_out_0_func} << 5)
                       + ({3'b0, c2h_byp_out_0_func} << 3) - 11'd9;
    assign in_data_qid = c2h_byp_in_st_0_qid - ({3'b0, c2h_byp_in_st_0_func} << 5)
                       + ({3'b0, c2h_byp_in_st_0_func} << 3) - 11'd9;

    always @ (posedge axi_aclk)
    begin
        if (~axi_aresetn)
        begin
            cidx[ 0] <= 7'b0; cidx[ 1] <= 7'b0; cidx[ 2] <= 7'b0; cidx[ 3] <= 7'b0;
            cidx[ 4] <= 7'b0; cidx[ 5] <= 7'b0; cidx[ 6] <= 7'b0; cidx[ 7] <= 7'b0;
            cidx[ 8] <= 7'b0; cidx[ 9] <= 7'b0; cidx[10] <= 7'b0; cidx[11] <= 7'b0;
            cidx[12] <= 7'b0; cidx[13] <= 7'b0; cidx[14] <= 7'b0; cidx[15] <= 7'b0;
            cidx[16] <= 7'b0; cidx[17] <= 7'b0; cidx[18] <= 7'b0; cidx[19] <= 7'b0;
            cidx[20] <= 7'b0; cidx[21] <= 7'b0; cidx[22] <= 7'b0; cidx[23] <= 7'b0;
            cidx[24] <= 7'b0; cidx[25] <= 7'b0; cidx[26] <= 7'b0; cidx[27] <= 7'b0;
            cidx[28] <= 7'b0; cidx[29] <= 7'b0; cidx[30] <= 7'b0; cidx[31] <= 7'b0;
        end
        else if (c2h_byp_out_0_ready & c2h_byp_out_0_valid)
        begin
            cidx[out_data_qid] <= c2h_byp_out_0_cidx[6:0];
        end
    end

    always @ (posedge axi_aclk)
    begin
        if (~axi_aresetn)
        begin
            used_cidx[ 0] <= 7'b0; used_cidx[ 1] <= 7'b0; used_cidx[ 2] <= 7'b0; used_cidx[ 3] <= 7'b0;
            used_cidx[ 4] <= 7'b0; used_cidx[ 5] <= 7'b0; used_cidx[ 6] <= 7'b0; used_cidx[ 7] <= 7'b0;
            used_cidx[ 8] <= 7'b0; used_cidx[ 9] <= 7'b0; used_cidx[10] <= 7'b0; used_cidx[11] <= 7'b0;
            used_cidx[12] <= 7'b0; used_cidx[13] <= 7'b0; used_cidx[14] <= 7'b0; used_cidx[15] <= 7'b0;
            used_cidx[16] <= 7'b0; used_cidx[17] <= 7'b0; used_cidx[18] <= 7'b0; used_cidx[19] <= 7'b0;
            used_cidx[20] <= 7'b0; used_cidx[21] <= 7'b0; used_cidx[22] <= 7'b0; used_cidx[23] <= 7'b0;
            used_cidx[24] <= 7'b0; used_cidx[25] <= 7'b0; used_cidx[26] <= 7'b0; used_cidx[27] <= 7'b0;
            used_cidx[28] <= 7'b0; used_cidx[29] <= 7'b0; used_cidx[30] <= 7'b0; used_cidx[31] <= 7'b0;
        end
        else if (c2h_byp_in_st_0_ready & c2h_byp_in_st_0_valid)
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
    assign c2h_byp_in_st_0_addr = {16'b0, s_axis_c2h_byp_in_tdata[47:0]};
    assign c2h_byp_in_st_0_qid = {2'b0, s_axis_c2h_byp_in_tdata[72:64]};
    assign c2h_byp_in_st_0_port_id = s_axis_c2h_byp_in_tdata[77:75];
    assign c2h_byp_in_st_0_pfch_tag = pfch_tag[in_data_qid];
    assign c2h_byp_in_st_0_func = {6'b0, s_axis_c2h_byp_in_tdata[74:73]};

    assign c2h_byp_in_st_0_valid = s_axis_c2h_byp_in_tvalid;
    assign s_axis_c2h_byp_in_tready = c2h_byp_in_st_0_ready;

    assign c2h_byp_in_st_0_error = 1'b0;

    assign c2h_byp_out_0_ready = 1'b1;

    assign pfch_tag[ 0] = pfch_tag_0_lsb[6:0];
    assign pfch_tag[ 1] = pfch_tag_0_lsb[13:8];
    assign pfch_tag[ 2] = pfch_tag_0_lsb[22:16];
    assign pfch_tag[ 3] = pfch_tag_0_lsb[30:24];
    assign pfch_tag[ 4] = pfch_tag_0_msb[6:0];
    assign pfch_tag[ 5] = pfch_tag_0_msb[13:8];
    assign pfch_tag[ 6] = pfch_tag_0_msb[22:16];
    assign pfch_tag[ 7] = pfch_tag_0_msb[30:24];

    assign pfch_tag[ 8] = pfch_tag_1_lsb[6:0];
    assign pfch_tag[ 9] = pfch_tag_1_lsb[13:8];
    assign pfch_tag[10] = pfch_tag_1_lsb[22:16];
    assign pfch_tag[11] = pfch_tag_1_lsb[30:24];
    assign pfch_tag[12] = pfch_tag_1_msb[6:0];
    assign pfch_tag[13] = pfch_tag_1_msb[13:8];
    assign pfch_tag[14] = pfch_tag_1_msb[22:16];
    assign pfch_tag[15] = pfch_tag_1_msb[30:24];

    assign pfch_tag[16] = pfch_tag_2_lsb[6:0];
    assign pfch_tag[17] = pfch_tag_2_lsb[13:8];
    assign pfch_tag[18] = pfch_tag_2_lsb[22:16];
    assign pfch_tag[19] = pfch_tag_2_lsb[30:24];
    assign pfch_tag[20] = pfch_tag_2_msb[6:0];
    assign pfch_tag[21] = pfch_tag_2_msb[13:8];
    assign pfch_tag[22] = pfch_tag_2_msb[22:16];
    assign pfch_tag[23] = pfch_tag_2_msb[30:24];

    assign pfch_tag[24] = pfch_tag_3_lsb[6:0];
    assign pfch_tag[25] = pfch_tag_3_lsb[13:8];
    assign pfch_tag[26] = pfch_tag_3_lsb[22:16];
    assign pfch_tag[27] = pfch_tag_3_lsb[30:24];
    assign pfch_tag[28] = pfch_tag_3_msb[6:0];
    assign pfch_tag[29] = pfch_tag_3_msb[13:8];
    assign pfch_tag[30] = pfch_tag_3_msb[22:16];
    assign pfch_tag[31] = pfch_tag_3_msb[30:24];

    always @ (posedge axi_aclk) begin
        if (~axi_aresetn)
            aw_pkt_cnt <= 32'b0;
        else if (s_axis_c2h_byp_in_tvalid & s_axis_c2h_byp_in_tready)
            aw_pkt_cnt <= aw_pkt_cnt + 32'b1;
    end

endmodule
