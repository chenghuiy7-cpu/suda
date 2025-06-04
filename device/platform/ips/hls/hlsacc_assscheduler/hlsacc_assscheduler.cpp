
#include "hlsacc_assscheduler.hpp"
#include "string.h"

struct CQE_Pkt{
	AssSchedRet ret;
	bool last;
};

static void CtrlReqSender(
	ap_uint<SQCQ_PTR_WIDTH> &sq_header,
	ap_uint<SQCQ_PTR_WIDTH> &sq_tailer,
	ap_uint<SQCQ_PTR_WIDTH> &cq_header,
	ap_uint<SQCQ_PTR_WIDTH> &cq_tailer,
	AssSchedCmd sq[32],
	AssSchedRet cq[32],
	hls::stream<CQE_Pkt> &cqe_in,
	hls::stream<AssSchedCmdOpcode> &opc_out,
	hls::stream<ap_uint<4>> &outstanding_num_out,
	CtrlReq &ctrl_req_to_acc,
	hls::stream<ap_uint<TDEST_WIDTH>> &operator_done_signal
){
#pragma HLS inline off
	static enum {RECV_SQE,SEND_CQE_HEADER,SEND_CQE_PAYLOAD} fsm_state = RECV_SQE;
	static ap_uint<SQCQ_PTR_WIDTH> inner_sq_header = 0;
	static ap_uint<SQCQ_PTR_WIDTH> inner_cq_tailer = 0;

	Ctrlreq_Pkt req;
	Ctrlrsp_Pkt rsp;
	CQE_Pkt pkt;
	static AssSchedCmd tempsqe;
	AssSchedCmd tempsqe_payload_buffer;
	/*Only Used for Apply-Start */
	static ap_uint<4> apply_op_lists[8];
	/*Only Used for Apply-End */

	req.keep = 0xffff;
	req.strb = 0xffff;

	/*当sq有新的元素且cq没有满的情况下，执行下列逻辑*/
	ap_uint<8> temp_sq_tailer = sq_tailer;
	if(!operator_done_signal.empty()){
		ap_uint<4> opnum = operator_done_signal.read();
		pkt.ret.header.cid = 255;//分配一个cid永远不会用到
		pkt.ret.header.ops_num = opnum;
		cq[inner_cq_tailer++] = pkt.ret;
		cq_tailer = inner_cq_tailer;
		return;
	}
	switch (fsm_state)
	{
		case RECV_SQE:
			if ((inner_sq_header) != temp_sq_tailer && ((inner_cq_tailer + 1) != cq_header))
			{
					unsigned char i = inner_sq_header;
					/*当前的设计要求
					必须将SQE整个传完（包括payload和header）
					才能更新指针，否则会出现问题！
					*/
					tempsqe = sq[i++];
					unsigned char index = 0;
					unsigned char connection_num = 0;
					unsigned char outstanding_cmd_num = 0;

					switch (tempsqe.header.opc)
					{
						case FORCE_FREE_OPS:
						case SUSPEND_OPS:
						case RESUME_OPS:
						case QUERY_OPS:
							req.last = 1;
							req.data.header.type = tempsqe.header.opc == FORCE_FREE_OPS? FORCE_FREE:
												tempsqe.header.opc == SUSPEND_OPS? SUSPEND:
												tempsqe.header.opc == QUERY_OPS? QUERY:
												RESUME;
							outstanding_cmd_num = tempsqe.header.ops_num;
							req.data.header.payload_length = 0;
							// 读取Ops list
							tempsqe_payload_buffer = sq[i++];
							opc_out.write(tempsqe.header.opc);
							outstanding_num_out.write(outstanding_cmd_num);
							do{
								req.dest = ((tempsqe_payload_buffer.generic_ops_payload).op_lists)[index];
								ctrl_req_to_acc.write(req);
								index++;
							}while(tempsqe.header.ops_num != index);
							break;
						case APPLY_OPS:
							outstanding_cmd_num = tempsqe.header.ops_num;
							opc_out.write(tempsqe.header.opc);
							outstanding_num_out.write(outstanding_cmd_num);
							for(ap_uint<4> j=0;j<ap_uint<4>(tempsqe.header.ops_num);j++)
							{
							#pragma HLS unroll off
							#pragma HLS PIPELINE II=2
								req.data.header.type = APPLY;
								req.data.header.payload_length = 0;
								req.last = 0;
								/**
								 * 根据cmd_length读取命令的负载部分
								 * 首先读取上下文地址
								 */
							
								tempsqe_payload_buffer = sq[i++];
								req.data.header.context_address = tempsqe_payload_buffer. \
																	apply_ops_payload1.	  \
																	context_address;
								
								req.last = 0;
								/**
								 * 然后读取算子的连接关系
								 * 发送控制器请求首部
								 */
								
								tempsqe_payload_buffer = sq[i++];
								req.data.header.payload_length = ap_uint<16>(tempsqe_payload_buffer. \
																	apply_ops_payload2.\
																	connections_num);
								bool need_payload3 =  ap_uint<16>(tempsqe_payload_buffer. \
																	apply_ops_payload2.\
																	connections_num)>3; 
								
								req.dest = ap_uint<8>((tempsqe_payload_buffer.apply_ops_payload2.connections)[0].from)(7,4);
								apply_op_lists[j] = req.dest;
								//i += 2;
								ctrl_req_to_acc.write(req);
								/**
								 * 发送携带算子连接关系的
								 * 控制器请求负载
								 */		
							
								for(int k=0;k<3;k++){
									req.data.apply_payload.connections[k].from = (tempsqe_payload_buffer.\
																				apply_ops_payload2.
																				connections)[k].from;
									req.data.apply_payload.connections[k].to = (tempsqe_payload_buffer.\
																				apply_ops_payload2.
																				connections)[k].to;
								}
								if(need_payload3)
								tempsqe_payload_buffer = sq[i++];
								for(int k=3;k<7;k++){
									req.data.apply_payload.connections[k].from = (tempsqe_payload_buffer.\
																				apply_ops_payload2.
																				connections)[k].from;
									req.data.apply_payload.connections[k].to = (tempsqe_payload_buffer.\
																				apply_ops_payload2.
																				connections)[k].to;
								}
								req.last = 1;
								outstanding_cmd_num++;
								ctrl_req_to_acc.write(req);
							}					
							break;	
				}
				inner_sq_header = i;
				sq_header = inner_sq_header;
				fsm_state = SEND_CQE_HEADER;

			} 
			break;
		case SEND_CQE_HEADER:
			if(!cqe_in.empty()){
				pkt = cqe_in.read();
				if(tempsqe.header.opc == APPLY_OPS && pkt.ret.header.state == SUCCESS_STAGE1)
				{
					//如果是收到APPLY，那就无需更新CQ
					req.data.header.type = APPLY_STAGE2;
					req.data.header.payload_length = 0;
					req.last = 1;			
					opc_out.write(tempsqe.header.opc);

					outstanding_num_out.write(tempsqe.header.ops_num);

					for(int k=0;k<tempsqe.header.ops_num;k++)
					{
						req.dest = apply_op_lists[k];
						ctrl_req_to_acc.write(req);
					}

					
					break;
				}
				pkt.ret.header.cid = tempsqe.header.cid;
				cq[inner_cq_tailer++] = pkt.ret;
				fsm_state =	pkt.last?RECV_SQE:SEND_CQE_PAYLOAD;
				if(pkt.last) cq_tailer = inner_cq_tailer;
			}
			break;
		case SEND_CQE_PAYLOAD:
			if(!cqe_in.empty()){
				cq[inner_cq_tailer++] = pkt.ret;
				fsm_state =	pkt.last?RECV_SQE:SEND_CQE_HEADER;
			}
			if(pkt.last) cq_tailer = inner_cq_tailer;
			break;
		}	
}

