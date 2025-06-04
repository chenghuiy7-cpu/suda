module axis_r_tvalid_gating (
    input                     aclk,
    input                     aresetn,
        
    input 						        m_axis_ic_r_tvalid0,
    input 						        m_axis_ic_r_tvalid1,
    input 						        m_axis_ic_r_tvalid2,
    input 						        m_axis_ic_r_tvalid3,
    input 						        m_axis_ic_r_tvalid4,
    input 						        m_axis_ic_r_tvalid5,
    input 						        m_axis_ic_r_tvalid6,
    input 						        m_axis_ic_r_tvalid7,
    input 						        m_axis_ic_r_tready0,
    input 						        m_axis_ic_r_tready1,
    input 						        m_axis_ic_r_tready2,
    input 						        m_axis_ic_r_tready3,
    input 						        m_axis_ic_r_tready4,
    input 						        m_axis_ic_r_tready5,
    input 						        m_axis_ic_r_tready6,
    input 						        m_axis_ic_r_tready7,
    input 						        m_axis_ic_r_tlast0,
    input 						        m_axis_ic_r_tlast1,
    input 						        m_axis_ic_r_tlast2,
    input 						        m_axis_ic_r_tlast3,
    input 						        m_axis_ic_r_tlast4,
    input 						        m_axis_ic_r_tlast5,
    input 						        m_axis_ic_r_tlast6,
    input 						        m_axis_ic_r_tlast7,
    output 					          r_gating0,
    output 					          r_gating1,
    output 					          r_gating2,
    output 					          r_gating3,
    output 					          r_gating4,
    output 					          r_gating5,
    output 					          r_gating6,
    output 					          r_gating7
);

    reg   [2:0]           r_pt;
    wire 	[7:0]						m_axis_ic_r_tvalid;
    wire 	[7:0]						m_axis_ic_r_tready;
    wire 	[7:0]						m_axis_ic_r_tlast;
    wire 	[7:0]					 	m_axis_ic_r_submitted;
    wire  [7:0]           axis_r_gating;

    onehot_encoder enc(
        .in(r_pt),
        .out(axis_r_gating)
    );

    assign m_axis_ic_r_submitted = m_axis_ic_r_tready & m_axis_ic_r_tvalid & m_axis_ic_r_tlast;
    
    assign m_axis_ic_r_tready = {
      m_axis_ic_r_tready7,
      m_axis_ic_r_tready6,
      m_axis_ic_r_tready5,
      m_axis_ic_r_tready4,
      m_axis_ic_r_tready3,
      m_axis_ic_r_tready2,
      m_axis_ic_r_tready1,
      m_axis_ic_r_tready0
    };
    
    assign m_axis_ic_r_tvalid = {
      m_axis_ic_r_tvalid7,
      m_axis_ic_r_tvalid6,
      m_axis_ic_r_tvalid5,
      m_axis_ic_r_tvalid4,
      m_axis_ic_r_tvalid3,
      m_axis_ic_r_tvalid2,
      m_axis_ic_r_tvalid1,
      m_axis_ic_r_tvalid0
    };
    
    assign m_axis_ic_r_tlast = {
      m_axis_ic_r_tlast7,
      m_axis_ic_r_tlast6,
      m_axis_ic_r_tlast5,
      m_axis_ic_r_tlast4,
      m_axis_ic_r_tlast3,
      m_axis_ic_r_tlast2,
      m_axis_ic_r_tlast1,
      m_axis_ic_r_tlast0
    };
    
    assign r_gating0 = ~axis_r_gating[0];
    assign r_gating1 = ~axis_r_gating[1];
    assign r_gating2 = ~axis_r_gating[2];
    assign r_gating3 = ~axis_r_gating[3];
    assign r_gating4 = ~axis_r_gating[4];
    assign r_gating5 = ~axis_r_gating[5];
    assign r_gating6 = ~axis_r_gating[6];
    assign r_gating7 = ~axis_r_gating[7];

    always @ (posedge aclk)
    begin
      if (~aresetn)
        r_pt <= 3'b0;
      else if(m_axis_ic_r_submitted[r_pt])
        r_pt <= r_pt + 3'b1;
    end

endmodule