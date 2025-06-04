`define INIT    6'b000001
`define WAIT_AW 6'b000010
`define SEND_AW 6'b000100
`define SEND_W 6'b001000
`define WAIT_W_LAST 6'b010000
`define SEND_W_LAST 6'b100000

module axis_w_merger_fake (
    input axi_aclk,
    input axi_aresetn,
    
    input [95:0]                        s_axis_aw_req_tdata,
    input                               s_axis_aw_req_tlast,
    input [7:0]                         s_axis_aw_req_tdest,
    input [7:0]                         s_axis_aw_req_tid,
    input [11:0]                        s_axis_aw_req_tkeep,
    input                               s_axis_aw_req_tvalid,
    output                              s_axis_aw_req_tready,

    input [127:0]                       s_axis_w_tdata,
    input                               s_axis_w_tlast,
    input [7:0]                         s_axis_w_tdest,
    input [7:0]                         s_axis_w_tid,
    input [15:0]                        s_axis_w_tkeep,
    input [15:0]                        s_axis_w_tuser,
    input                               s_axis_w_tvalid,
    output                              s_axis_w_tready,

    output [127:0]                      m_axis_tdata,
    output                              m_axis_tlast,
    output [7:0]                        m_axis_tdest,
    output [7:0]                        m_axis_tid,
    output [15:0]                       m_axis_tkeep,
    output [15:0]                       m_axis_tuser,
    output                              m_axis_tvalid,
    input                               m_axis_tready,

    output reg [31:0]                   aww_pkt_cnt,
    output reg [31:0]                   burst_len_cnt
);


    reg [5:0] current_state;
    reg [5:0] next_state;

    //96+8+8+12
    reg [123:0] reg_aw;

    wire reg_aw_wen;


    wire [95:0]                       m_axis_aw_req_tdata;
    wire [7:0]                        m_axis_aw_req_tdest;
    wire [7:0]                        m_axis_aw_req_tid;
    wire [11:0]                       m_axis_aw_req_tkeep;

    wire [127:0]                      m_axis_w_tdata;
    wire [7:0]                        m_axis_w_tdest;
    wire [7:0]                        m_axis_w_tid;
    wire [15:0]                       m_axis_w_tkeep;
    wire [15:0]                       m_axis_w_tuser;

    // pkt cnt


    always @ (posedge axi_aclk) begin
        if (~axi_aresetn)
            burst_len_cnt <= 32'b0;
        else if (m_axis_tvalid & m_axis_tready)
            burst_len_cnt <= burst_len_cnt + 32'b1;
    end

    //reg for the first aw and the last w
    always @ (posedge axi_aclk) begin
        if (~axi_aresetn)
            reg_aw <= 124'b0;
        else if (reg_aw_wen && current_state == `WAIT_AW)
            reg_aw <= {m_axis_aw_req_tdata,    //123:28
                       s_axis_aw_req_tdest,    //27:20
                       s_axis_aw_req_tid,      //19:12
                       s_axis_aw_req_tkeep};   //11:0
    end


    // state machine
    always @ (posedge axi_aclk) begin
        if (~axi_aresetn)
            current_state <= `INIT;
        else
            current_state <= next_state;
    end

    always @ (*) begin
        case (current_state)
            `INIT:
                next_state = `WAIT_AW;
            `WAIT_AW:
                if (reg_aw_wen)
                    next_state = `SEND_AW;
                else
                    next_state = `WAIT_AW;
            `SEND_AW:
                if (m_axis_tvalid & m_axis_tready)
                    next_state = `SEND_W;
                else
                    next_state = `SEND_AW;
            `SEND_W:
                if (m_axis_tvalid & m_axis_tready & m_axis_tlast)
                    next_state = `WAIT_AW;
                else
                    next_state = `SEND_W;
            default: 
                next_state = `INIT;
        endcase
    end
    
    assign reg_aw_wen = s_axis_aw_req_tvalid && s_axis_aw_req_tready;


    assign m_axis_w_tdata = s_axis_w_tdata;
    assign m_axis_w_tdest = s_axis_w_tdest;
    assign m_axis_w_tid = s_axis_w_tid;
    assign m_axis_w_tkeep = s_axis_w_tkeep;
    assign m_axis_w_tuser = 16'd4096;
    assign s_axis_w_tready = ((current_state == `SEND_W) && m_axis_tready);

    assign m_axis_aw_req_tdata[66:0] = s_axis_aw_req_tdata[66:0];
    //assign m_axis_aw_req_tdata[74:67] = 8'd255; // 4096 / 16 = 256
    assign m_axis_aw_req_tdata[74:67] = s_axis_aw_req_tdata[74:67];
    assign m_axis_aw_req_tdata[80:75] = s_axis_aw_req_tdata[80:75];
    assign m_axis_aw_req_tdata[95:81] = s_axis_aw_req_tdata[95:81];
    //assign m_axis_aw_req_tdata[95:81] = 15'b0;
    assign m_axis_aw_req_tdest = s_axis_aw_req_tdest;
    assign m_axis_aw_req_tid = s_axis_aw_req_tid;
    assign m_axis_aw_req_tkeep = s_axis_aw_req_tkeep;
    assign s_axis_aw_req_tready = current_state == `WAIT_AW;
                            
    assign m_axis_tdata = {128{current_state == `SEND_AW }} & {32'b0, reg_aw[123:28]} |
                          {128{current_state != `SEND_AW }} & m_axis_w_tdata          ;

    assign m_axis_tdest = {8{current_state == `SEND_AW }} & reg_aw[27:20] |
                          {8{current_state != `SEND_AW }} & m_axis_w_tdest;

    assign m_axis_tid = {8{current_state == `SEND_AW }} & reg_aw[19:12] |
                        {8{current_state != `SEND_AW }} & m_axis_w_tid  ;

    assign m_axis_tkeep =  16'hFFFF;

    assign m_axis_tuser = m_axis_w_tuser;

    assign m_axis_tvalid = (current_state == `SEND_AW    )    ||   //aw valid
                           ((current_state == `SEND_W     ) &&  s_axis_w_tvalid);  //0-31 w valid

    assign m_axis_tlast = s_axis_w_tlast&&(current_state == `SEND_W);

    always @ (posedge axi_aclk) begin
        if (~axi_aresetn)
            aww_pkt_cnt <= 32'b0;
        else if (m_axis_tvalid & m_axis_tready & m_axis_tlast)
            aww_pkt_cnt <= aww_pkt_cnt + 32'b1;
    end
    
endmodule