static void CtrlRspReceiver(
	CtrlRsp &ctrl_rsp_from_acc_in,
	CtrlRsp &ctrl_rsp_from_acc_out,
	hls::stream<ap_uint<TDEST_WIDTH>> &operator_done_signal
){
#pragma hls pipeline II=1
	static enum{
		RECV_RSP_HEADER,
		RECV_RSP_PAYLOAD
	} fsm_state = RECV_RSP_HEADER;
	if(!ctrl_rsp_from_acc_in.empty()){
		Ctrlrsp_Pkt pkt = ctrl_rsp_from_acc_in.read();
		switch(fsm_state){
			case RECV_RSP_HEADER:
				if(pkt.data.header.state==OPERATOR_REAL_FIN){
					operator_done_signal.write(pkt.id);
				}else{
					ctrl_rsp_from_acc_out.write(pkt);
					if(!pkt.last)
						fsm_state = RECV_RSP_PAYLOAD;
				}
				break;
			case RECV_RSP_PAYLOAD:
				if(pkt.last)
					fsm_state = RECV_RSP_HEADER;
			default:
				break;
		}
	}

}

static void CtrlRspHandler(
	hls::stream<CQE_Pkt> &cqe_out, 
	CtrlRsp &ctrl_rsp_from_acc,
	hls::stream<AssSchedCmdOpcode>& opc_in,
	hls::stream<ap_uint<4>> &outstanding_num_in,
	Context_Cmd &acc_context_save,
	Context_Cmd &acc_context_recovery,
	hls::stream<ap_uint<TDATA_WIDTH>> &acc_context_add_list, 
	Acc_Data &acc_context_data_out,
	hls::stream<ap_uint<TDEST_WIDTH>> &tdest
){
#pragma HLS interface ap_ctrl_none port=return
#pragma HLS inline off
#pragma HLS PIPELINE II=1
	static enum {WAIT_OPC,RECV_RSP_HEADER,\
				RECV_RSP_QUERY_PAYLOAD,\
				RECV_RSP_APPLY_PAYLOAD0,\
				RECV_RSP_APPLY_PAYLOAD1,\
				RECV_RSP_APPLY_PAYLOAD2,\
				RECV_RSP_APPLY_PAYLOAD3,\
				RECV_RSP_APPLY_PAYLOAD4,\
				SEND_CQE_HEADER,\
				SEND_CQE_PAYLOAD} fsm_state = WAIT_OPC;
	static AssSchedRet tempcqe;
	static AssSchedRet tempcqe_payload_buffer[4];
	static AssSchedCmdOpcode opc;
	static ap_uint<4> outstanding_num;
	static ap_uint<1> query_ops_index = 0;
	static ap_uint<2> query_buffer_num = 0;
	static ap_uint<2> query_buffer_index = 0;

	//Only Used for Apply-Start
	static unsigned long new_bbt;
	static int old_bbt = 0;
	static int new_static_var_size;
	static unsigned long long old_context_address;
	static unsigned long long new_context_address;
	static ap_uint<16> context_counter = 0;//
	static bool need_save = false;
	static bool has_recv_add_lists = false;
	//Only Used for Apply-End

	CQE_Pkt pkt;
	switch (fsm_state)
	{
	case WAIT_OPC:
		if(!opc_in.empty() && !outstanding_num_in.empty())
		{
			opc = opc_in.read();
			outstanding_num = outstanding_num_in.read();
			fsm_state = RECV_RSP_HEADER;
			tempcqe.header.state = SUCCESS;
			tempcqe.header.ops_num = 0;
		}
		break;
	case RECV_RSP_HEADER:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt rsp = ctrl_rsp_from_acc.read();
			if(rsp.data.header.state == SUCCESS_STAGE1)
				tempcqe.header.state = SUCCESS_STAGE1;
			else if(rsp.data.header.state == FAILED)
				tempcqe.header.state = FAILED;
			outstanding_num--;
			/**
			 * 对于APPLY命令，如果是第一次握手，那不携带除状态以外的信息
			 */
			if(opc == APPLY_OPS && rsp.data.header.state == SUCCESS){
				new_bbt = rsp.data.header.meta.apply_meta.new_bbt;
				old_bbt = rsp.data.header.meta.apply_meta.old_bbt;
				need_save = (bool)(rsp.data.header.meta.apply_meta.need_save);
				old_context_address = rsp.data.header.meta.apply_meta.old_context_address;
				new_context_address = rsp.data.header.meta.apply_meta.new_context_address;
				DataMoverCmd mem_req;

				mem_req.type = 1;
				mem_req.dsa = 0;
				mem_req.eof = 1;
				mem_req.drr = 0;
				mem_req.tag = 0;
				if(old_bbt != 0 && need_save){
					//如果算子正在被占用，那就需要保存上下文
					mem_req.bbt = rsp.data.header.meta.apply_meta.old_static_var_size+64;
					mem_req.saddr = ap_uint<64>(old_context_address)(39,0) | 128;
					acc_context_save.write(mem_req);
					context_counter = (int)rsp.data.header.meta.apply_meta.old_static_var_size/(TDATA_WIDTH/8);
				}
				else
					context_counter = 0;
				//如果需要从新的数据中恢复，那就发送恢复地址信息给Datamover
				if(new_bbt != 0 && !need_save){		
					mem_req.bbt = rsp.data.header.meta.apply_meta.new_static_var_size+sizeof(AccContext::privated_data);			
					mem_req.saddr = ap_uint<64>(new_context_address)(39,0);
					acc_context_recovery.write(mem_req);
					tdest.write(rsp.id);
				}
				new_static_var_size = rsp.data.header.meta.apply_meta.new_static_var_size+sizeof(AccContext::privated_data);
			} 
			fsm_state = (opc==QUERY_OPS)? RECV_RSP_QUERY_PAYLOAD : \
						(opc==APPLY_OPS&&old_bbt!=0&&need_save&&rsp.data.header.state == SUCCESS)? RECV_RSP_APPLY_PAYLOAD0 : \
						(opc==APPLY_OPS&&rsp.data.header.state == SUCCESS&&new_bbt!=0)? RECV_RSP_APPLY_PAYLOAD2: \
						(outstanding_num==0)? SEND_CQE_HEADER : \
						 RECV_RSP_HEADER;

		}	
		break;
	case RECV_RSP_APPLY_PAYLOAD0:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt rsp = ctrl_rsp_from_acc.read();
			//context_counter--;		
			//old_bbt -= TDATA_WIDTH/8;
			//AccContext的最后一个部分刚好是64B的fifo address list
			//注意，这里可能存在一个BUG
			//当数据位宽不等于512时，会出现问题！！！
			DataMoverCmd mem_req;
			mem_req.bbt = PAGE_SIZE;
			mem_req.type = 1;
			mem_req.dsa = 0;
			mem_req.eof = 1;
			mem_req.drr = 0;
			mem_req.tag = 0;
			unsigned long long available_buf_num = rsp.data.apply_payload.data[0];
			Acc_Data_Pkt context_pkt;
			context_pkt.last = 0;
			context_pkt.keep = 0xffffffffffffffff;
			memcpy(&context_pkt.data,&rsp.data,sizeof(context_pkt.data));
			acc_context_data_out.write(context_pkt);
			for(int i=0;i<available_buf_num;i++){
				mem_req.saddr = ap_uint<64>(rsp.data.apply_payload.data[i+1])(39,0);
				acc_context_save.write(mem_req);
			}
			mem_req.saddr = ap_uint<64>(old_context_address)(39,0);
			mem_req.bbt = 64;
			acc_context_save.write(mem_req);
			fsm_state = RECV_RSP_APPLY_PAYLOAD1;	
		}
		break;
	case RECV_RSP_APPLY_PAYLOAD1:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt rsp = ctrl_rsp_from_acc.read();
			old_bbt -= TDATA_WIDTH/8;
			context_counter--;
			Acc_Data_Pkt context_pkt;
			context_pkt.last = 0;
			context_pkt.keep = 0xffffffffffffffff;
			if(context_counter == 0)
			{
				context_pkt.last = 1;
				DataMoverCmd mem_req;
				mem_req.bbt = new_static_var_size;
				mem_req.type = 1;
				mem_req.dsa = 0;
				mem_req.eof = 1;
				mem_req.drr = 0;
				mem_req.tag = 0;
				if(new_bbt != 0){
					//mem_req.bbt = sizeof(AccContext);
					mem_req.saddr = ap_uint<64>(new_context_address)(39,0);
					acc_context_recovery.write(mem_req);
					tdest.write(rsp.id);
				}
				fsm_state = (old_bbt != 0)? RECV_RSP_APPLY_PAYLOAD3:\
									(outstanding_num==0)? SEND_CQE_HEADER : \
									RECV_RSP_HEADER;
			}
			memcpy(&context_pkt.data,&rsp.data,sizeof(context_pkt.data));
			acc_context_data_out.write(context_pkt);

		}
		break;
	case RECV_RSP_APPLY_PAYLOAD2:
		context_counter = 0;
		if(!acc_context_add_list.empty()){
			ap_uint<512> addlists_raw = acc_context_add_list.read();
			unsigned long long addlists[8];
			memcpy(addlists,&addlists_raw,sizeof(unsigned long long)*8);
			for(int i=1;i<=addlists[0];i++){
				DataMoverCmd mem_req;
				mem_req.bbt = PAGE_SIZE;
				mem_req.type = 1;
				mem_req.dsa = 0;
				mem_req.eof = 1;
				mem_req.drr = 0;
				mem_req.tag = 0;
				mem_req.saddr = ap_uint<64>(addlists[i])(39,0);
				acc_context_recovery.write(mem_req);
			} 
			fsm_state = (outstanding_num==0)? SEND_CQE_HEADER : RECV_RSP_HEADER;
		}
		break;
	case RECV_RSP_APPLY_PAYLOAD3:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt rsp = ctrl_rsp_from_acc.read();
			old_bbt -= TDATA_WIDTH/8;
			context_counter++;
			Acc_Data_Pkt context_pkt;
			context_pkt.keep = 0xffffffffffffffff;
			context_pkt.last = 0;
			if(context_counter == (PAGE_SIZE/(TDATA_WIDTH/8))|| old_bbt==64 )
			{
				context_counter = 0;
				context_pkt.last = 1;
			}
			memcpy(&context_pkt.data,&rsp.data,sizeof(context_pkt.data));
			acc_context_data_out.write(context_pkt);
		}
		fsm_state = (old_bbt==64)?RECV_RSP_APPLY_PAYLOAD4:RECV_RSP_APPLY_PAYLOAD3;
		break;
	case RECV_RSP_APPLY_PAYLOAD4:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt rsp = ctrl_rsp_from_acc.read();
			Acc_Data_Pkt context_pkt;
			memcpy(&context_pkt.data,&rsp.data,sizeof(context_pkt.data));
			context_pkt.keep = 0xffffffffffffffff;
			context_pkt.last = 1;
			fsm_state = ((new_bbt==0)? ((outstanding_num==0)? SEND_CQE_HEADER : RECV_RSP_HEADER)
						: RECV_RSP_APPLY_PAYLOAD2);
			acc_context_data_out.write(context_pkt);
			context_counter = 0;
		}
		break;
	case RECV_RSP_QUERY_PAYLOAD:
		if(!ctrl_rsp_from_acc.empty()){
			Ctrlrsp_Pkt pkt = ctrl_rsp_from_acc.read();
			tempcqe_payload_buffer[query_buffer_num].query_payload.state_pair[query_ops_index].op = pkt.id;
			tempcqe_payload_buffer[query_buffer_num].query_payload.state_pair[query_ops_index].fsm_state = pkt.data.header.\
																											meta.query_meta.\
																											fsm_state;
			query_ops_index = !query_ops_index;
			if(query_ops_index==0)
				query_buffer_num++;
			fsm_state = (outstanding_num==0)? SEND_CQE_HEADER : RECV_RSP_HEADER;
		}
		break;
	case SEND_CQE_HEADER:
		pkt.last = opc != QUERY_OPS;
		pkt.ret = tempcqe;
		cqe_out.write(pkt);
		fsm_state = (opc==QUERY_OPS)? SEND_CQE_PAYLOAD:WAIT_OPC;
		break;
	case SEND_CQE_PAYLOAD:
		pkt.last = (query_buffer_index==query_buffer_num);
		pkt.ret = tempcqe_payload_buffer[query_buffer_index++];
		cqe_out.write(pkt);
		fsm_state = (pkt.last)? SEND_CQE_PAYLOAD:WAIT_OPC;
	default:
		break;
	}
}


