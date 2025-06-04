#include "KNNApplication.hpp"
#ifndef __SYNTHESIS__
#include "iostream"
#endif

#ifndef PLUS_NUM
#define PLUS_NUM 1
#endif

#pragma pack(1)
struct OperatorContext
{
	ap_uint<MAX_FEATURE_WEIGHT_LOG2> predicting_vec[FEATURE_DIM / 64][64];		   // 1024B
	unsigned int dist_arr[DATA_BUS_WIDTH / sizeof(unsigned int) / 2];			   // 32B
	unsigned int result_arr[DATA_BUS_WIDTH / sizeof(unsigned int) / 2];			   // 32B
	unsigned int dist[(RESULT_SIZE + WEIGHT_SIZE * FEATURE_DIM) / DATA_BUS_WIDTH]; // 20B
	unsigned char needed_size = 8;												   // 1B
};
#pragma pack()

/// @brief 一个简单的模块，将数据按照每64位加PLUS_NUM的方式进行处理
/// @param data_in
/// @param data_out
void KNNApplication(
	Acc_Data &data_in,
	Acc_Data &data_out,
	ap_uint<512> context[256],
	hls::stream<ap_uint<TDEST_WIDTH>> &operator_done_signal)
{
#pragma HLS INTERFACE mode = axis register_mode = off port = data_in
#pragma HLS INTERFACE axis register_mode = off port = data_out
#pragma HLS INTERFACE axis register_mode = both port = operator_done_signal
#pragma HLS INTERFACE mode = bram port = context storage_type = ram_1p

	Acc_Data_Pkt pkt;
	pkt.last = 0;
	OperatorContext *data = (OperatorContext *)((AccContext *)context)->static_data;
	unsigned int received_predicting_vec_idx = 0;
	unsigned int dist_arr_idx = 0;
	unsigned int cnt_input_buf = 0;
	unsigned int cur_result;
	unsigned int predicting_vec_dim0_idx = 0;
	data->needed_size--;
	pkt.last = 0;
	pkt.user = 0;
	while (!pkt.last)
		if (data_in.read_nb(pkt))
		{

			if(pkt.user(7,4)!=0){
				data_out.write(pkt);
				return;
			}
			if (cnt_input_buf == 0)
			{
				cur_result = (pkt.data(503, 496) - '0') * 10 + (pkt.data(511, 504) - '0');
			}
			else
			{
				unsigned char weights[DATA_BUS_WIDTH / WEIGHT_SIZE];
#pragma HLS array_partition variable = weights complete dim = 1
				for (int i = 0; i < DATA_BUS_WIDTH / WEIGHT_SIZE; i++)
				{
#pragma HLS unroll
					weights[i] = pkt.data(i * 8 + 7, i * 8) - '0';
				}

				unsigned int tmp_dist = 0;
				for (int i = 0; i < 64; i++)
				{
#pragma HLS unroll
					char diff = (char)data->predicting_vec[predicting_vec_dim0_idx][i] -
								(char)weights[i];
					unsigned char mul = diff * diff;
					tmp_dist += mul;
				}
				data->dist[cnt_input_buf] = tmp_dist;
				predicting_vec_dim0_idx++;
			}
			cnt_input_buf++;
			if (cnt_input_buf ==
				(RESULT_SIZE + WEIGHT_SIZE * FEATURE_DIM) / DATA_BUS_WIDTH)
			{
				unsigned int sum_dist = 0;
				for (int i = 0;
					 i < (RESULT_SIZE + WEIGHT_SIZE * FEATURE_DIM) / DATA_BUS_WIDTH;
					 i++)
				{
#pragma HLS pipeline
					sum_dist += data->dist[i];
				}
				data->dist_arr[dist_arr_idx] = sum_dist;
				data->result_arr[dist_arr_idx] = cur_result;
				dist_arr_idx++;
				cnt_input_buf = 0;
				predicting_vec_dim0_idx = 0;
				if (dist_arr_idx == DATA_BUS_WIDTH / sizeof(unsigned int) / 2)
				{
					dist_arr_idx = 0;
					Acc_Data_Pkt output_app_data;
					output_app_data.last = 0;
					for (int idx = 0; idx < 512; idx += 64)
					{
						output_app_data.data(idx + 31, idx) = data->dist_arr[idx / 64];
						output_app_data.data(idx + 63, idx + 32) = data->result_arr[idx / 64];
					}
					data_out.write(output_app_data);
				}
			}
			// Donnot use it
			if (data->needed_size == 99999999)
			{
				operator_done_signal.write(1);
			}
		}
}
