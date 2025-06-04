#pragma once
#include "hlsacc_types.hpp"
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
);