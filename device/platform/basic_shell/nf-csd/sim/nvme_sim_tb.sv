`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06.11.2023 04:17:03
// Design Name: 
// Module Name: nvme_sim_tb
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////

import axi_vip_pkg::*;
import compute_sim_axi_vip_0_0_pkg::*;
import compute_sim_axi_vip_1_0_pkg::*;

module nvme_sim_tb(

    );
    reg aclk;
    reg aresetn;
    
    bit [127:0] data = 128'h000102030405060708090A0B0C0D0E0F;
    bit [127:0] incr = 128'h10101010101010101010101010101010;
    integer i, j;
     
    xil_axi_resp_t resp;

    xil_axi_data_beat  ruser;
    bit[63:0]  addr, base_addr;
    bit[4095:0] data;

    
    compute_sim UUT(
      .aclk_0(aclk),
      .aresetn_0(aresetn)
    );

    always #2ns aclk = ~aclk; // 250MHz
    
    compute_sim_axi_vip_1_0_mst_t master_agent;
    compute_sim_axi_vip_0_0_slv_mem_t slave_agent;
    
      // Reset
    initial begin
        //Assert the reset
        aclk = 0;
        aresetn = 0;
        #102ns
        // Release the reset
        aresetn = 1;
        #900ns;
    end

    initial begin
        
        // Step 4 - Create a new agent
        master_agent = new("master agent", UUT.axi_vip_1.inst.IF);
        slave_agent = new("slave agent", UUT.axi_vip_0.inst.IF);

        // Step 5 - Start the agent
        master_agent.start_master();
        slave_agent.start_slave();

        for (i = 0; i < 4; i++) begin
            data = 128'hFFFF_0001_0000_0001_0000_0000_0002_1F01; // OP SQE
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i, data);
            # 5ns ;

            data = 128'h0000_0000_0000_0000_0000_000A_FC7F_F000; // PRP2 PRP1
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 16, data);
            # 5ns ;

            /*data = 128'h8421_0004_2231_8000_FF00_0001_0000_0000; // PRP1 OP
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 32, data);
            # 5ns ;

            data = 128'h0000_0000_0000_0000_8421_0000_0001_d000; // OP PRP2
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 48, data);
            # 5ns ;*/
            data = 128'h0000_000A_FC7F_F000_FF00_0001_0000_0000; // PRP1 OP
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 32, data);
            # 5ns ;

            data = 128'h0000_0000_0000_0000_0000_0000_0000_0000; // OP PRP2
            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 48, data);
            # 5ns ;

//            data = 128'h8421_0000_0001_d000_8421_0004_2231_8000; // PRP2 PRP1
//            slave_agent.mem_model.backdoor_memory_write(64'h1000_0000 + 64'h0200_0000 * i + 64, data);
//            # 5ns ;
            
            // for (j = 1; j < 8; i++) begin
            //     slave_agent.mem_model.backdoor_memory_write(64'h1100_0000 + 64'h0200_0000 * i + 16 * j, 0);
            //     # 10ns ;
            // end
        end

        slave_agent.mem_model.backdoor_memory_write(64'h2000_0000, 128'h000102030405060708090A0B0C0D0E0F);
        # 5ns ;
        slave_agent.mem_model.backdoor_memory_write(64'h2001_0000, 128'h0F0E0D0C0B0A09080706050403020100);

//        Wait for the reset to be released
        #100ns ;
        // data 
        // master_agent.AXI4LITE_WRITE_BURST(
        //     16'd0, // addr
        //     0, // prot
        //     64'h1000_0000, // rdata
        //     resp // rresp
        // );

        //vip1 send aw/w to transfer compute sqe
        for (i = 0; i < 4; i++) begin
            // SQ start
            # 10ns ;
            master_agent.AXI4LITE_WRITE_BURST(
                16'd8 * i, // addr
                0, // prot
                64'h1000_0000 + 64'h0200_0000 * i, // rdata
                resp // rresp
            );
            // CQ start
            # 10ns ;
            master_agent.AXI4LITE_WRITE_BURST(
                16'd32 + 16'd8 * i, // addr
                0, // prot
                64'h1100_0000 + 64'h0200_0000 * i, // rdata
                resp // rresp
            );
        end
        
        #100ns ;
        
//        000020000000 40 8 20000

        i = 0;
//        for (i = 0; i < 4; i++) begin
            master_agent.AXI4LITE_WRITE_BURST(
                16'd64 + 16'd8 * i, // addr
                0, // prot
                64'h1, // rdata
                resp // rresp
            );
            # 10ns ;
//        end

        // Reset
        // # 20us ;
        // master_agent.AXI4LITE_WRITE_BURST(
        //     16'd160, // addr
        //     0, // prot
        //     64'h1, // rdata
        //     resp // rresp
        // );
        // # 10ns ;
        // // Reset
        // master_agent.AXI4LITE_WRITE_BURST(
        //     16'd160, // addr
        //     0, // prot
        //     64'h0, // rdata
        //     resp // rresp
        // );

        // for (i = 0; i < 4; i++) begin
        //     // SQ start
        //     # 10ns ;
        //     master_agent.AXI4LITE_WRITE_BURST(
        //         16'd8 * i, // addr
        //         0, // prot
        //         64'h1000_0000 + 64'h0200_0000 * i, // rdata
        //         resp // rresp
        //     );
        //     // CQ start
        //     # 10ns ;
        //     master_agent.AXI4LITE_WRITE_BURST(
        //         16'd32 + 16'd8 * i, // addr
        //         0, // prot
        //         64'h1100_0000 + 64'h0200_0000 * i, // rdata
        //         resp // rresp
        //     );
        // end
    
      end


//    master_agent.start_master();
endmodule
