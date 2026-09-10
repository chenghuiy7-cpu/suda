#include "selective_filter.hpp"

namespace {

bool is_suda_done_packet(const Acc_Data_Pkt &pkt)
{
#pragma HLS INLINE
    return pkt.user.range(TUSER_WIDTH - 1, TUSER_WIDTH - 4) != 0;
}

bool predicate_matches(
    ap_uint<8> quantity,
    ap_uint<8> threshold,
    ap_uint<32> predicate)
{
#pragma HLS INLINE
    if (predicate == SELECTIVE_FILTER_PREDICATE_GT) {
        return quantity > threshold;
    }
    return quantity == threshold;
}

void build_status_packet(
    Acc_Data_Pkt &finish_pkt,
    ap_uint<32> total_count,
    ap_uint<32> selected_count,
    ap_uint<32> error_code,
    ap_uint<32> record_bytes,
    ap_uint<32> quantity_offset,
    ap_uint<32> predicate,
    ap_uint<8> threshold)
{
#pragma HLS INLINE
    finish_pkt.data = 0;
    finish_pkt.data.range(63, 0) = SELECTIVE_FILTER_STATUS_MAGIC;
    finish_pkt.data.range(95, 64) = total_count;
    finish_pkt.data.range(127, 96) = selected_count;
    finish_pkt.data.range(159, 128) = error_code;
    finish_pkt.data.range(191, 160) = record_bytes;
    finish_pkt.data.range(223, 192) = quantity_offset;
    finish_pkt.data.range(255, 224) = predicate;
    finish_pkt.data.range(287, 256) = threshold;
    finish_pkt.keep = -1;
    finish_pkt.strb = -1;
    finish_pkt.user = 0xff;
    finish_pkt.last = 1;
}

void build_quantity_packet(
    Acc_Data_Pkt &output_pkt,
    const Acc_Data_Pkt &input_pkt,
    ap_uint<8> quantity)
{
#pragma HLS INLINE
    output_pkt.data = 0;
    output_pkt.data.range(7, 0) = quantity;
    output_pkt.keep = 1;
    output_pkt.strb = 1;
    output_pkt.user = 0;
    output_pkt.id = input_pkt.id;
    output_pkt.dest = input_pkt.dest;
    output_pkt.last = 0;
}

ap_uint<4> field_width(ap_uint<8> type)
{
#pragma HLS INLINE
    ap_uint<2> size_code = type.range(1, 0);
    return ap_uint<4>(1) << size_code;
}

bool field_is_signed(ap_uint<8> type)
{
#pragma HLS INLINE
    return type[2] != 0;
}

bool compare_value(
    ap_uint<64> value,
    ap_uint<64> literal,
    ap_uint<8> type,
    ap_uint<8> comparison)
{
#pragma HLS INLINE
    if (field_is_signed(type)) {
        ap_int<64> lhs = value;
        ap_int<64> rhs = literal;
        if (comparison == SELECTIVE_FILTER_CMP_EQ) return lhs == rhs;
        if (comparison == SELECTIVE_FILTER_CMP_NE) return lhs != rhs;
        if (comparison == SELECTIVE_FILTER_CMP_LT) return lhs < rhs;
        if (comparison == SELECTIVE_FILTER_CMP_LE) return lhs <= rhs;
        if (comparison == SELECTIVE_FILTER_CMP_GT) return lhs > rhs;
        return lhs >= rhs;
    }
    if (comparison == SELECTIVE_FILTER_CMP_EQ) return value == literal;
    if (comparison == SELECTIVE_FILTER_CMP_NE) return value != literal;
    if (comparison == SELECTIVE_FILTER_CMP_LT) return value < literal;
    if (comparison == SELECTIVE_FILTER_CMP_LE) return value <= literal;
    if (comparison == SELECTIVE_FILTER_CMP_GT) return value > literal;
    return value >= literal;
}

void capture_field(
    const Acc_Data_Pkt &pkt,
    ap_uint<7> beat_index,
    ap_uint<32> offset,
    ap_uint<8> type,
    ap_uint<64> &value,
    bool &valid)
{
#pragma HLS INLINE
    if ((offset >> 6) != beat_index) return;
    ap_uint<6> lane = offset.range(5, 0);
    ap_uint<4> width = field_width(type);
    ap_uint<512> shifted = pkt.data >> (lane * 8);
    ap_uint<64> raw = shifted.range(63, 0);
    ap_uint<64> mask = ~ap_uint<64>(0);
    if (width < 8) mask = (ap_uint<64>(1) << (width * 8)) - 1;
    value = raw & mask;
    valid = true;
valid_byte_loop:
    for (int byte_index = 0; byte_index < 8; ++byte_index) {
#pragma HLS UNROLL
        if (byte_index < width && !pkt.keep[lane + byte_index]) valid = false;
    }
    if (field_is_signed(type) && width < 8 && value[width * 8 - 1]) {
        value |= ~mask;
    }
}

bool evaluate_tokens(
    const ap_uint<8> tokens[SELECTIVE_FILTER_V2_MAX_TOKENS],
    ap_uint<5> token_count,
    const bool predicate_results[SELECTIVE_FILTER_V2_MAX_PREDICATES],
    bool &valid)
{
#pragma HLS INLINE off
    bool stack[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    ap_uint<4> depth = 0;
    valid = true;
token_loop:
    for (int token_index = 0; token_index < SELECTIVE_FILTER_V2_MAX_TOKENS; ++token_index) {
#pragma HLS PIPELINE II=1
        if (token_index >= token_count) continue;
        ap_uint<8> token = tokens[token_index];
        if (token <= SELECTIVE_FILTER_TOKEN_PREDICATE_MAX) {
            if (depth >= SELECTIVE_FILTER_V2_MAX_PREDICATES) valid = false;
            else stack[depth++] = predicate_results[token];
        } else if (token == SELECTIVE_FILTER_TOKEN_NOT) {
            if (depth < 1) valid = false;
            else stack[depth - 1] = !stack[depth - 1];
        } else if (token == SELECTIVE_FILTER_TOKEN_AND ||
                   token == SELECTIVE_FILTER_TOKEN_OR) {
            if (depth < 2) valid = false;
            else {
                bool rhs = stack[--depth];
                bool lhs = stack[depth - 1];
                stack[depth - 1] = token == SELECTIVE_FILTER_TOKEN_AND
                    ? lhs && rhs : lhs || rhs;
            }
        } else valid = false;
    }
    if (depth != 1) valid = false;
    return valid ? stack[0] : false;
}

void selective_filter_v2(
    Acc_Data &data_in,
    Acc_Data &data_out,
    ap_uint<512> context[256],
    ap_uint<512> cfg)
{
#pragma HLS INLINE off
    ap_uint<32> record_count = cfg.range(31, 0);
    ap_uint<32> record_bytes = cfg.range(63, 32);
    ap_uint<32> output_mode = cfg.range(191, 160);
    ap_uint<32> version = cfg.range(255, 224);
    ap_uint<5> predicate_count = cfg.range(287, 256);
    ap_uint<5> token_count = cfg.range(319, 288);
    ap_uint<32> projection_offset = cfg.range(351, 320);
    ap_uint<8> projection_type = cfg.range(359, 352);
    ap_uint<8> tokens[SELECTIVE_FILTER_V2_MAX_TOKENS];
    ap_uint<32> offsets[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    ap_uint<8> types[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    ap_uint<8> comparisons[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    ap_uint<64> literals[SELECTIVE_FILTER_V2_MAX_PREDICATES];

load_token_loop:
    for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_TOKENS; ++i) {
#pragma HLS PIPELINE II=1
        tokens[i] = cfg.range(391 + i * 8, 384 + i * 8);
    }
load_predicate_loop:
    for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_PREDICATES; ++i) {
#pragma HLS PIPELINE II=1
        ap_uint<512> descriptor_word =
            context[SELECTIVE_FILTER_STATIC_CONTEXT_BASE + 1 + (i >> 2)];
        ap_uint<512> shifted = descriptor_word >> ((i & 3) * 128);
        offsets[i] = shifted.range(31, 0);
        types[i] = shifted.range(39, 32);
        comparisons[i] = shifted.range(47, 40);
        literals[i] = shifted.range(127, 64);
    }

    ap_uint<32> config_error = 0;
    if (version != SELECTIVE_FILTER_V2_VERSION) config_error = 10;
    else if (record_bytes < SELECTIVE_FILTER_AXIS_BYTES ||
             record_bytes > SELECTIVE_FILTER_V2_MAX_RECORD_BYTES ||
             (record_bytes & (SELECTIVE_FILTER_AXIS_BYTES - 1)) != 0) config_error = 11;
    else if (output_mode != SELECTIVE_FILTER_OUTPUT_QUANTITY) config_error = 12;
    else if (predicate_count == 0 ||
             predicate_count > SELECTIVE_FILTER_V2_MAX_PREDICATES) config_error = 13;
    else if (token_count == 0 || token_count > SELECTIVE_FILTER_V2_MAX_TOKENS) config_error = 14;
    else if (projection_type > SELECTIVE_FILTER_TYPE_I64 ||
             projection_offset + field_width(projection_type) > record_bytes ||
             projection_offset.range(5, 0) + field_width(projection_type) > 64) config_error = 15;

validate_predicate_loop:
    for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_PREDICATES; ++i) {
#pragma HLS PIPELINE II=1
        if (i < predicate_count && config_error == 0 &&
            (types[i] > SELECTIVE_FILTER_TYPE_I64 ||
             comparisons[i] > SELECTIVE_FILTER_CMP_GE ||
             offsets[i] + field_width(types[i]) > record_bytes ||
             offsets[i].range(5, 0) + field_width(types[i]) > 64)) config_error = 16;
    }

    ap_uint<32> total_count = 0;
    ap_uint<32> selected_count = 0;
    ap_uint<7> beat_index = 0;
    ap_uint<7> record_beats = record_bytes >> 6;
    ap_uint<64> predicate_values[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    bool predicate_valid[SELECTIVE_FILTER_V2_MAX_PREDICATES];
    ap_uint<64> projection_value = 0;
    bool projection_valid = false;

v2_stream_loop:
    for (ap_uint<32> stream_index = 0; stream_index < 0xffffffff; ++stream_index) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=65536
        Acc_Data_Pkt input_pkt = data_in.read();
        if (is_suda_done_packet(input_pkt)) {
            ap_uint<32> error_code = config_error;
            if (error_code == 0 && beat_index != 0) error_code = 17;
            if (error_code == 0 && record_count != 0 && total_count != record_count) error_code = 18;
            Acc_Data_Pkt status = input_pkt;
            build_status_packet(status, total_count, selected_count, error_code,
                record_bytes, projection_offset, version, projection_type);
            data_out.write(status);
            return;
        }
        if (config_error != 0 ||
            (record_count != 0 && total_count >= record_count && beat_index == 0)) continue;
        if (beat_index == 0) {
            projection_value = 0;
            projection_valid = false;
        clear_values_loop:
            for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_PREDICATES; ++i) {
#pragma HLS PIPELINE II=1
                predicate_values[i] = 0;
                predicate_valid[i] = false;
            }
        }
    capture_predicate_loop:
        for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_PREDICATES; ++i) {
#pragma HLS PIPELINE II=1
            if (i < predicate_count)
                capture_field(input_pkt, beat_index, offsets[i], types[i],
                    predicate_values[i], predicate_valid[i]);
        }
        capture_field(input_pkt, beat_index, projection_offset, projection_type,
            projection_value, projection_valid);

        if (beat_index == record_beats - 1) {
            bool predicate_results[SELECTIVE_FILTER_V2_MAX_PREDICATES];
        compare_loop:
            for (int i = 0; i < SELECTIVE_FILTER_V2_MAX_PREDICATES; ++i) {
#pragma HLS PIPELINE II=1
                predicate_results[i] = i < predicate_count && predicate_valid[i] &&
                    compare_value(predicate_values[i], literals[i], types[i], comparisons[i]);
            }
            bool expression_valid = false;
            bool selected = evaluate_tokens(tokens, token_count, predicate_results, expression_valid);
            if (!expression_valid && config_error == 0) config_error = 19;
            ++total_count;
            if (selected && projection_valid) {
                Acc_Data_Pkt output_pkt;
                output_pkt.data = projection_value;
                ap_uint<4> width = field_width(projection_type);
                output_pkt.keep = (ap_uint<64>(1) << width) - 1;
                output_pkt.strb = output_pkt.keep;
                output_pkt.user = 0;
                output_pkt.id = input_pkt.id;
                output_pkt.dest = input_pkt.dest;
                output_pkt.last = 0;
                data_out.write(output_pkt);
                ++selected_count;
            }
            beat_index = 0;
        } else ++beat_index;
    }
}

} // namespace

