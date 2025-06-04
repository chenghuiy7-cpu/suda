#pragma once
#include "string.h"
#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "hlsacc_types.hpp"
#include "memory_access.hpp"

#define FEATURE_DIM (256)
#define MAX_FEATURE_WEIGHT (10)
#define MAX_FEATURE_WEIGHT_LOG2 (4)
#define RESULT_SIZE (64)
#define WEIGHT_SIZE (1)
#define POKE_WIDTH (32)
#define DATA_BUS_WIDTH 64

void KNNApplication(
	Acc_Data &data_in,
	Acc_Data &data_out,
	ap_uint<512> context[256],
	hls::stream<ap_uint<TDEST_WIDTH>> &operator_done_signal);