static void ContextReceiver(
	Acc_Data &acc_context_data_in,
	Acc_Data &acc_context_data_out,
	hls::stream<ap_uint<TDATA_WIDTH>> &context_header_out,
	hls::stream<ap_uint<TDEST_WIDTH>> &tdest

){
	#pragma HLS interface ap_ctrl_none port=return
	#pragma HLS inline off
	#pragma HLS pipeline II=1
	static ap_uint<TDEST_WIDTH> dest;
	static unsigned long long available_buf_num;
	static enum{WAIT_META0,WAIT_META1,WAIT_META2,WAIT_META3,WAIT_PAYLOAD} fsm_state = WAIT_META0;
	Acc_Data_Pkt context_pkt;

	switch (fsm_state)
	{
	case WAIT_META0:
		if(!acc_context_data_in.empty()&&!tdest.empty())
		{
			context_pkt = acc_context_data_in.read();
			dest = tdest.read();
			context_pkt.dest = dest;
			acc_context_data_out.write(context_pkt);
			fsm_state = WAIT_META1;
		}
		break;
	case WAIT_META1:
		if(!acc_context_data_in.empty())
		{
			context_pkt = acc_context_data_in.read();				
			context_pkt.dest = dest;		
			acc_context_data_out.write(context_pkt);	
			//fsm_state = available_buf_num==0? WAIT_META0:WAIT_PAYLOAD;		
			fsm_state = WAIT_META2;
		}
		break;
	case WAIT_META2:
		if(!acc_context_data_in.empty())
		{
			context_pkt = acc_context_data_in.read();		
			available_buf_num = context_pkt.data(63,0);			
			context_pkt.dest = dest;		
			acc_context_data_out.write(context_pkt);
			context_header_out.write(context_pkt.data);
			fsm_state = WAIT_META3;
		}
		break;
	case WAIT_META3:
		if(!acc_context_data_in.empty())
		{
			context_pkt = acc_context_data_in.read();
			context_pkt.dest = dest;
			bool islast = context_pkt.last;
			context_pkt.last = available_buf_num == 0 && context_pkt.last;
			acc_context_data_out.write(context_pkt);	
			fsm_state = islast?(available_buf_num==0? WAIT_META0:WAIT_PAYLOAD):
			WAIT_META3;
		}
		break;
	case WAIT_PAYLOAD:
		if(!acc_context_data_in.empty())
		{
			context_pkt = acc_context_data_in.read();
			context_pkt.dest = dest;
			if(context_pkt.last)
			{
				available_buf_num --;
				if(available_buf_num == 0)
				{
					fsm_state = WAIT_META0;
					acc_context_data_out.write(context_pkt);
					break;
				}
			}
			context_pkt.last = 0;
			acc_context_data_out.write(context_pkt);
		}
	default:
		break;
	}
	

}