static void selective_filter_legacy(
    Acc_Data &data_in,
    Acc_Data &data_out,
    ap_uint<512> context[256],
    ap_uint<512> cfg)
{
    ap_uint<32> record_count = cfg.range(31, 0);
    ap_uint<32> record_bytes = cfg.range(63, 32);
    ap_uint<32> quantity_offset = cfg.range(95, 64);
    ap_uint<32> predicate = cfg.range(127, 96);
    ap_uint<8> threshold = cfg.range(135, 128);
    ap_uint<32> output_mode = cfg.range(191, 160);

    ap_uint<32> total_count = 0;
    ap_uint<32> selected_count = 0;
    ap_uint<4> beat_index = 0;
    bool selected = false;

    ap_uint<32> config_error = 0;
    if (record_bytes != SELECTIVE_FILTER_RECORD_BYTES) {
        config_error = 1;
    } else if (quantity_offset != SELECTIVE_FILTER_QUANTITY_OFFSET) {
        config_error = 2;
    } else if (predicate > SELECTIVE_FILTER_PREDICATE_EQ) {
        config_error = 3;
    } else if (output_mode > SELECTIVE_FILTER_OUTPUT_QUANTITY) {
        config_error = 4;
    }

stream_loop:
    for (ap_uint<32> stream_index = 0; stream_index < 0xffffffff; stream_index++) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=8192
#pragma HLS PIPELINE II=1
        Acc_Data_Pkt input_pkt = data_in.read();
        Acc_Data_Pkt output_pkt;
        bool emit_output = false;
        bool finish_stream = false;

        if (is_suda_done_packet(input_pkt)) {
            ap_uint<32> error_code = config_error;
            if (error_code == 0 && beat_index != 0) {
                error_code = 5;
            }
            if (error_code == 0 && record_count != 0 &&
                total_count != record_count) {
                error_code = 6;
            }
            output_pkt = input_pkt;
            build_status_packet(
                output_pkt,
                total_count,
                selected_count,
                error_code,
                record_bytes,
                quantity_offset,
                predicate,
                threshold);
            emit_output = true;
            finish_stream = true;
        } else {
            bool ignore_input = config_error != 0 ||
                (record_count != 0 && total_count >= record_count && beat_index == 0);
            if (!ignore_input) {
                if (beat_index == 0) {
                    bool quantity_valid =
                        input_pkt.keep[SELECTIVE_FILTER_QUANTITY_OFFSET] != 0;
                    ap_uint<8> quantity = input_pkt.data.range(39, 32);

                    selected = quantity_valid &&
                        predicate_matches(quantity, threshold, predicate);
                    total_count++;
                    if (selected) {
                        selected_count++;
                        if (output_mode == SELECTIVE_FILTER_OUTPUT_QUANTITY) {
                            build_quantity_packet(output_pkt, input_pkt, quantity);
                            emit_output = true;
                        }
                    }
                }

                if (selected && output_mode == SELECTIVE_FILTER_OUTPUT_FULL_RECORD) {
                    output_pkt = input_pkt;
                    output_pkt.user = 0;
                    output_pkt.last = 0;
                    emit_output = true;
                }

                if (beat_index == SELECTIVE_FILTER_RECORD_BEATS - 1) {
                    beat_index = 0;
                    selected = false;
                } else {
                    beat_index++;
                }
            }
        }

        if (emit_output) {
            data_out.write(output_pkt);
        }
        if (finish_stream) {
            return;
        }
    }
}

void selective_filter(
    Acc_Data &data_in,
    Acc_Data &data_out,
    ap_uint<512> context[256])
{
#pragma HLS INTERFACE axis port = data_in
#pragma HLS INTERFACE axis port = data_out
#pragma HLS INTERFACE bram port = context

    ap_uint<512> cfg = context[SELECTIVE_FILTER_STATIC_CONTEXT_BASE];
    if (cfg.range(223, 192) == SELECTIVE_FILTER_V2_MAGIC) {
        selective_filter_v2(data_in, data_out, context, cfg);
    } else {
        selective_filter_legacy(data_in, data_out, context, cfg);
    }
}
