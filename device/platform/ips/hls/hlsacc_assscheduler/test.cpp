#include "hlsacc_assscheduler.hpp"
#include "iostream"
int main()
{
    AssSchedCmd sq[32];
    AssSchedRet cq[32];

    ap_uint<SQCQ_PTR_WIDTH> sq_header;
    ap_uint<SQCQ_PTR_WIDTH> sq_tailer;
    ap_uint<SQCQ_PTR_WIDTH> cq_header;
    ap_uint<SQCQ_PTR_WIDTH> cq_tailer;

    CtrlReq ctrl_req_to_acc("CTRLREQFIFO");
	CtrlRsp ctrl_rsp_from_acc("CTRLRSPFIFO");
	Context_Cmd acc_context_save("ACCSAVEFIFO");
	Context_Cmd acc_context_recovery("ACCRECOVERYFIFO");
	Acc_Data acc_context_data_send_to_acc("CONTOACC");
	Acc_Data acc_context_data_send_to_datamover("CONTODATAMOVER");
	Acc_Data acc_context_data_recv_from_datamover("CONRECVDATAMOVER");
    hls::stream<ap_uint<TDEST_WIDTH>> operator_done_signal("OPERATOR_SIGNAL");

	AssSchedCmd FakeSQ;

	std::cout << "APPLY OPS A" << std::endl;
	FakeSQ.header.cid = 0;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A00000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 1 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 0;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	//FakeSQ.apply_ops_payload1.context_address = 0x44A00000;
	//std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	//FakeSQ.apply_ops_payload2.connections_num = 1;
	//FakeSQ.apply_ops_payload2.connections[0].from = 0;
	//FakeSQ.apply_ops_payload2.connections[0].to = 3 << 4 | 0;
	//std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "pause REQUEST A" << std::endl;
	FakeSQ.header.cid = 3;
	FakeSQ.header.opc = SUSPEND_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	memset(&FakeSQ,0,8);
	FakeSQ.generic_ops_payload.op_lists[0] = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "APPLY OPS A" << std::endl;
	FakeSQ.header.cid = 5;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A00000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;


	std::cout << "APPLY OPS B" << std::endl;
	FakeSQ.header.cid = 4;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 2;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A01000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 1 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 2 << 4 | 0;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A01000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 2 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 3 << 4 | 0;
	std::cout << "BINRARY DUMPXXXXX" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "SUSPEND OPS A" << std::endl;
	FakeSQ.header.cid = 2;
	FakeSQ.header.opc = SUSPEND_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.generic_ops_payload.op_lists[0] = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

    std::cout << "RESUME OPS A" << std::endl;
	FakeSQ.header.cid = 3;
	FakeSQ.header.opc = RESUME_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.generic_ops_payload.op_lists[0] = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "APPLY OPS B" << std::endl;
	FakeSQ.header.cid = 4;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A01000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 1 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 0;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "SUSPEND OPS B" << std::endl;
	FakeSQ.header.cid = 5;
	FakeSQ.header.opc = SUSPEND_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.generic_ops_payload.op_lists[0] = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "APPLY OPS A" << std::endl;
	FakeSQ.header.cid = 7;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A00000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 1 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 0;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "APPLY OPS B" << std::endl;
	FakeSQ.header.cid = 8;
	FakeSQ.header.opc = APPLY_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload1.context_address = 0x44A01000;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.apply_ops_payload2.connections_num = 1;
	FakeSQ.apply_ops_payload2.connections[0].from = 1 << 4 | 0;
	FakeSQ.apply_ops_payload2.connections[0].to = 0;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;

	std::cout << "FORCE FREE OPS B"<< std::endl;
	FakeSQ.header.cid = 9;
	FakeSQ.header.opc = FORCE_FREE_OPS;
	FakeSQ.header.ops_num = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;
	FakeSQ.generic_ops_payload.op_lists[0] = 1;
	std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(&FakeSQ))[0] << std::endl;



    /**********
     * 第一次测试
     * 发送请求FORCE_FREE_OPS，发往8个不同的算子
     */
    sq[0].header.cid = 0;
    sq[0].header.opc = FORCE_FREE_OPS;
    sq[0].header.ops_num = 8;
    for(int i=0;i<8;i++)
        sq[1].generic_ops_payload.op_lists[i] = i;
    std::cout << "BINRARY DUMP" << std::hex << ((unsigned long long*)(sq))[0] << " " << std::hex
            << ((unsigned long long*)(sq))[1] << std::endl;
    sq_tailer += 2;
    cq_header = 0;
    sq_header = 0;
    cq_tailer = 0;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(!ctrl_req_to_acc.empty()){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep <<
            std::endl;
            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);

        }
        if(cq_tailer==1){
			std::cout << "CQ CID " << cq[0].header.cid \
			<< "OPS NUM " << cq[0].header.ops_num
			 << "STATE " << cq[0].header.state << std::endl;
			std::cout << "FIRST TEST FORCE FREE PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
     /**********
     * 第二次测试
     * 发送请求FORCE_FREE_OPS，发往8个不同的算子，但是中途两个算子返回错误信号
     */
    sq[2].header.cid = 2;
    sq[2].header.opc = FORCE_FREE_OPS;
    sq[2].header.ops_num = 8;
    for(int i=0;i<8;i++)
        sq[3].generic_ops_payload.op_lists[i] = i;
    sq_tailer += 2;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(!ctrl_req_to_acc.empty()){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep <<
            std::endl;
            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS;
            if(pkt1.id = 3||pkt1.id==5)
                pkt1.data.header.state = FAILED;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);

        }
        if(cq_tailer==2){
			std::cout << "CQ CID " << cq[1].header.cid \
			<< "OPS NUM " << cq[1].header.ops_num
			 << "STATE " << cq[1].header.state << std::endl;
            if(cq[1].header.state==FAILED)
			std::cout << "SECOND TEST FORCE FREE PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
     /**********
     * 第三次测试
     * 发送请求FORCE_FREE_OPS，发往8个不同的算子，但是发送两次，再更新指针
     */
    
    sq[4].header.cid = 4;
    sq[4].header.opc = FORCE_FREE_OPS;
    sq[4].header.ops_num = 8;
    for(int i=0;i<8;i++)
        sq[5].generic_ops_payload.op_lists[i] = i;
    sq[6].header.cid = 6;
    sq[6].header.opc = FORCE_FREE_OPS;
    sq[6].header.ops_num = 8;
    for(int i=0;i<8;i++)
        sq[7].generic_ops_payload.op_lists[i] = i;
    sq_tailer += 4;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(!ctrl_req_to_acc.empty()){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep <<
            std::endl;
            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);

        }
        if(cq_tailer==4){
			std::cout << "CQ CID " << cq[2].header.cid \
			<< "OPS NUM " << cq[2].header.ops_num
			 << "STATE " << cq[2].header.state << std::endl;
            std::cout << "CQ CID " << cq[3].header.cid \
			<< "OPS NUM " << cq[3].header.ops_num
			 << "STATE " << cq[3].header.state << std::endl;
			std::cout << "THIRD TEST FORCE FREE PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
    /**********
     * 第四次测试
     * 发送请求APPLY_OPS，请求三个算子，算子不需要保存上下文，可以直接进行恢复
     */
    sq[8].header.cid = 7;
    sq[8].header.opc = APPLY_OPS;
    sq[8].header.ops_num = 3;
    sq[9].apply_ops_payload1.context_address = 0xB0081000;
    sq[10].apply_ops_payload2.connections_num = 1;
    sq[10].apply_ops_payload2.connections[0].from = 0;
    sq[10].apply_ops_payload2.connections[0].to = 1 << 4 | 0;
    sq[11].apply_ops_payload1.context_address = 0xB0082000;
    sq[12].apply_ops_payload2.connections_num = 1;
    sq[12].apply_ops_payload2.connections[0].from = 1 << 4 | 1;
    sq[12].apply_ops_payload2.connections[0].to = 2 << 4 | 0;
    sq[13].apply_ops_payload1.context_address = 0xB0083000;
    sq[14].apply_ops_payload2.connections_num = 0;
    sq[14].apply_ops_payload2.connections[0].from = 2 << 4;
    sq_tailer += 7;
    unsigned int outstanding_num = 3;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(ctrl_req_to_acc.size() >= 2 && outstanding_num!=0){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            Ctrlreq_Pkt payload = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep << \
            "CONTEXT ADDRESS " << std::hex << pkt.data.header.context_address << \
            "CONNECTNUM " << pkt.data.header.payload_length << \
            std::endl;
            
            std::cout << "CONNECTIONS: FROM " << (int)payload.data.apply_payload.connections[0].from << \
            "TO " << (int)payload.data.apply_payload.connections[0].to << std::endl;

            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS_STAGE1;
            pkt1.data.header.meta.apply_meta.need_save = false;
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            pkt1.data.header.meta.apply_meta.new_context_address = pkt.data.header.context_address;
            pkt1.last = 1;
            pkt1.id = pkt.dest;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);
            outstanding_num --;
        }
        else if(ctrl_req_to_acc.size() >= 1 && outstanding_num == 0){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep << \
            "CONTEXT ADDRESS " << std::hex << pkt.data.header.context_address << \
            "CONNECTNUM " << pkt.data.header.payload_length << \
            std::endl;
            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS;
            pkt1.data.header.meta.apply_meta.need_save = false;
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            //pkt1.data.header.meta.apply_meta.new_context_address = pkt.data.header.context_address;
            pkt1.last = 1;
            pkt1.id = pkt.dest;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);
        }
        if(!acc_context_save.empty()){
            std::cerr << "UNDEFINED ERROR!" << std::endl;
            exit(-1);
        }
        if(!acc_context_recovery.empty()){
            DataMoverCmd cmd = acc_context_recovery.read();
            std::cout << "RECOVERY ADDR " << std::hex << cmd.saddr << std::endl;
            AccContext context;
        
            context.privated_data.stream_buf_add_lists[0] = 0xB0090000;
            context.privated_data.stream_buf_add_lists[1] = 0xB0091000;
            context.privated_data.available_buf_num = 0;
            context.privated_data.descriptor_num = 123;
            context.static_data[0] = 1;
            context.static_data[1] = 2;
            context.static_data[2] = 3;
            ap_uint<512> send_data;
            Acc_Data_Pkt send_pkt;
            for(int i=0;i<34;i++){
                memcpy(&send_data,(ap_uint<512>*)(&context)+i,sizeof(ap_uint<512>));
                send_pkt.data = send_data;
                send_pkt.last = 0;
                send_pkt.keep = 0xffffffffffffffff;
                acc_context_data_recv_from_datamover.write(send_pkt);
            }
            memcpy(&send_data,(ap_uint<512>*)(&context)+34,sizeof(ap_uint<512>));
            send_pkt.data = send_data;
            send_pkt.last = 1;
            acc_context_data_recv_from_datamover.write(send_pkt);
        }
        if(acc_context_data_send_to_acc.size()>=35){
            Acc_Data_Pkt recv_pkt;
            AccContext context;
            ap_uint<512> recv_data;
            for(int i=0;i<35;i++){
               recv_pkt = acc_context_data_send_to_acc.read();
               std::cout << "CONTEXT TDEST" << recv_pkt.dest << std::endl;
               memcpy((ap_uint<512>*)(&context)+i,&(recv_pkt.data),sizeof(ap_uint<512>));
            }            
            assert(recv_pkt.last == 1);
            if(context.privated_data.stream_buf_add_lists[0] != 0xB0090000 || 
                context.privated_data.stream_buf_add_lists[1] != 0xB0091000 ||
                context.privated_data.descriptor_num != 123 ||
                context.static_data[0] != 1||
                context.static_data[1] != 2||
                context.static_data[2] != 3)
            {
                std::cerr << "CONTEXT DATA BROKEN!" << std::endl;
                exit(-1);
            }
        }
        if(cq_tailer==5&&acc_context_data_send_to_acc.empty()){
			std::cout << "CQ CID " << cq[4].header.cid \
			<< "OPS NUM " << cq[4].header.ops_num
			 << "STATE " << cq[4].header.state << std::endl;
			std::cout << "FORTH TEST APPLY PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
    /**********
     * 第五次测试
     * 发送请求APPLY_OPS，请求一个算子，算子不需要保存上下文，但是算子需要从上下文中恢复
     */
    sq[15].header.cid = 15;
    sq[15].header.opc = APPLY_OPS;
    sq[15].header.ops_num = 1;
    sq[16].apply_ops_payload1.context_address = 0xB0081000;
    sq[17].apply_ops_payload2.connections_num = 1;
    sq[17].apply_ops_payload2.connections[0].from = 2 << 4 | 1;
    sq[17].apply_ops_payload2.connections[0].to = 1 << 4 | 0;
    sq_tailer += 3;
    outstanding_num = 1;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(ctrl_req_to_acc.size() >= 2&&outstanding_num==1){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            Ctrlreq_Pkt payload = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep << \
            "CONTEXT ADDRESS " << std::hex << pkt.data.header.context_address << \
            "CONNECTNUM " << pkt.data.header.payload_length << \
            std::endl;
            
            std::cout << "CONNECTIONS: FROM " << (int)payload.data.apply_payload.connections[0].from << \
            "TO " << (int)payload.data.apply_payload.connections[0].to << std::endl;

            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS_STAGE1;
            pkt1.data.header.meta.apply_meta.need_save = false;
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            pkt1.data.header.meta.apply_meta.new_context_address = pkt.data.header.context_address;
            pkt1.id = pkt.dest;
            pkt1.last = 1;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);
            outstanding_num--;
        }
        else if(!ctrl_req_to_acc.empty()&&outstanding_num==0){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            //Ctrlreq_Pkt payload = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep << \
            "CONTEXT ADDRESS " << std::hex << pkt.data.header.context_address << \
            "CONNECTNUM " << pkt.data.header.payload_length << \
            std::endl;
            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS;
            pkt1.last = 1;
            pkt1.keep = 0xffffffffffffffff;
            pkt1.data.header.meta.apply_meta.need_save = false;
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            pkt1.data.header.meta.apply_meta.new_context_address = 0xB0081000;
            pkt1.id = pkt.dest;
            pkt1.last = 1;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);
        }
        if(!acc_context_save.empty()){
            std::cerr << "UNDEFINED ERROR!" << std::endl;
            exit(-1);
        }
        if(!acc_context_recovery.empty()){
            DataMoverCmd cmd = acc_context_recovery.read();
            if(cmd.saddr == 0xB0081000 || cmd.saddr == 0xB0082000 || cmd.saddr == 0xB0083000){
                std::cout << "RECOVERY ADDR " << std::hex << cmd.saddr << std::endl;

                AccContext context;
                context.privated_data.stream_buf_add_lists[0] = 0xB0090000;
                context.privated_data.stream_buf_add_lists[1] = 0xB0091000;
                context.privated_data.available_buf_num = 0;
                context.privated_data.descriptor_num = 123;
                context.static_data[0] = 1;
                context.static_data[1] = 2;
                context.static_data[2] = 3;
                if(cmd.saddr == 0xB0081000)
                {
                    context.privated_data.available_buf_num = 2;
                }
                ap_uint<512> send_data;
                Acc_Data_Pkt send_pkt;
                send_pkt.id = 2;
                for(int i=0;i<34;i++){
                    memcpy(&send_data,&((ap_uint<512>*)(&context))[i],sizeof(ap_uint<512>));
                    send_pkt.data = send_data;
                    send_pkt.last = 0;
                    send_pkt.keep = 0xffffffffffffffff;
                    acc_context_data_recv_from_datamover.write(send_pkt);
                }
                memcpy(&send_data,(ap_uint<512>*)(&context)+34,sizeof(ap_uint<512>));
                send_pkt.data = send_data;
                send_pkt.last = 1;
                acc_context_data_recv_from_datamover.write(send_pkt);
            } else if(cmd.saddr == 0xB0090000 || cmd.saddr == 0xB0091000){
                std::cout << "READ FIFO DATA ADDR" << std::hex << cmd.saddr << std::endl;
                Acc_Data_Pkt send_pkt;
                send_pkt.id = 2;
                if(cmd.saddr == 0xB0090000)
                    for(int i=0;i<PAGE_SIZE/(TDATA_WIDTH/8);i++){
                        send_pkt.data = ap_uint<512>("123456789",16);
                        send_pkt.last = i==(PAGE_SIZE/(TDATA_WIDTH/8) -1);
                        acc_context_data_recv_from_datamover.write(send_pkt);
                    }
                else if(cmd.saddr == 0xB0091000){
                    for(int i=0;i<PAGE_SIZE/(TDATA_WIDTH/8);i++){
                        send_pkt.data = ap_uint<512>("23456789a",16);
                        send_pkt.last = i==(PAGE_SIZE/(TDATA_WIDTH/8) -1);
                        acc_context_data_recv_from_datamover.write(send_pkt);
                    }
                }
            }
        }
        if(acc_context_data_send_to_acc.size()>=(35+2*PAGE_SIZE/(TDATA_WIDTH/8))){
            Acc_Data_Pkt recv_pkt;
            AccContext context;
            ap_uint<512> recv_data;
            for(int i=0;i<35;i++){
               recv_pkt = acc_context_data_send_to_acc.read();
               std::cout << "CONTEXT TDEST" << recv_pkt.dest << std::endl;
               memcpy((ap_uint<512>*)(&context)+i,&(recv_pkt.data),sizeof(ap_uint<512>));
            }            
            assert(recv_pkt.last != 1);
            if(context.privated_data.stream_buf_add_lists[0] != 0xB0090000 || 
                context.privated_data.stream_buf_add_lists[1] != 0xB0091000 ||
                context.privated_data.descriptor_num != 123 ||
                context.static_data[0] != 1||
                context.static_data[1] != 2||
                context.static_data[2] != 3)
            {
                std::cerr << "CONTEXT DATA BROKEN!" << std::endl;
                exit(-1);
            }
            assert(recv_pkt.last != 1);
            for(int i=0;i<PAGE_SIZE/(TDATA_WIDTH/8);i++){
                recv_pkt = acc_context_data_send_to_acc.read();
                if(recv_pkt.data != ap_uint<512>("123456789",16)){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! DATA" << recv_pkt.data << "NEEDED DATA" << 0x123456789 << std::endl;
                }
                if(recv_pkt.dest != 2){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! DEST" << recv_pkt.dest << "NEEDED DEST 2" << std::endl;
                }
                if(recv_pkt.last == 1){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! LAST" << std::endl;
                }
            }
            for(int i=0;i<PAGE_SIZE/(TDATA_WIDTH/8);i++){
                recv_pkt = acc_context_data_send_to_acc.read();
                if(recv_pkt.data != ap_uint<512>("23456789a",16)){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! DATA" << recv_pkt.data << "NEEDED DATA" << 0x23456789a << std::endl;
                }
                if(recv_pkt.dest != 2){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! DEST" << recv_pkt.dest << "NEEDED DEST 2" << std::endl;
                }
                if(recv_pkt.last == 1 && i != ((PAGE_SIZE/(TDATA_WIDTH/8))-1)){
                    std::cerr << "CONTEXT FIFO DATA BROKEN! LAST" << std::endl;
                }
            }
            assert(recv_pkt.last == 1);
        }
        if(cq_tailer==6&&acc_context_data_send_to_acc.empty()){
			std::cout << "CQ CID " << cq[5].header.cid \
			<< "OPS NUM " << cq[5].header.ops_num
			 << "STATE " << cq[5].header.state << std::endl;
			std::cout << "FIFTH TEST APPLY PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
    /**********
     * 第六次测试
     * 发送请求APPLY_OPS，请求三个算子，第一个算子需要保存上下文再读取新的上下文
     * 另外两个算子不需要
     */
    
    sq[18].header.cid = 18;
    sq[18].header.opc = APPLY_OPS;
    sq[18].header.ops_num = 3;
    sq[19].apply_ops_payload1.context_address = 0xB0081000;
    sq[20].apply_ops_payload2.connections_num = 1;
    sq[20].apply_ops_payload2.connections[0].from = 0;
    sq[20].apply_ops_payload2.connections[0].to = 1 << 4 | 0;
    sq[21].apply_ops_payload1.context_address = 0xB0082000;
    sq[22].apply_ops_payload2.connections_num = 1;
    sq[22].apply_ops_payload2.connections[0].from = 1 << 4 | 1;
    sq[22].apply_ops_payload2.connections[0].to = 2 << 4 | 0;
    sq[23].apply_ops_payload1.context_address = 0xB0083000;
    sq[24].apply_ops_payload2.connections_num = 0;
    sq[24].apply_ops_payload2.connections[0].from = 2 << 4 | 0;
    sq_tailer += 7;
    outstanding_num = 3;
    while(true){
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(ctrl_req_to_acc.size() >= 2&&outstanding_num!=0){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();
            Ctrlreq_Pkt payload = ctrl_req_to_acc.read();
            std::cout << "TYPE" << pkt.data.header.type << \
            "DEST" << pkt.dest << "KEEP" << pkt.keep << \
            "CONTEXT ADDRESS " << std::hex << pkt.data.header.context_address << \
            "CONNECTNUM " << pkt.data.header.payload_length << \
            std::endl;
            
            std::cout << "CONNECTIONS: FROM " << (int)payload.data.apply_payload.connections[0].from << \
            "TO " << (int)payload.data.apply_payload.connections[0].to << std::endl;

            Ctrlrsp_Pkt pkt1;
            pkt1.data.header.state = SUCCESS_STAGE1;
            pkt1.data.header.meta.apply_meta.need_save = false;
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            pkt1.data.header.meta.apply_meta.new_context_address = pkt.data.header.context_address;
            pkt1.last = 1;
            pkt1.id = pkt.dest;
            pkt1.keep = 0xffffffffffffffff;
            ctrl_rsp_from_acc.write(pkt1);
            outstanding_num--;
        }else if(!ctrl_req_to_acc.empty()&&outstanding_num==0){
            Ctrlreq_Pkt pkt = ctrl_req_to_acc.read();

            Ctrlrsp_Pkt pkt1;
            //Ctrlreq_Pkt payload = ctrl_req_to_acc.read();

            pkt1.id = pkt.dest;
            pkt1.keep = 0xffffffffffffffff;
            pkt1.data.header.state = SUCCESS;
            
            pkt1.data.header.meta.apply_meta.need_save = false;
            if(pkt.dest == 0){
                pkt1.data.header.meta.apply_meta.need_save = true;
                std::cout << "NEED TO BE SAVED" << std::endl;
            }
            pkt1.data.header.meta.apply_meta.new_bbt = sizeof(AccContext);
            pkt1.data.header.meta.apply_meta.new_context_address = 0xB82001000;
            pkt1.data.header.meta.apply_meta.old_context_address = 0xB20020000;
            pkt1.data.header.meta.apply_meta.old_bbt = STATIC_VAR_SIZE + PAGE_SIZE + 64;
            pkt1.last = 0;
            ctrl_rsp_from_acc.write(pkt1);
            if(pkt.dest == 0){
                pkt1.data.apply_payload.data[0] = 1;
                pkt1.data.apply_payload.data[1] = 0xB2003000;
                pkt1.data.apply_payload.data[2] = 0xB0022000;
                pkt1.data.apply_payload.data[3] = 0;
                pkt1.data.apply_payload.data[4] = 0;
                pkt1.data.apply_payload.data[5] = 0;
                pkt1.data.apply_payload.data[6] = 0;
                pkt1.data.apply_payload.data[7] = 0;
                ctrl_rsp_from_acc.write(pkt1);
                //pkt1.data.apply_payload.data[0] = 123456;
                //pkt1.data.apply_payload.data[1] = 234567;
                //pkt1.data.apply_payload.data[2] = 345678;
                for(int i=0;i<(STATIC_VAR_SIZE+PAGE_SIZE)/(TDATA_WIDTH/8);i++){
                    pkt1.data.apply_payload.data[i%64] = i%8;
                    pkt1.data.apply_payload.data[i%64+1] = i%8;
                    pkt1.data.apply_payload.data[i%64+2] = i%8;
                    pkt1.data.apply_payload.data[i%64+3] = i%8;
                    pkt1.data.apply_payload.data[i%64+4] = i%8;
                    pkt1.data.apply_payload.data[i%64+5] = i%8;
                    pkt1.data.apply_payload.data[i%64+6] = i%8;
                    pkt1.data.apply_payload.data[i%64+7] = i%8;
                    ctrl_rsp_from_acc.write(pkt1);
                }
                pkt1.last = 1;
                ctrl_rsp_from_acc.write(pkt1);
            }
            

        }
        if(acc_context_save.size() == 3){
            DataMoverCmd cmd = acc_context_save.read();
            std::cout << "SAVE ADDR " << std::hex << cmd.saddr << "SIZE" << cmd.bbt << std::endl;
            cmd = acc_context_save.read();
            std::cout << "SAVE ADDR " << std::hex << cmd.saddr << "SIZE" << cmd.bbt << std::endl;
            cmd = acc_context_save.read();
            std::cout << "SAVE ADDR " << std::hex << cmd.saddr << "SIZE" << cmd.bbt << std::endl;
        }
         if(!acc_context_recovery.empty()){
            DataMoverCmd cmd = acc_context_recovery.read();
            std::cout << "RECOVERY ADDR " << std::hex << cmd.saddr << std::endl;
            AccContext context;
        
            context.privated_data.stream_buf_add_lists[0] = 0xB0090000;
            context.privated_data.stream_buf_add_lists[1] = 0xB0091000;
            context.privated_data.available_buf_num = 0;
            context.privated_data.descriptor_num = 123;
            context.static_data[0] = 1;
            context.static_data[1] = 2;
            context.static_data[2] = 3;
            ap_uint<512> send_data;
            Acc_Data_Pkt send_pkt;
            for(int i=0;i<34;i++){
                memcpy(&send_data,(ap_uint<512>*)(&context)+i,sizeof(ap_uint<512>));
                send_pkt.data = send_data;
                send_pkt.last = 0;
                send_pkt.keep = 0xffffffffffffffff;
                acc_context_data_recv_from_datamover.write(send_pkt);
            }
            memcpy(&send_data,(ap_uint<512>*)(&context)+34,sizeof(ap_uint<512>));
            send_pkt.data = send_data;
            send_pkt.last = 1;
            acc_context_data_recv_from_datamover.write(send_pkt);
        }
       if(acc_context_data_send_to_acc.size()>=35){
            Acc_Data_Pkt recv_pkt;
            AccContext context;
            ap_uint<512> recv_data;
            for(int i=0;i<35;i++){
               recv_pkt = acc_context_data_send_to_acc.read();
               std::cout << "CONTEXT TDEST" << recv_pkt.dest << std::endl;
               memcpy((ap_uint<512>*)(&context)+i,&(recv_pkt.data),sizeof(ap_uint<512>));
            }            
            assert(recv_pkt.last == 1);
            if(context.privated_data.stream_buf_add_lists[0] != 0xB0090000 || 
                context.privated_data.stream_buf_add_lists[1] != 0xB0091000 ||
                context.privated_data.descriptor_num != 123 ||
                context.static_data[0] != 1||
                context.static_data[1] != 2||
                context.static_data[2] != 3)
            {
                std::cerr << "CONTEXT DATA BROKEN!" << std::endl;
                exit(-1);
            }
        }
        if(cq_tailer==7&&acc_context_data_send_to_acc.empty()){
			std::cout << "CQ CID " << std::dec << cq[6].header.cid \
			<< "OPS NUM " << cq[6].header.ops_num
			 << "STATE " << cq[6].header.state << std::endl;
			//std::cout << "FIFTH TEST APPLY PASSED!" << std::endl;
			cq_header++;
			break;
        }
    }
    unsigned int last_count = 0;
    while(!acc_context_data_send_to_datamover.empty())
    {
        Acc_Data_Pkt pkt = acc_context_data_send_to_datamover.read();
        std::cout <<"DUMP_DATA1  " << std::hex << pkt.data(63,0) << std::endl;
        std::cout <<"DUMP_DATA2  " << std::hex << pkt.data(127,64) << std::endl;
        std::cout <<"DUMP_DATA3  " << std::hex << pkt.data(191,128) << std::endl;
        std::cout <<"DUMP_DATA4  " << std::hex << pkt.data(255,192) << std::endl;
        std::cout <<"DUMP_DATA5  " << std::hex << pkt.data(319,256) << std::endl;
        std::cout <<"DUMP_DATA6  " << std::hex << pkt.data(383,320) << std::endl;
        std::cout <<"DUMP_DATA7  " << std::hex << pkt.data(447,384) << std::endl;
        std::cout <<"DUMP_DATA8  " << std::hex << pkt.data(511,448) << std::endl;
        std::cout <<"AST " << std::hex << pkt.last << std::endl;
        if(pkt.last == 1)
            last_count++;
    }
    if(last_count == 3){
        std::cout << "SIXTH TEST APPLY PASSED!" << std::endl;
    }
    /**
     * 第七次测试，检查恢复信号测试
     */
    operator_done_signal.write(2);
    bool fin_flag = false;
    uint64_t try_times = 20;
    while(try_times){
        try_times--;
        AssScheduler(
            sq_header,
            sq_tailer,
            cq_header,
            cq_tailer,
            sq,
            cq,
            ctrl_req_to_acc,
            ctrl_rsp_from_acc,
            acc_context_save,
            acc_context_recovery,
            acc_context_data_send_to_acc,
            acc_context_data_send_to_datamover,
            acc_context_data_recv_from_datamover,
            operator_done_signal
        );
        if(cq_tailer==8){
            std::cout<<"CQ 7 CID" << (unsigned int)cq[7].header.cid << " OPS ID" << cq[7].header.ops_num<<std::endl;
            //std::cout<<"CQ 8 CID" << cq[8].header.cid << " OPS ID" << cq[8].header.ops_num<<std::endl;
            fin_flag = true;
            break;
        }
        assert(ctrl_req_to_acc.empty());
        assert(ctrl_rsp_from_acc.empty());
        assert(acc_context_save.empty());
        assert(acc_context_recovery.empty());
        assert(acc_context_data_send_to_acc.empty());
        assert(acc_context_data_recv_from_datamover.empty());
        assert(acc_context_data_send_to_datamover.empty());

    }
    if(fin_flag){
        std::cout << "SEVENTH TEST APPLY PASSED!" << std::endl;
    }
    return 0;
}
