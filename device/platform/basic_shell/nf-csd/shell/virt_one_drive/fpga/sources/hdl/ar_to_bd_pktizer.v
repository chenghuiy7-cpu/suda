module ar_to_bd_pktizer (
    //AXI-Stream slave I/F for AR-to-stream request input
  s_axis_ar_req_tdata,
  s_axis_ar_req_tlast,
  s_axis_ar_req_tkeep,
  s_axis_ar_req_tvalid,
  s_axis_ar_req_tready,

    //AXI-Stream master I/F to BD format data output
  m_axis_h2c_byp_st_tdata,
  m_axis_h2c_byp_st_tlast,
  m_axis_h2c_byp_st_tkeep,
  m_axis_h2c_byp_st_tvalid,
  m_axis_h2c_byp_st_tready,
);

//AXI-Stream master I/F for AR-to-stream request output
input  [ 95 : 0]                   s_axis_ar_req_tdata;
input                              s_axis_ar_req_tlast;
input  [ 11 : 0]                   s_axis_ar_req_tkeep;
input                              s_axis_ar_req_tvalid;
output                             s_axis_ar_req_tready;

//AXI-Stream master I/F to BD format data output
output  [ 127 : 0]                 m_axis_h2c_byp_st_tdata;
output                             m_axis_h2c_byp_st_tlast;
output  [ 15 : 0]                  m_axis_h2c_byp_st_tkeep;
output                             m_axis_h2c_byp_st_tvalid;
input                              m_axis_h2c_byp_st_tready;

/*encapsulation of an AR request as a AXI-Stream packet
 * 96-bit length single-beat packet
 * 95:78 Reserved
 * 77:75 3-bit ARID
 * 74:67 8-bit AR burst length
 * 66:64 3-bit AR burst size (4, indicating 128-bit)
 * 63: 0 64-bit AR address
 * 63:58 8-bit QID
 * 57:48 8-bit CID
 * 47:0 48-bit physical addr
 */
wire  [ 95 : 0 ]   ar_req_in;
wire  [  8 : 0 ]   burst_len;

assign ar_req_in = {96{s_axis_ar_req_tvalid}} & s_axis_ar_req_tdata;
assign burst_len = {1'b0, ar_req_in[74:67]} + 9'd1;

/* Encapsulation of an AR request as a AXI-Stream packet
 * 96-bit length single-beat packet
 * 95:78 Reserved
 * 77:75 3-bit ARID
 * 74:67 8-bit AR burst length
 * 66:64 3-bit AR burst size (4, indicating 128-bit)
 * 63: 0 64-bit AR address
 * 63:63 Always 1 for AXI routing
 * 62:61 2-bit FN ID (4 FNs)
 * 60:58 3-bit QID (8 queues)
 * 57:48 10-bit CID (1024 commands)
 * 47:0 48-bit physical addr
 */

wire [ 63 : 0 ]     h2c_byp_in_st_addr;
wire [ 15 : 0 ]     h2c_byp_in_st_len;
wire [  2 : 0 ]     h2c_byp_in_st_port_id;
wire [  1 : 0 ]     h2c_byp_in_st_fn_id;
wire [  1 : 0 ]     h2c_byp_in_st_drive_id;
wire [  8 : 0 ]     h2c_byp_in_st_qid;

assign h2c_byp_in_st_addr    = {24'd0, ar_req_in[39:0]};
assign h2c_byp_in_st_len     = {3'd0, burst_len, 4'd0}; /*FIXME: assume each data beat is 128-bit*/
assign h2c_byp_in_st_fn_id   = ar_req_in[62:61];
assign h2c_byp_in_st_qid     = {6'd0,  ar_req_in[60:58]} + 9'd8; // Data QID range: [9, 16]
assign h2c_byp_in_st_port_id = ar_req_in[77:75];
assign h2c_byp_in_st_drive_id = ar_req_in[80:79];

assign m_axis_h2c_byp_st_tdata  = {32'b0,
                                  h2c_byp_in_st_drive_id,    //95:94
                                  h2c_byp_in_st_len,          //93:78
                                  h2c_byp_in_st_port_id,      //77:75
                                  h2c_byp_in_st_fn_id,        //74:73
                                  h2c_byp_in_st_qid,          //72:64
                                  h2c_byp_in_st_addr};        //63:0
assign m_axis_h2c_byp_st_tkeep  = 16'hFFFF;
assign m_axis_h2c_byp_st_tvalid = s_axis_ar_req_tvalid;
assign m_axis_h2c_byp_st_tlast  = s_axis_ar_req_tlast;
assign s_axis_ar_req_tready = m_axis_h2c_byp_st_tready;

endmodule