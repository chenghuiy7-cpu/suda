#pragma once

#ifndef NO_CTOR
#define NO_CTOR
#endif

#include "hlsacc_types.hpp"
#include "selective_filter_protocol.h"

#define SELECTIVE_FILTER_STATIC_CONTEXT_BASE 3
#define SELECTIVE_FILTER_RECORD_BYTES 512
#define SELECTIVE_FILTER_QUANTITY_OFFSET 4
#define SELECTIVE_FILTER_AXIS_BYTES (TDATA_WIDTH / 8)
#define SELECTIVE_FILTER_RECORD_BEATS \
    (SELECTIVE_FILTER_RECORD_BYTES / SELECTIVE_FILTER_AXIS_BYTES)

#define SELECTIVE_FILTER_PREDICATE_GT 0
#define SELECTIVE_FILTER_PREDICATE_EQ 1

#define SELECTIVE_FILTER_OUTPUT_FULL_RECORD 0
#define SELECTIVE_FILTER_OUTPUT_QUANTITY 1

#define SELECTIVE_FILTER_STATUS_MAGIC 0x53464c5453544154ULL

// context[SELECTIVE_FILTER_STATIC_CONTEXT_BASE] layout:
// [ 31:  0] record_count; 0 means consume records until the SUDA finish packet
// [ 63: 32] record_bytes; first prototype requires exactly 512
// [ 95: 64] quantity_offset; first prototype requires exactly 4
// [127: 96] predicate; 0 = quantity > threshold, 1 = quantity == threshold
// [159:128] threshold; low eight bits are used
// [191:160] output_mode; 0 = selected full records, 1 = selected quantity bytes
//
// A record consists of eight consecutive 512-bit AXI-stream beats. The
// predicate field is kept in the first beat so the operator can decide before
// streaming or dropping the remaining seven beats, without buffering a record.
//
// The final SUDA TUSER=0xff packet is forwarded with a status payload:
// [ 63:  0] SELECTIVE_FILTER_STATUS_MAGIC
// [ 95: 64] total_count
// [127: 96] selected_count
// [159:128] error_code; zero means success
// [191:160] record_bytes
// [223:192] quantity_offset
// [255:224] predicate
// [287:256] threshold
void selective_filter(
    Acc_Data &data_in,
    Acc_Data &data_out,
    ap_uint<512> context[256]);