//当前的算子支持最多7个外部端口
//一个计算核可包含最多8个算子
void AssScheduler(
	ap_uint<SQCQ_PTR_WIDTH> &sq_header,
	ap_uint<SQCQ_PTR_WIDTH> &sq_tailer,
	ap_uint<SQCQ_PTR_WIDTH> &cq_header,
	ap_uint<SQCQ_PTR_WIDTH> &cq_tailer,
	AssSchedCmd sq[32],
	AssSchedRet cq[32],
	CtrlReq &ctrl_req_to_acc,
	CtrlRsp &ctrl_rsp_from_acc,
	Context_Cmd &acc_context_save,
	Context_Cmd &acc_context_recovery,
	Acc_Data &acc_context_data_send_to_acc,
	Acc_Data &acc_context_data_send_to_datamover,
	Acc_Data &acc_context_data_recv_from_datamover,
	hls::stream<ap_uint<TDEST_WIDTH>> &operator_done_signal
)
{
#pragma HLS INTERFACE axis register_mode = both port = ctrl_req_to_acc
#pragma HLS INTERFACE axis register_mode = both port = operator_done_signal
#pragma HLS INTERFACE axis register_mode = both port = ctrl_rsp_from_acc
#pragma HLS INTERFACE axis register_mode = both port = acc_context_save
#pragma HLS INTERFACE axis register_mode = both port = acc_context_recovery
#pragma HLS INTERFACE axis register_mode = both port = acc_context_data_send_to_acc
#pragma HLS INTERFACE axis register_mode = both port = acc_context_data_send_to_datamover
#pragma HLS INTERFACE axis register_mode = both port = acc_context_data_recv_from_datamover
#pragma HLS INTERFACE s_axilite bundle = Manager port = sq_header
#pragma HLS INTERFACE ap_none port = sq_header
#pragma HLS INTERFACE s_axilite bundle = Manager port = sq_tailer
#pragma HLS INTERFACE s_axilite bundle = Manager port = cq_header
#pragma HLS INTERFACE s_axilite bundle = Manager port = cq_tailer
#pragma HLS INTERFACE ap_none register port = cq_tailer
#pragma HLS INTERFACE ap_memory storage_type=ram_1p port=sq
#pragma HLS INTERFACE ap_memory storage_type=ram_1p port=cq
#pragma HLS aggregate variable = acc_context_save compact = bit
#pragma HLS aggregate variable = acc_context_recovery compact = bit
	static hls::stream<CQE_Pkt,3> cqe_fifo("CQE_FIFO");
	static hls::stream<AssSchedCmdOpcode,4> opc_fifo("OPC_FIFO");
	static hls::stream<ap_uint<4>,4> outstanding_num_fifo("OUTSTAND_FIFO");
	static hls::stream<ap_uint<TDEST_WIDTH>,4> tdest("CONTEXT_TDEST");
	static hls::stream<ap_uint<512>,1> context_add_lists("CONTEXT_HEADER");
	CtrlReqSender(sq_header,sq_tailer,cq_header,cq_tailer,sq,cq,cqe_fifo,opc_fifo,outstanding_num_fifo,ctrl_req_to_acc,operator_done_signal);
	//CtrlRspReceiver(ctrl_rsp_from_acc,ctrl_rsp_from_acc_out,operator_done_signal);
	CtrlRspHandler(cqe_fifo,ctrl_rsp_from_acc,opc_fifo,outstanding_num_fifo,acc_context_save,acc_context_recovery,context_add_lists,acc_context_data_send_to_datamover,tdest);
	ContextReceiver(acc_context_data_recv_from_datamover,acc_context_data_send_to_acc,context_add_lists,tdest);
}
