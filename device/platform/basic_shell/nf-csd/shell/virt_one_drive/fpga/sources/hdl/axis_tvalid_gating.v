module axis_tvalid_gating (
    input                               s_axib_aclk,
    input                               s_axib_aresetn,

    input                               h2c_is_data_queue,
        
    input 						        m_axis_ic_in_tvalid0,
    input 						        m_axis_ic_in_tvalid1,
    input 						        m_axis_ic_in_tvalid2,
    input 						        m_axis_ic_in_tvalid3,
    input 						        m_axis_ic_in_tvalid4,
    input 						        m_axis_ic_in_tvalid5,
    input 						        m_axis_ic_in_tvalid6,
    input 						        m_axis_ic_in_tvalid7,
    input 						        m_axis_ic_in_tready0,
    input 						        m_axis_ic_in_tready1,
    input 						        m_axis_ic_in_tready2,
    input 						        m_axis_ic_in_tready3,
    input 						        m_axis_ic_in_tready4,
    input 						        m_axis_ic_in_tready5,
    input 						        m_axis_ic_in_tready6,
    input 						        m_axis_ic_in_tready7,
    input 						        m_axis_ic_in_tlast0,
    input 						        m_axis_ic_in_tlast1,
    input 						        m_axis_ic_in_tlast2,
    input 						        m_axis_ic_in_tlast3,
    input 						        m_axis_ic_in_tlast4,
    input 						        m_axis_ic_in_tlast5,
    input 						        m_axis_ic_in_tlast6,
    input 						        m_axis_ic_in_tlast7,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive0,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive1,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive2,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive3,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive4,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive5,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive6,
    input  [ 1 : 0 ]			        m_axis_ic_in_drive7,
    input  [ 2 : 0 ]                    s_axis_h2c_tuser,
    input                               s_axis_h2c_tvalid,
    input                               s_axis_h2c_tready,
    input                               s_axis_h2c_tlast,
    output 					            ar_req_gating0,
    output 					            ar_req_gating1,
    output 					            ar_req_gating2,
    output 					            ar_req_gating3,
    output 					            ar_req_gating4,
    output 					            ar_req_gating5,
    output 					            ar_req_gating6,
    output 					            ar_req_gating7,
    output [ 7 : 0 ]                    ar_to_drive,
    output [15:0]                       dbg_ar_drive_map
);

    reg     [7:0]                       axib_ar_req_gating;
    wire 	[7:0]						m_axis_ic_in_tvalid;
    wire 	[7:0]						m_axis_ic_in_tready;
    wire 	[7:0]						m_axis_ic_in_tlast;
    wire 	[7:0]						m_axis_ic_in_req_submitted;
    wire                                s_axis_ic_req_cmpled;
    wire    [7:0]                       arid_onehot;

    reg [1:0] ar_drive_map [7:0];

    onehot_encoder enc(
        .in(s_axis_h2c_tuser),
        .out(arid_onehot)
    );

    assign m_axis_ic_in_req_submitted = m_axis_ic_in_tready & m_axis_ic_in_tvalid & m_axis_ic_in_tlast;
    assign s_axis_ic_req_cmpled = s_axis_h2c_tready & s_axis_h2c_tvalid & s_axis_h2c_tlast;
    
    assign m_axis_ic_in_tready = {
        m_axis_ic_in_tready7,
        m_axis_ic_in_tready6,
        m_axis_ic_in_tready5,
        m_axis_ic_in_tready4,
        m_axis_ic_in_tready3,
        m_axis_ic_in_tready2,
        m_axis_ic_in_tready1,
        m_axis_ic_in_tready0
    };
    
    assign m_axis_ic_in_tvalid = {
        m_axis_ic_in_tvalid7,
        m_axis_ic_in_tvalid6,
        m_axis_ic_in_tvalid5,
        m_axis_ic_in_tvalid4,
        m_axis_ic_in_tvalid3,
        m_axis_ic_in_tvalid2,
        m_axis_ic_in_tvalid1,
        m_axis_ic_in_tvalid0
    };
    
    assign m_axis_ic_in_tlast = {
        m_axis_ic_in_tlast7,
        m_axis_ic_in_tlast6,
        m_axis_ic_in_tlast5,
        m_axis_ic_in_tlast4,
        m_axis_ic_in_tlast3,
        m_axis_ic_in_tlast2,
        m_axis_ic_in_tlast1,
        m_axis_ic_in_tlast0
    };
    
    assign ar_req_gating0 = axib_ar_req_gating[0];
    assign ar_req_gating1 = axib_ar_req_gating[1];
    assign ar_req_gating2 = axib_ar_req_gating[2];
    assign ar_req_gating3 = axib_ar_req_gating[3];
    assign ar_req_gating4 = axib_ar_req_gating[4];
    assign ar_req_gating5 = axib_ar_req_gating[5];
    assign ar_req_gating6 = axib_ar_req_gating[6];
    assign ar_req_gating7 = axib_ar_req_gating[7];

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            axib_ar_req_gating <= 8'b00000000;

        // Gating on when one AR transaction is drained from the request FIFO
        else if(m_axis_ic_in_req_submitted)
            axib_ar_req_gating <= axib_ar_req_gating | m_axis_ic_in_req_submitted;

        // Gating off when the entire R packets are received
        else if (h2c_is_data_queue & s_axis_ic_req_cmpled)
            axib_ar_req_gating <= axib_ar_req_gating & ~arid_onehot; // One-hot encoding
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[0] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[0])
            ar_drive_map[0] <= m_axis_ic_in_drive0;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[1] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[1])
            ar_drive_map[1] <= m_axis_ic_in_drive1;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[2] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[2])
            ar_drive_map[2] <= m_axis_ic_in_drive2;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[3] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[3])
            ar_drive_map[3] <= m_axis_ic_in_drive3;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[4] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[4])
            ar_drive_map[4] <= m_axis_ic_in_drive4;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[5] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[5])
            ar_drive_map[5] <= m_axis_ic_in_drive5;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[6] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[6])
            ar_drive_map[6] <= m_axis_ic_in_drive6;
    end

    always @ (posedge s_axib_aclk)
    begin
        if (s_axib_aresetn == 1'b0)
            ar_drive_map[7] <= 2'b0;
        else if (m_axis_ic_in_req_submitted[7])
            ar_drive_map[7] <= m_axis_ic_in_drive7;
    end

    assign ar_to_drive = {6'b0, ar_drive_map[s_axis_h2c_tuser]};

    assign dbg_ar_drive_map = {ar_drive_map[7],
                               ar_drive_map[6],
                               ar_drive_map[5],
                               ar_drive_map[4],
                               ar_drive_map[3],
                               ar_drive_map[2],
                               ar_drive_map[1],
                               ar_drive_map[0]};

endmodule