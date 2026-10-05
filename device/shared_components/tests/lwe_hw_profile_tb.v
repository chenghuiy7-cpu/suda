`timescale 1ns/1ps
module lwe_hw_profile_tb;
    reg clk=0; always #2 clk=~clk;
    reg resetn=0, start=0, done=0, iv=0, ir=0;
    reg [7:0] iu=0;
    reg [511:0] sd=0; reg sv=0, mr=1;
    reg [63:0] sk=~64'b0, ss=~64'b0;
    reg [7:0] su=0, si=7, sx=8'h35; reg sl=0;
    wire sr,mv,ml; wire[511:0] md; wire[63:0] mk,ms; wire[7:0] mu,mi,mx;
    lwe_axis_profile dut(.ap_clk(clk),.slot_enable(resetn),.op_start(start),.op_done(done),
        .input_valid(iv),.input_ready(ir),.input_user(iu),
        .s_axis_TDATA(sd),.s_axis_TVALID(sv),.s_axis_TREADY(sr),.s_axis_TKEEP(sk),
        .s_axis_TSTRB(ss),.s_axis_TUSER(su),.s_axis_TLAST(sl),.s_axis_TID(si),.s_axis_TDEST(sx),
        .m_axis_TDATA(md),.m_axis_TVALID(mv),.m_axis_TREADY(mr),.m_axis_TKEEP(mk),
        .m_axis_TSTRB(ms),.m_axis_TUSER(mu),.m_axis_TLAST(ml),.m_axis_TID(mi),.m_axis_TDEST(mx));
    integer ticks=0, start_tick=0, nin=0,nout=0,nwait=0,owait=0;
    integer runs=0; reg active=0, input_finished=0, snapped=0;
    reg[511:0] finish_saved;
    always @(posedge clk) begin
        ticks=ticks+1;
        if (!resetn) begin active=0; snapped=0; end
        else begin
            if (sr !== mr || mv !== sv || mu !== su || ml !== sl || mi !== si || mx !== sx)
                $fatal(1,"profiler changed handshake/sidebands");
            if (sv && su[7:4]==0 && (md !== sd || mk !== sk || ms !== ss))
                $fatal(1,"profiler changed payload");
            if (start && !active) begin
                active=1; snapped=0; input_finished=0; start_tick=ticks;
                nin=0;nout=0;nwait=0;owait=0;
            end
            if (active && !snapped) begin
                if (iv && ir && iu[7:4]==0) nin=nin+1;
                if (ir && !iv && !input_finished && ticks>start_tick) nwait=nwait+1;
                if (sv && !sr && su[7:4]==0 && ticks>start_tick) owait=owait+1;
                if (iv && ir && iu[7:4]!=0) input_finished=1;
                if (sv && sr && su[7:4]==0) nout=nout+1;
            end
            if (sv && su[7:4]!=0) begin
                if (md[127:0] !== sd[127:0] || md[191:128] !== 64'h313030465045574c ||
                    mk !== ~64'b0 || ms !== ~64'b0) $fatal(1,"bad finish format");
                if (!snapped) begin
                    if (md[255:192] !== ticks-start_tick || md[319:256] !== nwait ||
                        md[383:320] !== owait || md[447:384] !== nin || md[511:448] !== nout)
                        $fatal(1,"counter mismatch: cycles=%d expected=%d iw=%d/%d ow=%d/%d in=%d/%d out=%d/%d",
                            md[255:192],ticks-start_tick,md[319:256],nwait,md[383:320],owait,
                            md[447:384],nin,md[511:448],nout);
                    finish_saved=md; snapped=1; runs=runs+1;
                end else if(md !== finish_saved) $fatal(1,"finish changed under backpressure");
            end
            if (done) active=0;
        end
    end
    task tick; begin @(negedge clk); end endtask
    integer r,i;
    initial begin
        repeat(3) tick(); resetn=1;
        for(r=0;r<3;r=r+1) begin
            tick();start=1;
            tick();start=0; ir=1;
            repeat(3+r) tick();
            iv=1; iu=0;
            repeat(10+r) tick();
            iu=8'hff; tick();iv=0;ir=0;
            sd={8{64'h123456789abcdef0}};sv=1;su=0;sl=0;mr=0;
            repeat(2+r) tick();mr=1;
            repeat(7+r) tick();
            su=8'hff;sl=1;sd={384'b0,64'h98765432,64'hfeedbeef01234567};
            sk=64'hffff;ss=sk;mr=0;
            repeat(4) tick();mr=1;
            tick();sv=0;done=1;
            tick();done=0;sk=~64'b0;ss=sk;
            repeat(3) tick();
        end
        if(runs!=3) $fatal(1,"missing runs");
        $display("PASS: LWE hardware profiler counters, payload passthrough, finish backpressure, repeated runs");
        $finish;
    end
endmodule
